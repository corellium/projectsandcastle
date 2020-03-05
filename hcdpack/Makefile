CFLAGS = -O2 -Wall -I.

hcdpack: hcdpack.o
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBRARIES)

clean:
	rm -f hcdpack.o hcdpack
