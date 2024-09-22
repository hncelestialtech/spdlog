#include <spdlog/details/cpuset.h>
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline int val_to_char(int v) {
    if (v >= 0 && v < 10) return '0' + v;
    if (v >= 10 && v < 16) return ('a' - 10) + v;
    return -1;
}

static inline int char_to_val(int c) {
    int cl;

    if (c >= '0' && c <= '9') return c - '0';
    cl = tolower(c);
    if (cl >= 'a' && cl <= 'f') return cl + (10 - 'a');
    return -1;
}

static const char *nexttoken(const char *q, int sep) {
    if (q) q = strchr(q, sep);
    if (q) q++;
    return q;
}

/*
 * Number of bits in a CPU bitmask on current system
 */
int get_max_number_of_cpus(void) { return sysconf(_SC_NPROCESSORS_CONF); }

#ifdef __linux__

/*
 * Allocates a new set for ncpus and returns size in bytes and size in bits
 */
cpu_set_t *cpuset_alloc(int ncpus, size_t *setsize, size_t *nbits) {
    cpu_set_t *set = CPU_ALLOC(ncpus);

    if (!set) return NULL;
    if (setsize) *setsize = CPU_ALLOC_SIZE(ncpus);
    if (nbits) *nbits = cpuset_nbits(CPU_ALLOC_SIZE(ncpus));
    return set;
}

void cpuset_free(cpu_set_t *set) { CPU_FREE(set); }

    #if !HAVE_DECL_CPU_ALLOC
/* Please, use CPU_COUNT_S() macro. This is fallback */
int __cpuset_count_s(size_t setsize, const cpu_set_t *set) {
    int s = 0;
    const __cpu_mask *p = set->__bits;
    const __cpu_mask *end = &set->__bits[setsize / sizeof(__cpu_mask)];

    while (p < end) {
        __cpu_mask l = *p++;

        if (l == 0) continue;
        #if LONG_BIT > 32
        l = (l & 0x5555555555555555ul) + ((l >> 1) & 0x5555555555555555ul);
        l = (l & 0x3333333333333333ul) + ((l >> 2) & 0x3333333333333333ul);
        l = (l & 0x0f0f0f0f0f0f0f0ful) + ((l >> 4) & 0x0f0f0f0f0f0f0f0ful);
        l = (l & 0x00ff00ff00ff00fful) + ((l >> 8) & 0x00ff00ff00ff00fful);
        l = (l & 0x0000ffff0000fffful) + ((l >> 16) & 0x0000ffff0000fffful);
        l = (l & 0x00000000fffffffful) + ((l >> 32) & 0x00000000fffffffful);
        #else
        l = (l & 0x55555555ul) + ((l >> 1) & 0x55555555ul);
        l = (l & 0x33333333ul) + ((l >> 2) & 0x33333333ul);
        l = (l & 0x0f0f0f0ful) + ((l >> 4) & 0x0f0f0f0ful);
        l = (l & 0x00ff00fful) + ((l >> 8) & 0x00ff00fful);
        l = (l & 0x0000fffful) + ((l >> 16) & 0x0000fffful);
        #endif
        s += l;
    }
    return s;
}
    #endif

/*
 * Returns human readable representation of the cpuset. The output format is
 * a list of CPUs with ranges (for example, "0,1,3-9").
 */
char *cpulist_create(char *str, size_t len, cpu_set_t *set, size_t setsize) {
    size_t i;
    char *ptr = str;
    int entry_made = 0;
    size_t max = cpuset_nbits(setsize);

    for (i = 0; i < max; i++) {
        if (CPU_ISSET_S(i, setsize, set)) {
            int rlen;
            size_t j, run = 0;
            entry_made = 1;
            for (j = i + 1; j < max; j++) {
                if (CPU_ISSET_S(j, setsize, set))
                    run++;
                else
                    break;
            }
            if (!run)
                rlen = snprintf(ptr, len, "%zu,", i);
            else if (run == 1) {
                rlen = snprintf(ptr, len, "%zu,%zu,", i, i + 1);
                i++;
            } else {
                rlen = snprintf(ptr, len, "%zu-%zu,", i, i + run);
                i += run;
            }
            if (rlen < 0 || (size_t)rlen >= len) return NULL;
            ptr += rlen;
            len -= rlen;
        }
    }
    ptr -= entry_made;
    *ptr = '\0';

    return str;
}

/*
 * Returns string with CPU mask.
 */
char *cpumask_create(char *str, size_t len, cpu_set_t *set, size_t setsize) {
    char *ptr = str;
    char *ret = NULL;
    int cpu;

    for (cpu = cpuset_nbits(setsize) - 4; cpu >= 0; cpu -= 4) {
        char val = 0;

        if (len == (size_t)(ptr - str)) break;

        if (CPU_ISSET_S(cpu, setsize, set)) val |= 1;
        if (CPU_ISSET_S(cpu + 1, setsize, set)) val |= 2;
        if (CPU_ISSET_S(cpu + 2, setsize, set)) val |= 4;
        if (CPU_ISSET_S(cpu + 3, setsize, set)) val |= 8;

        if (!ret && val) ret = ptr;
        *ptr++ = val_to_char(val);
    }
    *ptr = '\0';
    return ret ? ret : ptr - 1;
}

/*
 * Parses string with CPUs mask.
 */
int cpumask_parse(const char *str, cpu_set_t *set, size_t setsize) {
    int len = strlen(str);
    const char *ptr = str + len - 1;
    int cpu = 0;

    /* skip 0x, it's all hex anyway */
    if (len > 1 && !memcmp(str, "0x", 2L)) str += 2;

    CPU_ZERO_S(setsize, set);

    while (ptr >= str) {
        char val;

        /* cpu masks in /sys uses comma as a separator */
        if (*ptr == ',') ptr--;

        val = char_to_val(*ptr);
        if (val == (char)-1) return -1;
        if (val & 1) CPU_SET_S(cpu, setsize, set);
        if (val & 2) CPU_SET_S(cpu + 1, setsize, set);
        if (val & 4) CPU_SET_S(cpu + 2, setsize, set);
        if (val & 8) CPU_SET_S(cpu + 3, setsize, set);
        ptr--;
        cpu += 4;
    }

    return 0;
}

static int nextnumber(const char *str, char **end, unsigned int *result) {
    errno = 0;
    if (str == NULL || *str == '\0' || !isdigit(*str)) return -EINVAL;
    *result = (unsigned int)strtoul(str, end, 10);
    if (errno) return -errno;
    if (str == *end) return -EINVAL;
    return 0;
}

/*
 * Parses string with list of CPU ranges.
 * Returns 0 on success.
 * Returns 1 on error.
 * Returns 2 if fail is set and a cpu number passed in the list doesn't fit
 * into the cpu_set. If fail is not set cpu numbers that do not fit are
 * ignored and 0 is returned instead.
 */
int cpulist_parse(const char *str, cpu_set_t *set, size_t setsize, int fail) {
    size_t max = cpuset_nbits(setsize);
    const char *p, *q;
    char *end = NULL;

    q = str;
    CPU_ZERO_S(setsize, set);

    while (p = q, q = nexttoken(q, ','), p) {
        unsigned int a; /* beginning of range */
        unsigned int b; /* end of range */
        unsigned int s; /* stride */
        const char *c1, *c2;

        if (nextnumber(p, &end, &a) != 0) return 1;
        b = a;
        s = 1;
        p = end;

        c1 = nexttoken(p, '-');
        c2 = nexttoken(p, ',');

        if (c1 != NULL && (c2 == NULL || c1 < c2)) {
            if (nextnumber(c1, &end, &b) != 0) return 1;

            c1 = end && *end ? nexttoken(end, ':') : NULL;

            if (c1 != NULL && (c2 == NULL || c1 < c2)) {
                if (nextnumber(c1, &end, &s) != 0) return 1;
                if (s == 0) return 1;
            }
        }

        if (!(a <= b)) return 1;
        while (a <= b) {
            if (fail && (a >= max)) return 2;
            CPU_SET_S(a, setsize, set);
            a += s;
        }
    }

    if (end && *end) return 1;
    return 0;
}
#endif

#ifdef __cplusplus
}
#endif