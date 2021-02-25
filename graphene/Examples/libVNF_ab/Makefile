

################################# CONSTANTS ###################################

# Relative path to Graphene root
GRAPHENEDIR = /home/nuc/SafeLib/graphene
SGX_SIGNER_KEY ?= $(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/enclave-key.pem

SRCDIR = /home/nuc/SafeLib
COMMIT = master
TAR_SHA256 = 35839b9b8d2889e67e452aaccdd202797bfd0014

ifeq ($(DEBUG),1)
GRAPHENEDEBUG = inline
else
GRAPHENEDEBUG = none
endif

.PHONY: all
all:  b-kernelbypass-dynamic.manifest pal_loader
ifeq ($(SGX),1)
all: b-kernelbypass-dynamic.manifest.sgx
endif

include ../../Scripts/Makefile.configs



#ifeq ($(USE_SELECT),1)
#$(SRCDIR)/src/libVNF-master: $(SRCDIR)/Makefile
#	sed -i 's|#define HAVE_EPOLL 1|/* no HAVE_EPOLL */|g' src/src/config.h
#	$(MAKE) -C $(SRCDIR)
#else
#$(SRCDIR)/src/libVNF-master: $(SRCDIR)/Makefile
#	$(MAKE) -C $(SRCDIR)
#endif

################################ b MANIFEST ###############################


b-kernelbypass-dynamic.manifest: b-kernelbypass-dynamic.manifest.template
	sed -e 's|$$(GRAPHENEDIR)|'"$(GRAPHENEDIR)"'|g' \
		-e 's|$$(GRAPHENEDEBUG)|'"$(GRAPHENEDEBUG)"'|g' \
		-e 's|$$(ARCH_LIBDIR)|'"$(ARCH_LIBDIR)"'|g' \
		$< > $@


b-kernelbypass-dynamic.manifest.sgx: b-kernelbypass-dynamic.manifest $(SRCDIR)/Examples/libVNF_ab/b-kernelbypass-dynamic
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-sign \
		-libpal $(GRAPHENEDIR)/Runtime/libpal-Linux-SGX.so \
		-key $(SGX_SIGNER_KEY) \
		-manifest $< -output $@ \
		-exec  $(SRCDIR)/Examples/libVNF_ab/b-kernelbypass-dynamic
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-get-token \
		-output b-kernelbypass-dynamic.token -sig b-kernelbypass-dynamic.sig


pal_loader:
	ln -s $(GRAPHENEDIR)/Runtime/pal_loader $@

############################## RUNNING TESTS ##################################
.PHONY: start-native-server
start-native-server: all
	./b-kernelbypass-dynamic --save '' --protected-mode no

.PHONY: start-graphene-server
start-graphene-server: all
	./pal_loader b-kernelbypass-dynamic --save '' --protected-mode no

################################## CLEANUP ####################################

.PHONY: clean
clean:
	$(RM) *.token *.sig *.manifest.sgx *.manifest pal_loader  *.rdb

.PHONY: distclean
distclean: clean
	$(RM) *.token *.sig *.manifest.sgx *.manifest pal_loader
