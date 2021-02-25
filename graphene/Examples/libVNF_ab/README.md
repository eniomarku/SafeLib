
# About ab

1.AB scenario contains two components a and b. <br />
2.The packet flow is as shown below <br />
  a->b, b->a in a closed loop fashion. **a** generates multiple request to **b**, and **b** in each request perform some operations <br />
3. **b** is built using libvnf <br />
4. **a** is built without libvnf


# How to run
 1. To run ab over SafeLib, two physical machines are needed which are interconnected with each other via a private connection <br />
 2. In one of the machines build and run **b** , and at the other machine build and run **a** <br />
 3. The machine which will be used to build and run **b** _must_ have SafeLib built previously because b itself is built over libVNF <br />
 4. The machine which will be used to build and run **a**, does not require SGX support neither SafeLib.
 
## Build b executable
>>Before building **b**, copy and paste server.conf and ipsec.cfg from SafeLib/graphene/Examples/libVNF_epc/libVNF-release-socc/examples/epc/mme to 
SafeLib/graphene/Examples/libVNF_ab/ab <br />

>> cd /SafeLib/mtcp

>> export RTE_SDK=`echo $PWD`/dpdk 

>> export RTE_TARGET=x86_64-native-linuxapp-gcc

>> cd /SafeLib/graphene/Examples/libVNF_ab/ab

>> make clean\
>> make

>> ifconfig dpdk0 x.x.x.x netmask 255.255.255.0 up

    Note that before building b add your own path for mtcp library and header at Makefile
      

## Build a in another machine (Enterprise aka trusted host)

1. To build and run **a** and ran you must download on your machine only the folder named "TrustedHost" <br />
2. Then execute the following command <br />
>> cd TrustedHost

>> make clean && make

## Run b (Cloud provider aka untrusted host)
>>The process is the same as with LTE EPC, please refer to https://github.com/eniomarku/SafeLib/tree/master/graphene/Examples/libVNF_epc for more details <br />
    1.1 You need to copy b-kernelbypass-dynamic executable, server.conf, and ipsec.cfg from SafeLib/graphene/Examples/libVNF_ab/ab to SafeLib/graphene/Examples/libVNF_ab <br />
    1.2 Then execute: SGX=1 ./pal_loader b-kernelbypass-dynamic 4 x.x.x.x 5000 y.y.y.y 5000 at directory SafeLib/graphene/Examples/libVNF_ab <br />
    
        Note that x.x.x.x is the IP you set for dpdk interface, and y.y.y.y is the IP of the network interface of the trusted machine which is interconnected with dpdk interface of un trusted machine
     
 ## Run a
  >> cd TrustedHost

  
  >> ./a 4 60 y.y.y.y 6006 x.x.x.x 5000 
  
 
