#ifndef CPU_SET_H
#define CPU_SET_H

#include <sched.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __USE_GNU

#ifdef __linux__
#define CPU_ZERO_S(setsize, cpusetp)                  \
   do                                                 \
   {                                                  \
      size_t __i;                                     \
      size_t __imax = (setsize) / sizeof(__cpu_mask); \
      __cpu_mask *__bits = (cpusetp)->__bits;         \
      for (__i = 0; __i < __imax; ++__i)              \
         __bits[__i] = 0;                             \
   } while (0)

#define CPU_SET_S(cpu, setsize, cpusetp) \
   ({ size_t __cpu = (cpu);						      \
      __cpu < 8 * (setsize)						      \
      ? (((__cpu_mask *) ((cpusetp)->__bits))[__CPUELT (__cpu)]		      \
	 |= __CPUMASK (__cpu))						      \
      : 0; })

#define CPU_ISSET_S(cpu, setsize, cpusetp) \
   ({ size_t __cpu = (cpu);						      \
      __cpu < 8 * (setsize)						      \
      ? ((((__cpu_mask *) ((cpusetp)->__bits))[__CPUELT (__cpu)]	      \
	  & __CPUMASK (__cpu))) != 0					      \
      : 0; })

#define CPU_EQUAL_S(setsize, cpusetp1, cpusetp2) \
   ({ __cpu_mask *__arr1 = (cpusetp1)->__bits;				      \
      __cpu_mask *__arr2 = (cpusetp2)->__bits;				      \
      size_t __imax = (setsize) / sizeof (__cpu_mask);			      \
      size_t __i;							      \
      for (__i = 0; __i < __imax; ++__i)				      \
	if (__arr1[__i] != __arr2[__i])					      \
	  break;							      \
      __i == __imax; })

   extern int __cpuset_count_s(size_t setsize, const cpu_set_t *set);
#define CPU_COUNT_S(setsize, cpusetp) __cpuset_count_s(setsize, cpusetp)

#define CPU_ALLOC_SIZE(count) \
   ((((count) + __NCPUBITS - 1) / __NCPUBITS) * sizeof(__cpu_mask))
#define CPU_ALLOC(count) (cpu_set_t *)(malloc(CPU_ALLOC_SIZE(count)))
#define CPU_FREE(cpuset) (free(cpuset))

#endif /* __linux__ */

#endif /* __USE_GNU */

#ifdef __linux__
#define cpuset_nbits(setsize) (8 * (setsize))

   /*
    * The @idx parameter returns an index of the first mask from @ary array where
    * the @cpu is set.
    *
    * Returns: 0 if found, otherwise 1.
    */
   static inline int cpuset_ary_isset(size_t cpu, cpu_set_t **ary, size_t nmemb,
                                      size_t setsize, size_t *idx)
   {
      size_t i;

      for (i = 0; i < nmemb; i++)
      {
         if (CPU_ISSET_S(cpu, setsize, ary[i]))
         {
            *idx = i;
            return 0;
         }
      }
      return 1;
   }

   extern int get_max_number_of_cpus(void);

   extern cpu_set_t *cpuset_alloc(int ncpus, size_t *setsize, size_t *nbits);
   extern void cpuset_free(cpu_set_t *set);

   extern char *cpulist_create(char *str, size_t len, cpu_set_t *set, size_t setsize);
   extern int cpulist_parse(const char *str, cpu_set_t *set, size_t setsize, int fail);

   extern char *cpumask_create(char *str, size_t len, cpu_set_t *set, size_t setsize);
   extern int cpumask_parse(const char *str, cpu_set_t *set, size_t setsize);

#endif /* __linux__ */
#ifdef __cplusplus
}
#endif

#endif // CPU_SET_H