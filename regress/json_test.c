// Unit test JSON functions.
// @since Thu Aug  7 22:38:22 EDT 2014
// @author Mark Bucciarelli <mkbucc@gmail.com>


#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include "deps/vec/vec.h"

#include "j0g.h"
#include "js0n.h"
#include "tap.h"

#include "../cmustache.h"

void
get_nosection()
{
	char		*val = 0;
	int		rval = 0;

	rval = get("{\"a\": 1}", 0, "a", &val);

	ok(!rval, "Calling get with no section returned %d", rval);
	is(val, "1");
	free(val);
}

void
get_section()
{
	char		*val = 0;
	char		*json = "{\"a\": {\"b\": 2}}";
	int		rval = 0;

	rval = get(json, "a", "b", &val);
	ok(!rval, "rval is %d", rval);
	is(val, "2");
	free(val);

}

void
valcpy_trim()
{
	char		*val = 0;
	char		*json = "{\"a\": \"   b   \"}";
	int		rval = 0;

	rval = valcpy(json, "a", &val);
	ok(!rval, "rval is %d", rval);
	is(val, "b");
	free(val);

}


// 
// From https://github.com/bobthecow/mustache.php/wiki/Variable-Resolution:
//
//	dot notation resolution differs from section context resolution.
//		dot notation: foo.bar.baz
//				-->	if bar not in foo, return empty string.
//					if baz not in bar, return empty string.
//
//		sections: {{#foo}}{{#bar}}{{baz}}{{/bar}}{{/foo}}
//				-->	if bar not in foo or global context, return empty string.
//					if baz not in bar, foo, or global context, return empty string.
//
// 	Both sections and dotted notation start at the deepest context.
//	But the behavior diverges if the key is not found there.
//
//	The dotted notation returns empty string.
//	The section context pops up through the levels one-by-one,
//	stopping either when it finds the key or runs out of levels.
//


void
resolve_section()
{
	char		*val = 0;
	char		*json = "{\"a\": {\"one\": 1}, \"b\": {\"two\": 2}}";
	int		rval = 0;

	rval = get(json, "a", "one", &val);
	ok(!rval, "rval is %d", rval);
	is(val, "1");

	free(val);

}


void
resolve_nested_section()
{
	char		*val = 0;
	char		*json = "{\"a\": {\"one\": 1}, \"b\": {\"two\": 2}}";
	int		rval = 0;

	rval = get(json, "a.b", "two", &val);
	ok(!rval, "rval is %d", rval);
	is(val, "2");

	free(val);

}

void
jsonpath_nodot()
{
	char		*json = "{\"a\": {\"one\": 1}, \"b\": {\"two\": 2}}";
	int		rval = 0;
	unsigned short offset;
	unsigned short length;

	rval = jsonpath(json, strlen(json), "a", &offset, &length);
	ok(!rval, "rval is %d", rval);
	cmp_ok(offset, "==", 6);
	cmp_ok(length, "==", 10);
}

void
jsonpath_drilldown1()
{
	char		*json = "{\"a\": {\"one\": 1}, \"b\": {\"two\": 2}}";
	int		rval = 0;
	unsigned short offset;
	unsigned short length;

	rval = jsonpath(json, strlen(json), "a.one", &offset, &length);
	ok(!rval, "rval is %d", rval);
	cmp_ok(offset, "==", 18);
	cmp_ok(length, "==", 1);
}



int
main (int argc, char *argv[])
{
	plan(6);

	jsonpath_nodot();

	jsonpath_drilldown1();

/*
	skip(1, 6);

	get_nosection();	// 2
	get_section();		// 2
	valcpy_trim();		// 2

	end_skip;

	resolve_section();		// 2
	// resolve_nested_section();	// 2
*/
	

	done_testing();
}
