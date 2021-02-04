•	SafeLib is a library used for securely outsourcing VNFs in a third party service provider.\
•	This is an implementation of paper: “SafeLib: a practical library for outsourcing stateful network functions securely” submitted at NetSoft 2021

Features:

•	Written entirely in C/C++\
•	Provide integrity and confidentiality protection of user traffic, VNF policies, and integrity of VNF code.\
•	Provide support for stateful VNFs\
•	Provide support for user-level TCP stack\
•	Provide support for kernel bypass mechanisms such as DPDK


Prerequisites
1.	SafeLib makes use of Intel SGX, so first step is to make sure to run SafeLib on Intel CPU machines with support for SGX.
2.	Make sure to have a machine with Linux as OS; we have tested our library only for Ubuntu 18.04, and 20.04 version.
3.	Make sure to use a CPU and NIC supported by DPDK. The list of CPUs and NIC supported by DPDK can be found at http://core.dpdk.org/supported/ . For our testing purpose we have used Intel CPUs and “igb” drivers.
4.	To run our LTE EPC, and ab scenarios two physical machines are needed, which are interconnected via an ethernet cable. Only one of the machines needs to have support for SGX, and DPDK.

 Preliminary steps:
1.	Install Intel SGX driver/DCAP , SDK, and psw. To install them follow the instructions given in https://github.com/intel/linux-sgx
2.	Install nasm 2.14 as follows;

 >> wget http://www.nasm.us/pub/nasm/releasebuilds/2.14.02/nasm-2.14.02.tar.xz \
 >> tar -xf ./nasm-2.14.02-xdoc.tar.xz –strip-components=1 \
 >> ./configure –prefix=/usr \
 >> make\
 >> make install
3.	Install intel-ipsec-mb. Instructions can be found at https://github.com/intel/intel-ipsec-mb
4.	Install spdlog. Instructions can be found at https://github.com/gabime/spdlog
5.	Install boost (libboost-all-dev)
6.	Install libnuma, libpthread, librt, libgmp
7.	Run the following command for building graphene dependencies: 
>> sudo apt-get install -y \   build-essential autoconf gawk bison wget python3 libcurl4-openssl-dev \
   python3-protobuf libprotobuf-c-dev protobuf-c-compiler

Installation:

1. Build dpdk as follows:

 >>cd /SafeLib/mtcp\
 >>export RTE_SDK=`echo $PWD`/dpdk\
 >>export RTE_TARGET=x86_64-native-linuxapp-gcc\
 >>./setup_mtcp_dpdk_env.sh\
    step 38,\
    step 45, \
    step 48: huge pages\
      2048\
    step 51: \
#here register ethernet port corresponding to the NIC of your machine\
    step 62\
       y\
 >>ifconfig dpdk0 x.x.x.x netmask 255.255.255.0 up

2. Build mtcp trusted

  >> cd /SafeLib/mtcp\
  >> export RTE_SDK=`echo $PWD`/dpdk\
  >> export RTE_TARGET=x86_64-native-linuxapp-gcc\
  >> ./configure --with-dpdk-lib=$RTE_SDK/$RTE_TARGET CFLAGS="-DDISABLE_DPDK -DIN_ENCLAVE -DMAX_CPUS=#numberOFMaxCPUCores"\
  >>make clean\
  >>make

      Note: During this process should be an error while building the example applications of mtcp as we didn’t link the library(dpdk-dummy) related to ocall of trusted mtcp. So we can ignore this error.

3. Build libVNF

>>cd /SafeLib/mtcp\ 
>>export RTE_SDK=`echo $PWD`/dpdk\
>>export RTE_TARGET=x86_64-native-linuxapp-gcc\ 
>>cd /SafeLib/graphene/Examples/libVNF_epc/libVNF-release-socc/ \
>>rm -rf build \
>>mkdir build\
>>cd build\
>>cmake .. -DSTACK=KERNEL_BYPASS\
>>make \
>>make install

     Note that before building libVNF, you need to specify your mtcp path in CMakeLists.txt.

4. Build dpdk-dummy

>> cd /SafeLib/mtcp/dpdk-dummy/\
>> make distclean && make\
>> cp libdpdkdummy.so* /usr/lib/x86_64-linux-gnu/\
>> cd /usr/lib/x86_64-linux-gnu/\
>> rm libdpdkdummy.so\
>> ln -s libdpdkdummy.so.1 libdpdkdummy.so

5.Build untrusted mtcp

  >>cd /SafeLib/mtcp\
  >> export RTE_SDK=`echo $PWD`/dpdk\
  >> export RTE_TARGET=x86_64-native-linuxapp-gcc\ 
  >> ./configure --with-dpdk-lib=$RTE_SDK/$RTE_TARGET CFLAGS="-DMAX_CPUS=#numberOFMaxCPUCores "\                 
  >>make clean\
  >>make

6. Enable gsgx device (after restart)

  >> cd /SafeLib/graphene/Pal/src/host/Linux-SGX/sgx-driver\
  >> make
  >> sudo insmod gsgx.ko

    Note that before building graphene add your own mtcp_lib path at the Makefile located at /SafeLib/graphene/Pal/src/host/Linux-SGX

7.	Prepare a signing key
   >>openssl genrsa -3 -out graphene/Pal/src/host/Linux-SGX/signer/enclave-key.pem 3072

8. Build graphene

>>export ISGX_DRIVER_PATH="#the path of linux-sgx-driver"\
>>cd /SafeLib/mtcp\
>>export RTE_SDK=`echo $PWD`/dpdk\
>>export RTE_TARGET=x86_64-native-linuxapp-gcc\ 
>>cd /home/nuc/SafeLib/graphene\
>> make SGX=1 distclean\
>>make SGX=1 DEBUG=1


    The next step is to build test cases; in our case we have tested SafeLib with LTE EPC, and ab
    In case of LTE EPC, the only component build using SafeLib is mme

9. Build mme executable

>> cd /SafeLib/mtcp\ 
>> export RTE_SDK=`echo $PWD`/dpdk \ 
>> export RTE_TARGET=x86_64-native-linuxapp-gcc\ 
>> cd /SafeLib/graphene/Examples/libVNF_epc/libVNF-release-socc/examples/epc/mme/\       
>> make clean\
>> make\ 
>> ifconfig dpdk0 x.x.x.x netmask 255.255.255.0 up

    Note that before building mme add your own path for mtcp library and header at Makefile

