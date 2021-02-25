include ../../../../Scripts/Makefile.configs
include Makefile.am

CFLAGS += \
	-I. \
	-I../.. \
	-I../../../include \
	-I../../../include/pal \
	-I../../../include/arch/$(ARCH) \
	-I../../../include/arch/$(ARCH)/Linux \
	-I../../../include/host/Linux-SGX \
	-I../../../include/host/Linux-common \
	-I../../../include/lib \
	-I../../../lib/crypto/mbedtls/include \
	-I../../../lib/crypto/mbedtls/crypto/include \
	-Iprotected-files

# Some of the code uses alignof on expressions, which is a GNU extension.
# Silence Clang - it complains but does support it.
CFLAGS += $(cc-option,-Wno-gnu-alignof-expression)

ASFLAGS += \
	-I. \
	-I../.. \
	-I../../../include \
	-I../../../include/arch/$(ARCH)/Linux

host_files = libpal-Linux-SGX.a pal-sgx gdb_integration/sgx_gdb.so pal.map generated_offsets.py

defs	= -DIN_PAL
CFLAGS += $(defs)
ASFLAGS += $(defs)

commons_objs_encl = bogomips.o stack_protector.o
commons_objs_urts = file_utils.o main_exec_path.o

enclave-objs = \
	db_devices.o \
	db_eventfd.o \
	db_events.o \
	db_exception.o \
	db_files.o \
	db_main.o \
	db_main-$(ARCH).o \
	db_memory.o \
	db_misc.o \
	db_mutex.o \
	db_object.o \
	db_pipes.o \
	db_process.o \
	db_rtld.o \
	db_sockets.o \
	db_streams.o \
	db_threading.o \
	enclave_ecalls.o \
	enclave_framework.o \
	enclave_ocalls.o \
	enclave_pages.o \
	enclave_pf.o \
	enclave_platform.o \
	enclave_untrusted.o \
	enclave_xstate.o \
	protected-files/lru_cache.o \
	protected-files/protected_files.o \
	$(commons_objs_encl)

enclave-asm-objs = enclave_entry.o

urts-objs = \
	clone-x86_64.o \
	sgx_enclave.o \
	sgx_exception.o \
	sgx_framework.o \
	sgx_graphene.o \
	sgx_main.o \
	sgx_perf_data.o \
	sgx_platform.o \
	sgx_process.o \
	sgx_profile.o \
	sgx_gdb_info.o \
	sgx_thread.o \
	quote/aesm.pb-c.o \
	$(commons_objs_urts)

urts-asm-objs = sgx_entry.o

graphene_lib = .lib/graphene-lib.a

# rider added 2020-9-18 add mtcp dependency
mtcp_lib  = /home/nuc/SafeLib/mtcp/mtcp/lib/libmtcp.a

# use TLS-based stack protector of GCC (trusted PAL must have a canary in its PAL_TCB at offset
# 0x8, untrusted PAL uses the default GCC scheme)
# not all compilers support mstack-protector-guard, so use stack protector only if supported
CFLAGS += -fno-stack-protector
ifeq ($(ARCH),x86_64)
$(enclave-objs): CFLAGS += $(call cc-option, -fstack-protector-strong -mstack-protector-guard=tls \
                             -mstack-protector-guard-reg=%gs -mstack-protector-guard-offset=8)
$(urts-objs): CFLAGS += $(call cc-option, -fstack-protector-strong)
endif

.PHONY: all
all: gsgx.h $(host_files) tools

libpal-Linux-SGX.a: $(enclave-objs) $(enclave-asm-objs)
	$(call cmd,ar_a_o)

$(enclave-objs): CFLAGS += -DIN_ENCLAVE
$(patsubst %.o,%.i,$(enclave-objs)): CFLAGS += -DIN_ENCLAVE
$(patsubst %.o,%.s,$(enclave-objs)): CFLAGS += -DIN_ENCLAVE
$(enclave-asm-objs): ASFLAGS += -DIN_ENCLAVE
$(patsubst %.o,%.s,$(enclave-asm-objs)): ASFLAGS += -DIN_ENCLAVE

$(urts-objs): quote/aesm.pb-c.h

$(commons_objs_encl) $(commons_objs_urts): %.o: ../Linux-common/%.c
	$(call cmd,cc_o_c)

%.o: %.c
	$(call cmd,cc_o_c)

%.i: %.c
	$(call cmd,cpp_i_c)

%.s: %.c
	$(call cmd,cc_s_c)

%.o: %.S
	$(call cmd,as_o_S)

%.s: %.S
	$(call cmd,cpp_s_S)

CFLAGS-pal-sgx = -Wl,-z,relro,-z,now -pie
LDLIBS-pal-sgx += -lprotobuf-c

# rider added 2020-9-18 add library dependencies
# DPDK_MACHINE_LINKER_FLAGS=$${RTE_SDK}/$${RTE_TARGET}/lib/ldflags.txt
# DPDK_MACHINE_LDFLAGS=$(shell cat ${DPDK_MACHINE_LINKER_FLAGS})
# LDLIBS-pal-sgx += -pthread -lgmp -L${RTE_SDK}/${RTE_TARGET}/lib ${DPDK_MACHINE_LDFLAGS}
# LDLIBS-pal-sgx += -pthread -lgmp -L${RTE_SDK}/${RTE_TARGET}/lib -Wl,--whole-archive -Wl,-lrte_hash -Wl,-lrte_kvargs -Wl,-lrte_mbuf -Wl,-lrte_net -Wl,-lrte_ethdev -Wl,-lrte_mempool -Wl,-lrte_ring -Wl,-lrte_pci -Wl,-lrte_eal -Wl,-lrte_bus_pci -Wl,-lrte_pmd_e1000 -Wl,-lrte_pmd_ixgbe -Wl,-lnuma -Wl,--no-whole-archive -Wl,-ldl
LDLIBS-pal-sgx +=	-pthread 
LDLIBS-pal-sgx +=	-lgmp 
LDLIBS-pal-sgx +=	-L${RTE_SDK}/${RTE_TARGET}/lib 
#LDLIBS-pal-sgx +=	-Wl,-lrte_flow_classify 
LDLIBS-pal-sgx +=	-Wl,--whole-archive 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pipeline 
#LDLIBS-pal-sgx +=	-Wl,--no-whole-archive 
#LDLIBS-pal-sgx +=	-Wl,--whole-archive 
#LDLIBS-pal-sgx +=	-Wl,-lrte_table 
#LDLIBS-pal-sgx +=	-Wl,--no-whole-archive 
#LDLIBS-pal-sgx +=	-Wl,--whole-archive 
#LDLIBS-pal-sgx +=	-Wl,-lrte_port 
#LDLIBS-pal-sgx +=	-Wl,--no-whole-archive 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pdump 
#LDLIBS-pal-sgx +=	-Wl,-lrte_distributor 
#LDLIBS-pal-sgx +=	-Wl,-lrte_ip_frag 
#LDLIBS-pal-sgx +=	-Wl,-lrte_meter 
#LDLIBS-pal-sgx +=	-Wl,-lrte_fib 
#LDLIBS-pal-sgx +=	-Wl,-lrte_rib 
#LDLIBS-pal-sgx +=	-Wl,-lrte_lpm 
#LDLIBS-pal-sgx +=	-Wl,-lrte_acl 
#LDLIBS-pal-sgx +=	-Wl,-lrte_jobstats 
#LDLIBS-pal-sgx +=	-Wl,-lrte_metrics 
#LDLIBS-pal-sgx +=	-Wl,-lrte_bitratestats 
#LDLIBS-pal-sgx +=	-Wl,-lrte_latencystats 
#LDLIBS-pal-sgx +=	-Wl,-lrte_power 
#LDLIBS-pal-sgx +=	-Wl,-lrte_efd 
#LDLIBS-pal-sgx +=	-Wl,-lrte_bpf 
#LDLIBS-pal-sgx +=	-Wl,-lrte_ipsec 
#LDLIBS-pal-sgx +=	-Wl,--whole-archive 
#LDLIBS-pal-sgx +=	-Wl,-lrte_cfgfile 
#LDLIBS-pal-sgx +=	-Wl,-lrte_gro 
#LDLIBS-pal-sgx +=	-Wl,-lrte_gso 
LDLIBS-pal-sgx +=	-Wl,-lrte_hash 
#LDLIBS-pal-sgx +=	-Wl,-lrte_member 
#LDLIBS-pal-sgx +=	-Wl,-lrte_vhost 
LDLIBS-pal-sgx +=	-Wl,-lrte_kvargs 
LDLIBS-pal-sgx +=	-Wl,-lrte_mbuf 
LDLIBS-pal-sgx +=	-Wl,-lrte_net 
LDLIBS-pal-sgx +=	-Wl,-lrte_ethdev 
#LDLIBS-pal-sgx +=	-Wl,-lrte_bbdev 
#LDLIBS-pal-sgx +=	-Wl,-lrte_cryptodev 
#LDLIBS-pal-sgx +=	-Wl,-lrte_security 
#LDLIBS-pal-sgx +=	-Wl,-lrte_compressdev 
#LDLIBS-pal-sgx +=	-Wl,-lrte_eventdev 
#LDLIBS-pal-sgx +=	-Wl,-lrte_rawdev 
#LDLIBS-pal-sgx +=	-Wl,-lrte_timer 
LDLIBS-pal-sgx +=	-Wl,-lrte_mempool 
#LDLIBS-pal-sgx +=	-Wl,-lrte_stack 
LDLIBS-pal-sgx +=	-Wl,-lrte_mempool_ring 
#LDLIBS-pal-sgx +=	-Wl,-lrte_mempool_octeontx2 
LDLIBS-pal-sgx +=	-Wl,-lrte_ring 
LDLIBS-pal-sgx +=	-Wl,-lrte_pci 
LDLIBS-pal-sgx +=	-Wl,-lrte_eal 
#LDLIBS-pal-sgx +=	-Wl,-lrte_cmdline 
#LDLIBS-pal-sgx +=	-Wl,-lrte_reorder 
#LDLIBS-pal-sgx +=	-Wl,-lrte_sched 
#LDLIBS-pal-sgx +=	-Wl,-lrte_rcu 
#LDLIBS-pal-sgx +=	-Wl,-lrte_kni 
#LDLIBS-pal-sgx +=	-Wl,-lrte_common_cpt 
#LDLIBS-pal-sgx +=	-Wl,-lrte_common_octeontx 
#LDLIBS-pal-sgx +=	-Wl,-lrte_common_octeontx2 
#LDLIBS-pal-sgx +=	-Wl,-lrte_common_dpaax 
LDLIBS-pal-sgx +=	-Wl,-lrte_bus_pci 
#LDLIBS-pal-sgx +=	-Wl,-lrte_bus_vdev 
#LDLIBS-pal-sgx +=	-Wl,-lrte_bus_dpaa 
#LDLIBS-pal-sgx +=	-Wl,-lrte_bus_fslmc 
#LDLIBS-pal-sgx +=	-Wl,-lrte_mempool_bucket 
#LDLIBS-pal-sgx +=	-Wl,-lrte_mempool_stack 
#LDLIBS-pal-sgx +=	-Wl,-lrte_mempool_dpaa 
#LDLIBS-pal-sgx +=	-Wl,-lrte_mempool_dpaa2 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_af_packet 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_ark 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_atlantic 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_avp 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_axgbe 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_bnxt 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_bond 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_cxgbe 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_dpaa 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_dpaa2 
LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_e1000 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_ena 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_enetc 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_enic 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_fm10k 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_failsafe 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_hinic 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_hns3 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_i40e 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_iavf 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_ice 
LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_ixgbe 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_kni 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_lio 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_memif 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_nfp 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_null 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_octeontx2 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_qede 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_ring 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_softnic 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_sfc_efx 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_tap 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_thunderx_nicvf 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_vdev_netvsc 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_virtio 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_vhost 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_ifc 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_vmxnet3_uio 
#LDLIBS-pal-sgx +=	-Wl,-lrte_bus_vmbus 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_netvsc 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_bbdev_null 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_bbdev_fpga_lte_fec 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_bbdev_turbo_sw 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_null_crypto 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_nitrox 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_octeontx_crypto 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_octeontx2_crypto 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_crypto_scheduler 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_dpaa2_sec 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_dpaa_sec 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_caam_jr 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_virtio_crypto 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_octeontx_zip 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_qat 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_skeleton_event 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_sw_event 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_dsw_event 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_octeontx_ssovf 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_dpaa_event 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_dpaa2_event 
#LDLIBS-pal-sgx +=	-Wl,-lrte_mempool_octeontx 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_octeontx 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_octeontx2_event 
#LDLIBS-pal-sgx +=	-Wl,-lrte_pmd_opdl_event 
#LDLIBS-pal-sgx +=	-Wl,-lrte_rawdev_skeleton 
#LDLIBS-pal-sgx +=	-Wl,-lrte_rawdev_dpaa2_cmdif 
#LDLIBS-pal-sgx +=	-Wl,-lrte_rawdev_dpaa2_qdma 
#LDLIBS-pal-sgx +=	-Wl,-lrte_bus_ifpga 
#LDLIBS-pal-sgx +=	-Wl,-lrte_rawdev_ioat 
#LDLIBS-pal-sgx +=	-Wl,-lrte_rawdev_ntb 
#LDLIBS-pal-sgx +=	-Wl,-lrte_rawdev_octeontx2_dma 
LDLIBS-pal-sgx +=	-Wl,--no-whole-archive 
#LDLIBS-pal-sgx +=	-Wl,-lrt 
#LDLIBS-pal-sgx +=	-Wl,-lm 
LDLIBS-pal-sgx +=	-Wl,-lnuma 
LDLIBS-pal-sgx +=	-Wl,-ldl


# rider added 2020-9-18 add mtcp dependency $(mtcp_lib)
pal-sgx: $(urts-objs) $(urts-asm-objs) $(graphene_lib) $(mtcp_lib)
	$(call cmd,cmulti)

quote/aesm.pb-c.o: quote/aesm.pb-c.c quote/aesm.pb-c.h
quote/aesm.pb-c.c quote/aesm.pb-c.h: quote/aesm.proto
	@echo [ host/Linux-SGX/quote/aesm.pb-c.c ]
	@echo [ host/Linux-SGX/quote/aesm.pb-c.h ]
	@protoc-c --c_out=. $<

gdb_integration/sgx_gdb.so: CFLAGS =
CFLAGS-gdb_integration/sgx_gdb.so = -shared -Wall -fPIC -O2 -std=c11
gdb_integration/sgx_gdb.so: gdb_integration/sgx_gdb.c
	$(call cmd,csingle)

enclave_entry.o sgx_entry.o: asm-offsets.h

gsgx.h: gsgx.h.in
	./link-intel-driver.py < $< > $@

ifeq ($(filter %clean,$(MAKECMDGOALS)),)
include $(wildcard *.d) $(wildcard gdb_integration/*.d)
endif

include ../../../../Scripts/Makefile.rules

.PHONY: tools
tools:
	$(MAKE) -C tools

CLEAN_FILES += $(notdir $(pal_static) $(pal_lib) $(pal_loader))
CLEAN_FILES += gdb_integration/sgx_gdb.so
CLEAN_FILES += quote/aesm.pb-c.c quote/aesm.pb-c.h quote/aesm.pb-c.d quote/aesm.pb-c.o
CLEAN_FILES += gsgx.h

.PHONY: clean_
clean_:
	$(RM) -r *.o *.e *.i *.s $(host_files) $(CLEAN_FILES) *.d gdb_integration/*.d signer/*.pyc \
	         __pycache__ signer/__pycache__
	$(RM) -r protected-files/*.o protected-files/*.d

.PHONY: clean
clean: clean_
	$(MAKE) -C tools $@

.PHONY: distclean
distclean: clean_
	$(MAKE) -C tools $@

.PHONY: test
test:
