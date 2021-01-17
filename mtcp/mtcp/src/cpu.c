#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <numa.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <assert.h>
#include "mtcp_api.h"
// rider added 2020-9-23 
//#ifndef DISABLE_DPDK
#if     !defined(DISABLE_DPDK) || defined(IN_ENCLAVE)
#include <rte_per_lcore.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <gmp.h>
#include <mtcp.h>
#endif

#define MAX_FILE_NAME 1024

/*----------------------------------------------------------------------------*/
inline int 
GetNumCPUs() 
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}
/*----------------------------------------------------------------------------*/
pid_t 
Gettid()
{
	return syscall(__NR_gettid);
}
/*----------------------------------------------------------------------------*/
inline int
whichCoreID(int thread_no)
{
// rider added 2020-9-23 
//#ifndef DISABLE_DPDK
#if     !defined(DISABLE_DPDK)
	int i, cpu_id;
	if (mpz_get_ui(CONFIG._cpumask) == 0)
		return thread_no;
	else {
		int limit =  mpz_popcount(CONFIG._cpumask);
		
		for (cpu_id = 0, i = 0; i < limit; cpu_id++)
			if (mpz_tstbit(CONFIG._cpumask, cpu_id)) {
				if (thread_no == i)
					return cpu_id;
				i++;
			}
	}
#endif
	return thread_no;
}
/*----------------------------------------------------------------------------*/
// int 
// mtcp_core_affinitize(int cpu)
// {
// 	printf("RIDER REPORT: mtcp_core_affinitize function cpu=%d, sched_getcpu()=%d\n", cpu, sched_getcpu()); fflush(stdout);
// 	cpu_set_t cpus;
// 	size_t n;
// 	int ret;

// 	n = GetNumCPUs();

// 	cpu = whichCoreID(cpu);
	
// 	if (cpu < 0 || cpu >= (int) n) {
// 		errno = -EINVAL;
// 		return -1;
// 	}
// 	printf("RIDER REPORT: mtcp_core_affinitize pin 1 cpu=%d\n", cpu); fflush(stdout);

// 	CPU_ZERO(&cpus);
// 	CPU_SET((unsigned)cpu, &cpus);

// // rider added 2020-9-23 
// //#ifndef DISABLE_DPDK
// #if  1 ||   !defined(DISABLE_DPDK)
// 	ret = rte_thread_set_affinity(&cpus);
// 	printf("RIDER REPORT: mtcp_core_affinitize pin 1-1 ret=%d, sched_getcpu()=%d\n", ret, sched_getcpu()); fflush(stdout);
// 	return ret;
// #else

// 	printf("RIDER REPORT: mtcp_core_affinitize pin 2 cpu=%d\n", cpu); fflush(stdout);

// 	struct bitmask *bmask;
// 	FILE *fp;
// 	char sysfname[MAX_FILE_NAME];
// 	int phy_id;
	
// 	ret = sched_setaffinity(Gettid(), sizeof(cpus), &cpus);

// 	printf("RIDER REPORT: mtcp_core_affinitize pin 3 cpu=%d, ret=%d, numa_max_node()=%d\n", cpu, ret, numa_max_node()); fflush(stdout);

// 	if (numa_max_node() == 0)
// 		return ret;
	
// 	printf("RIDER REPORT: mtcp_core_affinitize pin 3-1\n"); fflush(stdout);

// 	bmask = numa_bitmask_alloc(numa_max_node() + 1);
// 	assert(bmask);

// 	/* read physical id of the core from sys information */
// 	snprintf(sysfname, MAX_FILE_NAME - 1, 
// 			"/sys/devices/system/cpu/cpu%d/topology/physical_package_id", cpu);
// 	fp = fopen(sysfname, "r");
// 	if (!fp) {
// 		perror(sysfname);
// 		errno = EFAULT;
// 		return -1;
// 	}
// 	printf("RIDER REPORT: mtcp_core_affinitize pin 4\n"); fflush(stdout);

// 	ret = fscanf(fp, "%d", &phy_id);
// 	if (ret != 1) {
// 		fclose(fp);
// 		perror("Fail to read core id");
// 		errno = EFAULT;
// 		return -1;
// 	}
// 	printf("RIDER REPORT: mtcp_core_affinitize pin 5\n"); fflush(stdout);

// 	numa_bitmask_setbit(bmask, phy_id);
// 	numa_set_membind(bmask);
// 	numa_bitmask_free(bmask);

// 	fclose(fp);
// 	printf("RIDER REPORT: mtcp_core_affinitize pin 6\n"); fflush(stdout);

// #endif
// 	return ret;
// }


int 
mtcp_core_affinitize(int cpu)
{
	printf("RIDER REPORT: mtcp_core_affinitize function started\n"); fflush(stdout);
	int ret;
    cpu_set_t cpus, get_cpus;
    long numprocs = sysconf(_SC_NPROCESSORS_ONLN);
    if (numprocs < 0) {
        printf("Failed to retrieve the number of logical processors!"); fflush(stdout);
		return -1;
    }

	CPU_ZERO(&cpus);
	CPU_ZERO(&get_cpus);
	CPU_SET(cpu, &cpus);
	ret = sched_setaffinity(0, sizeof(cpus), &cpus);
	if (ret < 0) {
		printf("Failed to set affinity for current thread, core id: %d", cpu); fflush(stdout);
		return -1;
	}
	ret = sched_getaffinity(0, sizeof(get_cpus), &get_cpus);
	if (ret < 0) {
		printf("Failed to get affinity for current thread, core id: %d", cpu); fflush(stdout);
		return -1;
	}
	if (!CPU_EQUAL_S(sizeof(cpus), &cpus, &get_cpus)) {
		printf("The get cpu set is not equal to set on core id: %d", cpu); fflush(stdout);
		return -1;
	}
    printf("RIDER REPORT: mtcp_core_affinitize function finished ret=%d\n", ret); fflush(stdout);
	return ret;
}
