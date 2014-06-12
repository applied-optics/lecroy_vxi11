include ../../config.mk

.PHONY:	all clean install

CFLAGS:=$(CFLAGS) -I../../library

all:	lgetwf

lgetwf: lgetwf.o ../../library/$(full_libname)
	$(CXX) $(LDFLAGS) -o $@ $^ -lvxi11

lgetwf.o: lgetwf.c ../../library/$(full_libname)
	$(CXX) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o test* lgetwf

install: all
	$(INSTALL) lgetwf $(DESTDIR)$(prefix)/bin/

