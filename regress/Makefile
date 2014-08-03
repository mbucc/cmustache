# Run unit tests
# @since Sat Aug  2 10:36:15 EDT 2014
# @author Mark Bucciarelli <mkbucc@gmail.com>
# ISC license.

D=../deps
J=${D}/js0n
T=${D}/tap.c
V=deps/vec

CCFLAGS=-Wall


all: dep interpolation
	./interpolation

interpolation: interpolation.c ${T}/tap.c ${T}/tap.h test.c test.h  ${J}/js0n.c ${J}/j0g.c ${V}/vec.c
	gcc -o interpolation interpolation.c ${T}/tap.c ${J}/js0n.c ${J}/j0g.c ${V}/vec.c test.c 

dep: ${V}

${V}:
	clib install rxi/vec


clean:
	rm -f interpolation