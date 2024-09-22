// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
    #include <spdlog/details/thread_pool.h>
#endif

#include <cassert>
#include <spdlog/common.h>
#include <pthread.h>
#include <spdlog/details/cpuset.h>

namespace utils {
SPDLOG_INLINE void pthread_setname_np(pthread_t tid, const char *name) {
#ifdef __APPLE__
    pthread_setname_np(name);
#elif defined(__linux__)
    pthread_setname_np(tid, name);
#endif
}

static int do_taskset(size_t setsize, cpu_set_t *set, pthread_t *pid_list, size_t pid_num)
{
    for (size_t i = 0; i < pid_num; ++i) {
        if (pthread_setaffinity_np(pid_list[i], setsize, set) < 0) {
            return -1;
        }
    }
    return 0;
}

SPDLOG_INLINE int set_cpu_affinity(pthread_t *tids, size_t num_threads) {
#ifdef __linux__
    int ncpus;
    const char *cpulist_env = getenv("LogCPUSet");
    if (cpulist_env == NULL) {
        return -1;
    }
    ncpus = get_max_number_of_cpus();
    if (ncpus <= 0) {
        fprintf(stderr, "Failed to get cpu nums\n");
        return -1;
    }
    size_t setsize;
    size_t nbits;
    cpu_set_t *set = cpuset_alloc(ncpus, &setsize, &nbits);
    if (set == NULL) {
        fprintf(stderr, "Failed to alloc cpuset\n");
        return -1;
    }
    if (cpulist_parse(cpulist_env, set, setsize, 0)) {
        fprintf(stderr, "Failed to parse cpu list\n");
        return -1;
    }
    return do_taskset(setsize, set, tids, num_threads);
#else
    return 0;
#endif
}
}  // namespace utils

namespace spdlog {
namespace details {

#define _TP_TASKNUM 3278

SPDLOG_INLINE thread_pool::thread_pool(size_t q_max_items,
                                       size_t threads_n,
                                       std::function<void()> on_thread_start,
                                       std::function<void()> on_thread_stop)
    : q_(q_max_items) {
    int worker_num = 0;
    pthread_t process_id_list[_TP_TASKNUM];
    std::string logger_prefix = "spdlog.worker";
    if (threads_n == 0 || threads_n > 1000) {
        throw_spdlog_ex(
            "spdlog::thread_pool(): invalid threads_n param (valid "
            "range is 1-1000)");
    }
    for (size_t i = 0; i < threads_n; i++) {
        threads_.emplace_back([this, on_thread_start, on_thread_stop] {
            on_thread_start();
            this->thread_pool::worker_loop_();
            on_thread_stop();
        });
        auto pid = threads_.back().native_handle();
        auto logger_name = logger_prefix + std::to_string(i);
        process_id_list[worker_num++] = pid;
        int ret = pthread_setname_np(pid, logger_name.c_str());
        if (ret != 0) {
            fprintf(stderr, "Failed to set thread name for %s\n", logger_name.c_str());
        }
    }

    ::utils::set_cpu_affinity(process_id_list, worker_num);
}

SPDLOG_INLINE thread_pool::thread_pool(size_t q_max_items,
                                       size_t threads_n,
                                       std::function<void()> on_thread_start)
    : thread_pool(q_max_items, threads_n, on_thread_start, [] {}) {}

SPDLOG_INLINE thread_pool::thread_pool(size_t q_max_items, size_t threads_n)
    : thread_pool(q_max_items, threads_n, [] {}, [] {}) {}

// message all threads to terminate gracefully join them
SPDLOG_INLINE thread_pool::~thread_pool() {
    SPDLOG_TRY {
        for (size_t i = 0; i < threads_.size(); i++) {
            post_async_msg_(async_msg(async_msg_type::terminate), async_overflow_policy::block);
        }

        for (auto &t : threads_) {
            t.join();
        }
    }
    SPDLOG_CATCH_STD
}

void SPDLOG_INLINE thread_pool::post_log(async_logger_ptr &&worker_ptr,
                                         const details::log_msg &msg,
                                         async_overflow_policy overflow_policy) {
    async_msg async_m(std::move(worker_ptr), async_msg_type::log, msg);
    post_async_msg_(std::move(async_m), overflow_policy);
}

std::future<void> SPDLOG_INLINE thread_pool::post_flush(async_logger_ptr &&worker_ptr,
                                                        async_overflow_policy overflow_policy) {
    std::promise<void> promise;
    std::future<void> future = promise.get_future();
    post_async_msg_(async_msg(std::move(worker_ptr), async_msg_type::flush, std::move(promise)),
                    overflow_policy);
    return future;
}

size_t SPDLOG_INLINE thread_pool::overrun_counter() { return q_.overrun_counter(); }

void SPDLOG_INLINE thread_pool::reset_overrun_counter() { q_.reset_overrun_counter(); }

size_t SPDLOG_INLINE thread_pool::discard_counter() { return q_.discard_counter(); }

void SPDLOG_INLINE thread_pool::reset_discard_counter() { q_.reset_discard_counter(); }

size_t SPDLOG_INLINE thread_pool::queue_size() { return q_.size(); }

void SPDLOG_INLINE thread_pool::post_async_msg_(async_msg &&new_msg,
                                                async_overflow_policy overflow_policy) {
    if (overflow_policy == async_overflow_policy::block) {
        q_.enqueue(std::move(new_msg));
    } else if (overflow_policy == async_overflow_policy::overrun_oldest) {
        q_.enqueue_nowait(std::move(new_msg));
    } else {
        assert(overflow_policy == async_overflow_policy::discard_new);
        q_.enqueue_if_have_room(std::move(new_msg));
    }
}

void SPDLOG_INLINE thread_pool::worker_loop_() {
    while (process_next_msg_()) {
    }
}

// process next message in the queue
// return true if this thread should still be active (while no terminate msg
// was received)
bool SPDLOG_INLINE thread_pool::process_next_msg_() {
    async_msg incoming_async_msg;
    q_.dequeue(incoming_async_msg);

    switch (incoming_async_msg.msg_type) {
        case async_msg_type::log: {
            incoming_async_msg.worker_ptr->backend_sink_it_(incoming_async_msg);
            return true;
        }
        case async_msg_type::flush: {
            incoming_async_msg.worker_ptr->backend_flush_();
            incoming_async_msg.flush_promise.set_value();
            return true;
        }

        case async_msg_type::terminate: {
            return false;
        }

        default: {
            assert(false);
        }
    }

    return true;
}

}  // namespace details
}  // namespace spdlog
