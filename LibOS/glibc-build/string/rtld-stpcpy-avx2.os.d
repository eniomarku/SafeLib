$(common-objpfx)string/rtld-stpcpy-avx2.os: \
 ../sysdeps/x86_64/multiarch/stpcpy-avx2.S ../include/stdc-predef.h \
 $(common-objpfx)libc-modules.h \
 ../include/libc-symbols.h \
 $(common-objpfx)config.h \
 ../sysdeps/generic/symbol-hacks.h \
 ../sysdeps/x86_64/multiarch/strcpy-avx2.S

../include/stdc-predef.h:

$(common-objpfx)libc-modules.h:

../include/libc-symbols.h:

$(common-objpfx)config.h:

../sysdeps/generic/symbol-hacks.h:

../sysdeps/x86_64/multiarch/strcpy-avx2.S:
