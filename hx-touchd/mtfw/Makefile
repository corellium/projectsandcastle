CFLAGS += -O2 -Wall
LDFLAGS += -lmxml

libmtfw.a: qdict.o eplist.o syscfg.o mtfw.o
	@rm -f $@
	$(AR) crs $@ $^

testload: testload.o qdict.o eplist.o syscfg.o mtfw.o

clean:
	rm -f libmtfw.a testload.o qdict.o eplist.o syscfg.o mtfw.o testload
