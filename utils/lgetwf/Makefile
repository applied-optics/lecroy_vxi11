CFLAGS = -g

all:	lgetwf
.PHONY:	all clean install

lgetwf: lgetwf.o lecroy_user.o vxi11_user.o vxi11_clnt.o vxi11_xdr.o
	g++ $(CFLAGS) -o lgetwf lgetwf.o lecroy_user.o vxi11_user.o vxi11_clnt.o vxi11_xdr.o

lgetwf.o: lgetwf.c ../../library/lecroy_user.c ../../../vxi11/vxi11_user.cc ../../../vxi11/vxi11.h
	g++ $(CFLAGS) -c lgetwf.c -o lgetwf.o

lecroy_user.o:	../../library/lecroy_user.c
	g++ $(CFLAGS) -c ../../library/lecroy_user.c -o lecroy_user.o

vxi11_user.o: ../../../vxi11/vxi11_user.cc ../../../vxi11/vxi11.h
	g++ $(CLFAGS) -c ../../../vxi11/vxi11_user.cc -o vxi11_user.o 

vxi11_clnt.o: ../../../vxi11/vxi11_clnt.c
	g++ $(CLFAGS) -c ../../../vxi11/vxi11_clnt.c -o vxi11_clnt.o

vxi11_xdr.o: ../../../vxi11/vxi11_xdr.c
	g++ $(CLFAGS) -c ../../../vxi11/vxi11_xdr.c -o vxi11_xdr.o

clean:
	rm -f *.o test* lgetwf

install:
	cp -f lgetwf /usr/local/bin

