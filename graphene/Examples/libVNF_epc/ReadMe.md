# About LTE EPC

1.The EPC contains 5 components ran, mme, hss, sgw and pgw <br />
2.The packet flow is as shown below <br />
  2.1 initial attach and authentication: ran --> mme --> hss --> mme --> ran <br />
  2.2 session setup: ran --> mme --> sgw --> pgw --> sgw --> mme --> ran. <br />
3. mme is built using libvnf <br />
4. ran, hss, sgw, pgw components of epc are built without libvnf


# How to run
 1. To run LTE EPC over SafeLib, two physical machines are needed which are interconnected with each other via a private connection <br />
 2. In one of the machines build and run **mme**is , and at the other machine build and run other components of LTE EPC (hss,sgw,pgw,ran) <br />
 3. The machine which will be used to build and run **mme** _must_ have SafeLib built previously because mme itself is built over libVNF <br />
 4. The machine which will be used to build and run other components of LTE EPC, does not require SGX support neither SafeLib being built.
 
## Build mme executable

>> cd /SafeLib/mtcp

>> export RTE_SDK=`echo $PWD`/dpdk 

>> export RTE_TARGET=x86_64-native-linuxapp-gcc

>> cd /SafeLib/graphene/Examples/libVNF_epc/libVNF-release-socc/examples/epc/mme/

>> make clean\
>> make

>> ifconfig dpdk0 x.x.x.x netmask 255.255.255.0 up

    Note that before building mme add your own path for mtcp library and header at Makefile
      

## Build hss, sgw, pgw, ran in another machine (Enterprise aka trusted host)

1. To build and run hss,sgw,pgw and ran you must download on your machine only the folder named "Trusted-Host" <br />
2. Then execute the following command <br />
>> cd Trusted-Host/src

>> make clean && make

## Run mme (Cloud provider aka untrusted host)
>>cd SafeLib/graphene/Examples/libVNF_epc/

>> cp libVNF-release-socc/examples/epc/mme/mme ./\
>>cp libVNF-release-socc/examples/epc/mme/server.conf ./

>>cp libVNF-release-socc/examples/epc/mme/ipsec.cfg ./

>>make SGX=1 distclean && make SGX=1\
>>ifconfig dpdk0 169.254.9.10 netmask 255.255.255.0 up

>>SGX=1 ./pal_loader mme 1

     Note the paths in Makefile and mme.manifest.template . Place your own local path for all dynamically compiled binaries loaded in the manifest.
     Note that it is recommended to arrange server.conf and ipsec.cfg according to your needs.
     
 ## Run hss, sgw, pgw, ran
  >> cd Trusted-Host/src
  
  >> ./hss.out 1 > hss.log
  
  >> ./sgw.out 1 1 1 > sgw.log
  
  >> ./pgw.out 1 1 > pgw.log
  
  >> ./ransim.out 4 60
  
 
