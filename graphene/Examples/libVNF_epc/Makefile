

################################# CONSTANTS ###################################

# Relative path to Graphene root
GRAPHENEDIR = ../..
SGX_SIGNER_KEY ?= $(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/enclave-key.pem

SRCDIR = libVNF-release-socc
COMMIT = master
TAR_SHA256 = 35839b9b8d2889e67e452aaccdd202797bfd0014

ifeq ($(DEBUG),1)
GRAPHENEDEBUG = inline
else
GRAPHENEDEBUG = none
endif

.PHONY: all
all:  mme.manifest pal_loader
ifeq ($(SGX),1)
all: mme.manifest.sgx
endif

include ../../Scripts/Makefile.configs

############################## MME EXECUTABLE ###############################


#ifeq ($(USE_SELECT),1)
#$(SRCDIR)/src/libVNF-master: $(SRCDIR)/Makefile
#	sed -i 's|#define HAVE_EPOLL 1|/* no HAVE_EPOLL */|g' src/src/config.h
#	$(MAKE) -C $(SRCDIR)
#else
#$(SRCDIR)/src/libVNF-master: $(SRCDIR)/Makefile
#	$(MAKE) -C $(SRCDIR)
#endif

################################ MME MANIFEST ###############################

# The template file contains almost all necessary information to run mme
# under Graphene / Graphene-SGX.

mme.manifest: mme.manifest.template
	sed -e 's|$$(GRAPHENEDIR)|'"$(GRAPHENEDIR)"'|g' \
		-e 's|$$(GRAPHENEDEBUG)|'"$(GRAPHENEDEBUG)"'|g' \
		-e 's|$$(ARCH_LIBDIR)|'"$(ARCH_LIBDIR)"'|g' \
		$< > $@

# Manifest for Graphene-SGX requires special "pal-sgx-sign" procedure. This
# procedure measures all mm dependencies (shared libraries and trusted
# files), measures mmm code/data pages, and adds measurements in the
# resulting manifest.sgx file (among other, less important SGX options).
#
# Additionally, Graphene-SGX requires EINITTOKEN and SIGSTRUCT objects (see
# SGX hardware ABI, in particular EINIT instruction). The "pal-sgx-get-token"
# script generates these objects and puts them in files .token and .sig
# respectively. Note that filenames must be the same as the executable/manifest
# name (i.e., "redis-server").

mme.manifest.sgx: mme.manifest $(SRCDIR)/examples/epc/mme/mme
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-sign \
		-libpal $(GRAPHENEDIR)/Runtime/libpal-Linux-SGX.so \
		-key $(SGX_SIGNER_KEY) \
		-manifest $< -output $@ \
		-exec  $(SRCDIR)/examples/epc/mme/mme
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-get-token \
		-output mme.token -sig mme.sig

########################### COPIES OF EXECUTABLES #############################

# mmm build process creates the final executable.
# Also, create a link to pal_loader for simplicity.

#mme: $(SRCDIR)/src/libVNF-master
#	cp $< $@

pal_loader:
	ln -s $(GRAPHENEDIR)/Runtime/pal_loader $@

############################## RUNNING TESTS ##################################
.PHONY: start-native-server
start-native-server: all
	./mme --save '' --protected-mode no

.PHONY: start-graphene-server
start-graphene-server: all
	./pal_loader mme --save '' --protected-mode no

################################## CLEANUP ####################################

.PHONY: clean
clean:
	$(RM) *.token *.sig *.manifest.sgx *.manifest pal_loader  *.rdb

.PHONY: distclean
distclean: clean
	$(RM) *.token *.sig *.manifest.sgx *.manifest pal_loader
