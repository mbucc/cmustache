# Run unit tests
# @since Sat Aug  2 10:36:15 EDT 2014
# @author Mark Bucciarelli <mkbucc@gmail.com>
# ISC license.

D=../deps
J=${D}/js0n
E=${D}/chtmlescape
Q=${D}/cqueue
V=./deps/vec
T=./deps/tap.c

CC=gcc

CFLAGS=-Wall -I${J} -I${E} -I${T} -I${V} -I${Q} #-DDEBUG

resolution: spec_test
	./spec_test '../specs/resolution.json'

all: sections #json interpolation

json: json_test
	./json_test

json_test: json_test.c ../cmustache.c ../cmustache.h ${T}/tap.c ${T}/tap.h ${J}/js0n.c ${J}/j0g.c 
	$(CC) $(CFLAGS) -o json_test json_test.c ../cmustache.c ${T}/tap.c ${J}/js0n.c ${J}/j0g.c ${E}/htmlescape.c


interpolation: spec_test
	./spec_test '../specs/interpolation.json'

sections: spec_test
	./spec_test '../specs/sections.json'

spec_test: dep spec_test.c spec_test.h ../cmustache.h ../cmustache.c ${T}/tap.c ${T}/tap.h ${J}/js0n.c ${J}/j0g.c ${V}/vec.c ${E}/htmlescape.c
	$(CC) $(CFLAGS) -o spec_test spec_test.c ../cmustache.c ${T}/tap.c ${J}/js0n.c ${J}/j0g.c ${V}/vec.c ${E}/htmlescape.c

dep: ${V} ${T}

${V}:
	clib install rxi/vec

${T}:
	clib install thlorenz/tap.c   

clean:
	rm -f spec_test json_test
