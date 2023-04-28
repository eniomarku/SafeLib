* SafeLib is a library used for securely outsourcing VNFs in a third party service provider.
*	This is an implementation of paper: “SafeLib: a practical library for outsourcing stateful network functions securely” submitted at NetSoft 2021

# Features:

*	Written entirely in C/C++.
*	Provide integrity and confidentiality protection of user traffic, VNF policies, and integrity of VNF code.
*	Provide support for stateful VNFs.
*	Provide support for user-level TCP stack.
*	Provide support for kernel bypass mechanisms such as DPDK.


# Prerequisites
*	To run our LTE EPC, and ab scenarios two physical machines are needed, which are interconnected via an ethernet cable. Only one of the machines needs to have support for SGX, and DPDK.

   
# Navigating the repository

* libVNF folder contains the source code of libVNF, and some senario cases  <br />
   * Before building our library, one should use libVNF APIs for building stateful VNFs <br />
   * See below how to build libVNF <br />
* mTC folder containts the source code of mTCP, dpdk, and dpdk-dummy <br />
* graphene folder contains the source code of grahene, and our test cases (LTE EPC, ab) <br />
   * Our test cases can be found at https://github.com/eniomarku/SafeLib/tree/master/graphene/Examples


# Tested environment

a) We have tested our library in the following environment:<br />

1. Ubuntu 18.04<br />
2. 5.3.0-28-generic kernel version<br />
3. No Intel SGX DCAP <br />
4. Intel(R) Core(TM) i7-8809G CPU @ 3.10GHz <br />

b) We have tested our library also for the following environment: <br />

1. Ubuntu 20.04 <br />
2. 5.9.0-050900rc6-generic kernel version <br />
3. Intel SGX DCAP https://github.com/intel/SGXDataCenterAttestationPrimitives/ <br />

**Note:** For the second type of environment some changes are needed to be done in the current version of SafeLib published in this repository, as given below: <br />

b.1)  Uncomment the function *static  void
netdev_no_ret_dummy* at dpdk_iface.h<br />
b.2) Uncomment the part of the code between *ridder added .... ridder closed* at compat.h, and kni_dev.h (both part of dpdk)<br />


# Installation:

## 1. Build dpdk as follows:
 >>sudo -s\
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

## 2. Build mtcp trusted

  >> cd /SafeLib/mtcp\
  >> export RTE_SDK=`echo $PWD`/dpdk\
  >> export RTE_TARGET=x86_64-native-linuxapp-gcc\
  >> ./configure --with-dpdk-lib=$RTE_SDK/$RTE_TARGET CFLAGS="-DDISABLE_DPDK -DIN_ENCLAVE -DMAX_CPUS=#numberOFMaxCPUCores"\
  >>make clean\
  >>make

      Note: During this process should be an error while building the example applications of mtcp as we didn’t link the library(dpdk-dummy) related to ocall of trusted mtcp. So we can ignore this error.

## 3. Build libVNF

>>cd /SafeLib/mtcp

>>export RTE_SDK=`echo $PWD`/dpdk\
>>export RTE_TARGET=x86_64-native-linuxapp-gcc 

>>cd /SafeLib/graphene/Examples/libVNF_epc/libVNF-release-socc/ 

>>rm -rf build \
>>mkdir build\
>>cd build\
>>cmake .. -DSTACK=KERNEL_BYPASS\
>>make \
>>make install

     Note that before building libVNF, you need to specify your mtcp path in CMakeLists.txt.

## 4. Build dpdk-dummy

>> cd /SafeLib/mtcp/dpdk-dummy/\
>> make distclean && make\
>> cp libdpdkdummy.so* /usr/lib/x86_64-linux-gnu/\
>> cd /usr/lib/x86_64-linux-gnu/\
>> rm libdpdkdummy.so\
>> ln -s libdpdkdummy.so.1 libdpdkdummy.so

## 5.Build untrusted mtcp

  >>cd /SafeLib/mtcp\
  >> export RTE_SDK=`echo $PWD`/dpdk\
  >> export RTE_TARGET=x86_64-native-linuxapp-gcc
  
  >> ./configure --with-dpdk-lib=$RTE_SDK/$RTE_TARGET CFLAGS="-DMAX_CPUS=#numberOFMaxCPUCores "
  
  >>make clean\
  >>make

## 6. Enable gsgx device (after restart)

  >> cd /SafeLib/graphene/Pal/src/host/Linux-SGX/sgx-driver\
  >> make clean && make\
  >> insmod gsgx.ko

    Note that before building graphene add your own mtcp_lib path at the Makefile located at /SafeLib/graphene/Pal/src/host/Linux-SGX

## 7.	Prepare a signing key
   >>openssl genrsa -3 -out graphene/Pal/src/host/Linux-SGX/signer/enclave-key.pem 3072

## 8. Build graphene

>>export ISGX_DRIVER_PATH="#the path of linux-sgx-driver"\
>>cd /SafeLib/mtcp\
>>export RTE_SDK=`echo $PWD`/dpdk\
>>export RTE_TARGET=x86_64-native-linuxapp-gcc

>>cd /SafeLib/graphene\
>> make SGX=1 distclean\
>>make SGX=1 DEBUG=1


    The next step is to build test cases; in our case we have tested SafeLib with LTE EPC, and ab
    After all the above steps are completed, and in case you want to try and run our scenario cases
    then read ReadMe file located at https://github.com/eniomarku/SafeLib/tree/master/graphene/Examples/libVNF_epc for LTE EPC scenario
    and ReadMe file located at https://github.com/eniomarku/SafeLib/tree/master/graphene/Examples/ab for ab scenario

# Contact Information
GitHub issue board is the preferred way to report bugs and ask questions about SafeLib.

For any difficulties experienced during the process of building SafeLib or any other question, please contact us at the details given below <br />
CONTACTS FOR THE AUTHORS\
enio.marku@ntnu.no\
biczok@crysys.hu

