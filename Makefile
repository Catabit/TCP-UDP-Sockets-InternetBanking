CC=gcc
LIBSOCKET=-lnsl
CCFLAGS=-Wall -g
SRV=server
SEL_SRV=server
CLT=client
UTL=util

all: $(SEL_SRV) $(CLT)

$(SEL_SRV):$(SEL_SRV).c $(UTL).h
	$(CC) -o $(SEL_SRV) $(LIBSOCKET) $(SEL_SRV).c $(UTL).h

$(CLT):	$(CLT).c $(UTL).h
	$(CC) -o $(CLT) $(LIBSOCKET) $(CLT).c $(UTL).h

clean:
	rm -f *.o *~
	rm -f $(SEL_SRV) $(CLT)


