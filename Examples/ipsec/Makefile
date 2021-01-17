# Build Redis as follows:
#
# - make               -- create non-SGX no-debug-log manifest
# - make SGX=1         -- create SGX no-debug-log manifest
# - make SGX=1 DEBUG=1 -- create SGX debug-log manifest
#
# Any of these invocations clones Redis' git repository and builds Redis in
# default configuration and in the latest-to-date (5.0.5) version.
#
# By default, Redis uses poll/epoll mechanism of Linux. To build Redis with
# select, use `make USE_SELECT=1`. For correct re-builds, always clean up
# Redis source code beforehand via `make distclean`.
#
# Use `make clean` to remove Graphene-generated files and `make distclean` to
# additionally remove the cloned Redis git repository.

################################# CONSTANTS ###################################

# Relative path to Graphene root
GRAPHENEDIR = ../..
SGX_SIGNER_KEY ?= $(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/enclave-key.pem

SRCDIR = libVNF
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

############################## REDIS EXECUTABLE ###############################

# Redis is built as usual, without any changes to the build process (except to
# test select syscall instead of poll/epoll). The source is downloaded from the
# GitHub repo (5.0.5 tag) and built via `make`. The result of this build process
# is the final executable "src/redis-server".

#$(SRCDIR)/Makefile:
#	$(GRAPHENEDIR)/Scripts/download --output libVNF-master.tar.gz \
#	--sha256 $(TAR_SHA256) \
#	--url https://github.com/networkedsystemsIITB/libVNF.git
#	mkdir $(SRCDIR)
#	tar -C $(SRCDIR) --strip-components=1 -xf libVNF-master.tar.gz

#ifeq ($(USE_SELECT),1)
#$(SRCDIR)/src/libVNF-master: $(SRCDIR)/Makefile
#	sed -i 's|#define HAVE_EPOLL 1|/* no HAVE_EPOLL */|g' src/src/config.h
#	$(MAKE) -C $(SRCDIR)
#else
#$(SRCDIR)/src/libVNF-master: $(SRCDIR)/Makefile
#	$(MAKE) -C $(SRCDIR)
#endif

################################ REDIS MANIFEST ###############################

# The template file contains almost all necessary information to run Redis
# under Graphene / Graphene-SGX. We create redis.manifest (to be run under
# non-SGX Graphene) by simply replacing variables in the template file via sed.

b-kernelbypass-dynamic.manifest: b-kernelbypass-dynamic.manifest.template
	sed -e 's|$$(GRAPHENEDIR)|'"$(GRAPHENEDIR)"'|g' \
		-e 's|$$(GRAPHENEDEBUG)|'"$(GRAPHENEDEBUG)"'|g' \
		-e 's|$$(ARCH_LIBDIR)|'"$(ARCH_LIBDIR)"'|g' \
		$< > $@

# Manifest for Graphene-SGX requires special "pal-sgx-sign" procedure. This
# procedure measures all Redis dependencies (shared libraries and trusted
# files), measures Redis code/data pages, and adds measurements in the
# resulting manifest.sgx file (among other, less important SGX options).
#
# Additionally, Graphene-SGX requires EINITTOKEN and SIGSTRUCT objects (see
# SGX hardware ABI, in particular EINIT instruction). The "pal-sgx-get-token"
# script generates these objects and puts them in files .token and .sig
# respectively. Note that filenames must be the same as the executable/manifest
# name (i.e., "redis-server").

b-kernelbypass-dynamic.manifest.sgx: b-kernelbypass-dynamic.manifest ./b-kernelbypass-dynamic
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-sign \
		-libpal $(GRAPHENEDIR)/Runtime/libpal-Linux-SGX.so \
		-key $(SGX_SIGNER_KEY) \
		-manifest $< -output $@ \
		-exec  ./b-kernelbypass-dynamic
	$(GRAPHENEDIR)/Pal/src/host/Linux-SGX/signer/pal-sgx-get-token \
		-output b-kernelbypass-dynamic.token -sig b-kernelbypass-dynamic.sig

########################### COPIES OF EXECUTABLES #############################

# Redis build process creates the final executable as src/redis-server. For
# simplicity, copy it into our root directory.
# Also, create a link to pal_loader for simplicity.

#b-kernelbypass-dynamic: $(SRCDIR)/src/libVNF-master
#	cp $< $@

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
