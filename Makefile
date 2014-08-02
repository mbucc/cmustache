# Build and test mustache.c
# @since Sat Aug  2 11:58:58 EDT 2014
# @author Mark Bucciarelli <mkbucc@gmail.com>

CCFLAGS=-Wall

#---------------------------------------------------
#
#                                     Dependencies
#
# XXX: clibs has dependency mechanism; use it.
#
#---------------------------------------------------

dep: deps/tap.c deps/js0n

deps/tap.c:
	clib install thlorenz/tap.c   

deps/js0n:
	clib install mbucc/js0n


#---------------------------------------------------
#
#                                   Mustache specifications
#
#---------------------------------------------------

GITHUB=https://raw.githubusercontent.com
SPEC=${GITHUB}/mustache/spec/master

spec: specs/interpolation.json

specs/%.json:
	curl ${SPEC}/$@ > t
	mv t $@

showspec: specs/interpolation.json
	cat specs/interpolation.json | python -mjson.tool

#---------------------------------------------------
#
#                                                Tests
#
#---------------------------------------------------

test: dep
	(cd regress ; make)



#---------------------------------------------------
#
#                                                Other
#
#---------------------------------------------------

clean:
	(cd regress ; make clean)

	



