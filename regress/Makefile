# Run unit tests
# @since Sat Aug  2 10:36:15 EDT 2014
# @author Mark Bucciarelli <mkbucc@gmail.com>
# ISC license.

D=../deps
J=${D}/js0n
T=${D}/tap.c

all: test
	./test

test: test.c ${T}/tap.c ${T}/tap.h
	gcc -o test test.c ${T}/tap.c

clean:
	rm -f test