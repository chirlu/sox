# makefile for gcc

# Uncomment the following if you need a specific compiler.
#CC=gcc

CFLAGS:=-O2 -Wall
LDFLAGS:=-s
LIBS:=-lm

all: model lmodel corr

corr: corr.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

model: model.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

lmodel: model.c
	$(CC) $(CFLAGS) -DLSAMPL $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f model lmodel corr *~ A B
