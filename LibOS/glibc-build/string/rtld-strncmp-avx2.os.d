$(common-objpfx)string/rtld-strncmp-avx2.os: \
 ../sysdeps/x86_64/multiarch/strncmp-avx2.S ../include/stdc-predef.h \
 $(common-objpfx)libc-modules.h \
 ../include/libc-symbols.h \
 $(common-objpfx)config.h \
 ../sysdeps/generic/symbol-hacks.h \
 ../sysdeps/x86_64/multiarch/strcmp-avx2.S

../include/stdc-predef.h:

$(common-objpfx)libc-modules.h:

../include/libc-symbols.h:

$(common-objpfx)config.h:

../sysdeps/generic/symbol-hacks.h:

../sysdeps/x86_64/multiarch/strcmp-avx2.S:
