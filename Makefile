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

dep: deps/js0n deps/chtmlescape

deps/js0n:
	clib install mbucc/js0n

deps/chtmlescape:
	clib install mbucc/chtmlescape

#---------------------------------------------------
#
#                                   Mustache specifications
#
#---------------------------------------------------

GITHUB=https://raw.githubusercontent.com
SPEC=${GITHUB}/mustache/spec/master

spec: specs/interpolation.json specs/sections.json

specs/%.json:
	curl ${SPEC}/$@ > t
	mv t $@

CURRENT=specs/sections.json
showspec: ${CURRENT}
	cat ${CURRENT} | python -mjson.tool

#---------------------------------------------------
#
#                                                Tests
#
#---------------------------------------------------

test: dep
	(cd regress ; make)

#---------------------------------------------------
#
#                                        Documentation
#
#---------------------------------------------------

docs:
	(cd doc ; make)


#---------------------------------------------------
#
#                                                Other
#
#---------------------------------------------------

clean:
	(cd regress ; make clean)

	



