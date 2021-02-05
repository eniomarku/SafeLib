
# Build mme executable

>> cd /SafeLib/mtcp

>> export RTE_SDK=`echo $PWD`/dpdk 

>> export RTE_TARGET=x86_64-native-linuxapp-gcc

>> cd /SafeLib/graphene/Examples/libVNF_epc/libVNF-release-socc/examples/epc/mme/

>> make clean\
>> make

>> ifconfig dpdk0 x.x.x.x netmask 255.255.255.0 up

    Note that before building mme add your own path for mtcp library and header at Makefile
    
    
# Build and run mme on the machine you built SafeLib (Enterprise aka trusted host)
>>cd SafeLib/graphene/Examples/libVNF_epc/

>> cp libVNF-release-socc/examples/epc/mme/mme ./\
>>cp libVNF-release-socc/examples/epc/mme/server.conf ./

>>cp libVNF-release-socc/examples/epc/mme/ipsec.cfg ./

>>make SGX=1 distclean && make SGX=1\
>>ifconfig dpdk0 169.254.9.10 netmask 255.255.255.0 up

>>SGX=1 ./pal_loader mme 1

     Note the paths in Makefile and mme.manifest.template . Place your own local path for all dynamically compiled binaries loaded in the manifest.
     Note that it is recommended to arrange server.conf and ipsec.cfg according to your needs.

# Build and run hss, sgw, pgw, ran in another machine (Cloud provider aka untrusted host)

