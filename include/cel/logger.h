/*
 * --------------------------------------------------------------
 * File:          logger.h
 * Project:       log
 * Created:       Wednesday, 18th September 2024 10:51:58 am
 * @Author:       Molin Liu
 * Contact:       molinliu@live.com
 * Modified:      Wednesday, 18th September 2024 10:52:40 am
 * Copyright      © Celestial Trading 2024 - 2024
 * --------------------------------------------------------------
 */
#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <ctime>
#include <sstream>
#include <chrono>
#include <ctime>
#include <sys/time.h>
#include <iomanip>
#include <algorithm>
#include <exception>

namespace {
inline std::string get_wall_time_str() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y%m%d_%H%M%S");
    return ss.str();
}
}  // namespace

namespace Cel {
namespace Log {
typedef std::shared_ptr<spdlog::logger> logger_ptr;
static logger_ptr create_logger(int cpu_id,
                                const std::string& name,
                                std::string log_file_dir,
                                std::string log_level) {
    static std::unordered_map<std::string, logger_ptr> loggers;

    if (loggers.find(name) != loggers.end()) {
        return loggers[name];
    }

    std::string cpu_id_str = std::to_string(cpu_id);
    setenv("CEL_SPDLOG_CPU_ID", cpu_id_str.c_str(), 1);

    if (log_file_dir.empty()) {
        log_file_dir = "./";
    }
    // Ensure log_file_dir ends with a slash
    if (log_file_dir.back() != '/') {
        log_file_dir += '/';
    }

    std::string log_file_name = log_file_dir + name + "_" + get_wall_time_str() + ".log";

    auto logger = spdlog::basic_logger_mt<spdlog::async_factory>(name, log_file_name);

    // Convert log_level to lowercase
    std::transform(log_level.begin(), log_level.end(), log_level.begin(), ::tolower);

    // Validate and convert log_level to spdlog::level::level_enum
    spdlog::level::level_enum spdlog_level;
    if (log_level == "debug") {
        spdlog_level = spdlog::level::debug;
    } else if (log_level == "info") {
        spdlog_level = spdlog::level::info;
    } else if (log_level == "warning" || log_level == "warn") {
        spdlog_level = spdlog::level::warn;
    } else if (log_level == "error" || log_level == "err") {
        spdlog_level = spdlog::level::err;
    } else {
        // Terminate with error message if an invalid level is provided
        spdlog::error(
            "Invalid log level '{}' provided. Valid levels are: debug, info, warning, error.",
            log_level);
        std::terminate();
    }

    logger->set_level(spdlog_level);
    logger->flush_on(spdlog::level::info);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%F][%n][%l] %v");

    return logger;
}
}  // namespace Log
}  // namespace Cel