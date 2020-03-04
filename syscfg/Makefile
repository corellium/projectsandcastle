CFLAGS = -O2 -Wall -I.

syscfg: main.o syscfg.o
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBRARIES)

clean:
	rm -f main.o syscfg.o syscfg
