LDFLAGS = -Lmxml-3.1 -Lmtfw
LIBRARIES = -lmxml -lmtfw -lpthread
CFLAGS = -O2 -Wall -I`pwd`/mxml-3.1 -I`pwd`/mtfw

hx-touchd: hx-touchd.o mtfw/libmtfw.a mxml-3.1/libmxml.a
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBRARIES)

mtfw/libmtfw.a:
	make -C mtfw CFLAGS="$(CFLAGS)" CC=$(CC) AR=$(AR)

mxml-3.1/libmxml.a:
	make -C mxml-3.1 CFLAGS="$(CFLAGS)" CC=$(CC) AR=$(AR)

clean:
	make -C mtfw clean
	make -C mxml-3.1 clean
	rm -f hx-touchd.o hx-touchd
