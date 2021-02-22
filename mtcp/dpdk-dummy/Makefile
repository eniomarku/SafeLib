libdpdkdummy.so.1: dpdk_dummy.c
	gcc -fPIC -shared -Wl,-soname,libdpdkdummy.so.1 -o $@ $^
	ln -s libdpdkdummy.so.1 libdpdkdummy.so

clean:
	rm -rf libdpdkdummy.so*

distclean: clean
