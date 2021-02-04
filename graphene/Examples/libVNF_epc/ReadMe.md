
# Build mme executable

>> cd /SafeLib/mtcp

>> export RTE_SDK=`echo $PWD`/dpdk 

>> export RTE_TARGET=x86_64-native-linuxapp-gcc

>> cd /SafeLib/graphene/Examples/libVNF_epc/libVNF-release-socc/examples/epc/mme/

>> make clean\
>> make

>> ifconfig dpdk0 x.x.x.x netmask 255.255.255.0 up

    Note that before building mme add your own path for mtcp library and header at Makefile
    
# Build and run mme
>>cd SafeLib/graphene/Examples/libVNF_epc/

>> cp libVNF-release-socc/examples/epc/mme/mme ./\
>>cp libVNF-release-socc/examples/epc/mme/server.conf ./\ 
>>cp libVNF-release-socc/examples/epc/mme/ipsec.cfg ./

>>make SGX=1 distclean && make SGX=1\
>>ifconfig dpdk0 169.254.9.10 netmask 255.255.255.0 up\

>>SGX=1 ./pal_loader mme 1
