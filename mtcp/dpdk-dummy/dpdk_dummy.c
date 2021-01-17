#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

int ocall_dpdk_load_module ()
{
	return 0;
}

int ocall_dpdk_init_handle (void * ctxt_ptr, int ctxt_len)
{
	return 0;
}

int ocall_dpdk_destroy_handle (void * ctxt_ptr, int ctxt_len)
{
	return 0;
}

int ocall_set_net_env (char *dev_name_list, char *port_stat_list, void *global_config_ptr, int global_config_len)
{
	return 0;
}

int ocall_cpuid (unsigned int leaf, unsigned int subleaf, unsigned int values[4])
{
	return 0;
}