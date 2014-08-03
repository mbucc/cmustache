# Run unit tests
# @since Sat Aug  2 10:36:15 EDT 2014
# @author Mark Bucciarelli <mkbucc@gmail.com>
# ISC license.

D=../deps
J=${D}/js0n
T=${D}/tap.c
V=deps/vec

CC=gcc

CFLAGS=-Wall -I../deps/js0n -I../deps/tap.c  -I./deps/vec

all: interpolation


interpolation: test
	./test '../specs/interpolation.json'


test: dep test.c test.h ${T}/tap.c ${T}/tap.h ${J}/js0n.c ${J}/j0g.c ${V}/vec.c ../cmustache.h ../cmustache.c
	$(CC) $(CFLAGS) -o test test.c ${T}/tap.c ${J}/js0n.c ${J}/j0g.c ${V}/vec.c ../cmustache.c

dep: ${V}

${V}:
	clib install rxi/vec


clean:
	rm -f test