B_KERNEL_STATIC=b-kernel-static
B_KERNEL_DYNAMIC=b-kernel-dynamic
B_KERNELBYPASS_STATIC=b-kernelbypass-static
B_KERNELBYPASS_DYNAMIC=b-kernelbypass-dynamic
CXX_FLAGS=-fpermissive -std=c++11 -g -O3 -Wno-write-strings -Wall
A=a
C=c

# options
DPDK=1
PS=0
NETMAP=0

# mtcp library and header
MTCP_P_FLD  = /home/nuc/graphene_test/mtcp
MTCP_FLD    = $(MTCP_P_FLD)/mtcp
MTCP_INC    = -I$(MTCP_FLD)/include
MTCP_LIB    = -L$(MTCP_FLD)/lib
MTCP_TARGET = $(MTCP_LIB)/libmtcp.a

# mtcp util library and header
UTIL_FLD = $(MTCP_P_FLD)/util
UTIL_INC = -I$(UTIL_FLD)/include
UTIL_OBJ = $(UTIL_FLD)/http_parsing.o $(UTIL_FLD)/tdate_parse.o $(UTIL_FLD)/netlib.o

INCLUDES = $(UTIL_INC) $(MTCP_INC)

LIBS = $(MTCP_LIB) -lmtcp -lpthread -lnuma -lrt -lboost_system -ldpdkdummy -lIPSec_MB

# ps library and header
PS_DIR = $(MTCP_P_FLD)/io_engine/
PS_INC = -I$(PS_DIR)/include
ifeq ($(PS),1)
	INCLUDES += $(PS_INC)
	LIBS += -lmtcp -L$(PS_DIR)/lib -lps -lpthread -lnuma -lrt
endif

# dpdk library and header
DPDK_INC=$(MTCP_P_FLD)/dpdk/include
DPDK_LIB=$(MTCP_P_FLD)/dpdk/lib
#ifeq ($(DPDK),1)
#	DPDK_MACHINE_FLAGS = $(shell cat $(MTCP_P_FLD)/dpdk/include/cflags.txt)
#	INCLUDES += $(DPDK_MACHINE_FLAGS) -I$(DPDK_INC) -include $(DPDK_INC)/rte_config.h
#endif

ifeq ($(shell uname -m),x86_64)
	LIBS += -m64
endif

#ifeq ($(DPDK),1)
#	DPDK_LIB_FLAGS = $(shell cat $(MTCP_P_FLD)/dpdk/lib/ldflags.txt)
#	LIBS += -ldl -march=native -export-dynamic -L$(MTCP_P_FLD)/dpdk/lib $(DPDK_LIB_FLAGS)
#else
#	LIBS += -ldl -march=native -export-dynamic -L$(MTCP_P_FLD)/dpdk/lib $(DPDK_LIB_FLAGS)
#endif
ifeq ($(DPDK),1)
DPDK_MACHINE_LINKER_FLAGS=$${RTE_SDK}/$${RTE_TARGET}/lib/ldflags.txt
DPDK_MACHINE_LDFLAGS=$(shell cat ${DPDK_MACHINE_LINKER_FLAGS})
LIBS += -g -O3 -pthread -lrt -march=native ${MTCP_FLD}/lib/libmtcp.a -lnuma -lmtcp -lpthread -lrt -ldl -lgmp -L${RTE_SDK}/${RTE_TARGET}/lib ${DPDK_MACHINE_LDFLAGS}
endif

all:

$(B_KERNELBYPASS_STATIC): b.cpp
	g++ $(CXX_FLAGS) b.cpp -c
	g++ b.o -o $(B_KERNELBYPASS_STATIC) $(CXX_FLAGS) -lvnf-kernelbypass-static $(LIBS) $(INCLUDES)

$(B_KERNELBYPASS_DYNAMIC): b.cpp
	g++ b.cpp -o $(B_KERNELBYPASS_DYNAMIC) $(CXX_FLAGS) -lvnf-kernelbypass-dynamic $(LIBS) $(INCLUDES)

$(B_KERNEL_STATIC): b.cpp
	g++ $(CXX_FLAGS) b.cpp -c
	g++ b.o -O3 -std=c++11 -lvnf-kernel-static -lpthread -lboost_system -lsctp -o $(B_KERNEL_STATIC)

$(B_KERNEL_DYNAMIC): b.cpp
	g++ b.cpp -O3 -std=c++11 -lvnf-kernel-dynamic -lpthread -lboost_system -lsctp -o $(B_KERNEL_DYNAMIC)

$(A): a.cpp
	g++ a.cpp -std=c++11 -lpthread -lboost_system -o $(A)

$(C): c.cpp
	g++ c.cpp -std=c++11 -lpthread -lboost_system -o $(C)

clean:
	rm -f *~ *.o $(B_KERNEL_STATIC) $(B_KERNEL_DYNAMIC) $(B_KERNELBYPASS_STATIC) $(B_KERNELBYPASS_DYNAMIC) $(A) $(C) log_*
