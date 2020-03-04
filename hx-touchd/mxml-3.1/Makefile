CFLAGS += -O2 -Wall

libmxml.a: mxml-attr.o mxml-entity.o mxml-file.o mxml-get.o mxml-index.o mxml-node.o mxml-private.o mxml-search.o mxml-set.o mxml-string.o
	@rm -f $@
	$(AR) crs $@ $^

clean:
	rm -f libmxml.a mxml-attr.o mxml-entity.o mxml-file.o mxml-get.o mxml-index.o mxml-node.o mxml-private.o mxml-search.o mxml-set.o mxml-string.o
