# makefile for gcc

# Uncomment the following if you need a specific compiler.
#CC=gcc

CFLAGS:=-O2 -Wall -L../
LDFLAGS:=-s
LIBS:=-lm

all: ding model lding lmodel corr

corr: corr.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

ding: ding.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

model: model.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

lding: ding.c
	$(CC) $(CFLAGS) -DLSAMPL $(LDFLAGS) -o $@ $< $(LIBS)

lmodel: model.c
	$(CC) $(CFLAGS) -DLSAMPL $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f ding lding model lmodel corr *~

