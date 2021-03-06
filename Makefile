VERSION=1.00

MAKE=make
DIRS=library utils

.PHONY : all clean install

all :
	for d in ${DIRS}; do $(MAKE) -C $${d}; done

clean:
	for d in ${DIRS}; do $(MAKE) -C $${d} clean; done

install:
	for d in ${DIRS}; do $(MAKE) -C $${d} install; done
