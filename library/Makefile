include ../config.mk

.PHONY : all install clean

all : $(full_libname)

$(full_libname) : lecroy_vxi11.o
	$(CXX) ${LDFLAGS} -shared -Wl,-soname,$(full_libname) $^ -o $@ -lvxi11

lecroy_vxi11.o: lecroy_vxi11.c lecroy_vxi11.h
	$(CXX) -fPIC $(CFLAGS) -c $< -o $@

TAGS: $(wildcard *.c) $(wildcard *.h)
	etags $^

clean:
	rm -f *.o $(full_libname) TAGS

install: all
	$(INSTALL) -d $(DESTDIR)$(prefix)/lib${LIB_SUFFIX}/
	$(INSTALL) $(full_libname) $(DESTDIR)$(prefix)/lib${LIB_SUFFIX}/
	ln -sf $(full_libname) $(DESTDIR)$(prefix)/lib${LIB_SUFFIX}/$(libname)
	$(INSTALL) -d $(DESTDIR)$(prefix)/include/
	$(INSTALL) lecroy_vxi11.h $(DESTDIR)$(prefix)/include/

