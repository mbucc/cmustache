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
#include "queue.h"
#include "tap.h"

#include "../cmustache.h"

void
test_trim()
{
	char	*s = "     a     ";
	unsigned short	offset = 0;
	unsigned short length = strlen(s);

	trim(s, &offset, &length);

	cmp_ok(offset, "==", 5);
	cmp_ok(length, "==", 1);
}


void
jsonpath_trim()
{
	char		*json = "{\"a\": \"   b   \"}";
	int		rval = 0;
	unsigned short offset;
	unsigned short length;

	rval = jsonpath(json, strlen(json), "a", &offset, &length);
	ok(!rval, "rval is %d", rval);
	cmp_ok(offset, "==", 10);
	cmp_ok(length, "==", 1);
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
//
//	The section context pops off the outer-most section
//	("a" in this example)
//	and then tries again in the shortened context.
//	This repeats until you run out of contexts to pop off
//	or you find the key,
//	whichever comes first.
//


void
resolve_section()
{
	char		 section[][MAX_KEYSZ] = { { "a" }, { 0 } };
	char		*val = 0;
	char		*json = "{\"a\": {\"one\": 1}, \"b\": {\"two\": 2}}";
	int		rval = 0;

	
	rval = get(json, strlen(json), section, 1, "one", &val);
	ok(!rval, "rval is %d", rval);
	is(val, "1");

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
	cmp_ok(offset, "==", 14);
	cmp_ok(length, "==", 1);
}

void
jsonpath_drilldown2()
{
	char		*json = "{\"a\": {\"one\": {\"two\": \"abcdefg\" }, \"b\": {\"two\": 2} } }";
	int		rval = 0;
	unsigned short offset;
	unsigned short length;

	rval = jsonpath(json, strlen(json), "a.one.two", &offset, &length);
	ok(!rval, "rval is %d", rval);
	cmp_ok(offset, "==", 23);
	cmp_ok(length, "==", 7);
}

void
jsonpath_fail_lookup()
{
	char		*json = "{\"a\": {\"one\": {\"two\": \"abcdefg\" }, \"b\": {\"two\": 2} } }";
	int		rval = 0;
	unsigned short offset;
	unsigned short length;

	rval = jsonpath(json, strlen(json), "a.one.two.three", &offset, &length);
	ok(!rval, "rval is %d", rval);
	cmp_ok(offset, "==", 0);
	cmp_ok(length, "==", 0);
}

void
jsonpath_all_nulls()
{
	int rval = 0;

	rval = jsonpath(0, 0, 0, 0, 0);
	ok(rval == EX_LOGIC_ERROR, "rval is %d", rval);
}

void
jsonpath_null_json()
{
	int		rval = 0;
	unsigned short offset;
	unsigned short length;

	rval = jsonpath(0, 10, "a.one.two.three", &offset, &length);
	ok(!rval, "rval is %d", rval);
	cmp_ok(offset, "==", 0);
	cmp_ok(length, "==", 0);
}

void
jsonpath_null_key()
{
	char		*json = "{\"a\": {\"one\": {\"two\": \"abcdefg\" }, \"b\": {\"two\": 2} } }";
	int		rval = 0;
	unsigned short offset;
	unsigned short length;

	rval = jsonpath(json, strlen(json), 0, &offset, &length);
	ok(!rval, "rval is %d", rval);
	cmp_ok(offset, "==", 0);
	cmp_ok(length, "==", 0);
}

void
jsonpath_key_with_dot()
{
	char		*json = "{\"a\": {\"one.two\": 1}, \"b\": {\"two\": 2}}";
	int		rval = 0;
	unsigned short offset;
	unsigned short length;

	rval = jsonpath(json, strlen(json), "a.one.two", &offset, &length);
	ok(!rval, "rval is %d", rval);
	cmp_ok(offset, "==", 18);
	cmp_ok(length, "==", 1);
}

void
parsejson1()
{
	struct json j = {0};
	char	*json = "{\"a\": 1}";
	struct jsonpair *jp = 0;
	int	rval = 0;
	int	n = 0;

	rval = parsejson(json, &j);
	ok(!rval, "rval is %d", rval);

	SLIST_FOREACH(jp, &j, children)
		n++;
	cmp_ok(n, "==", 1);

	jp = SLIST_FIRST(&j);
	cmp_ok(jp->type, "==", number_type);
}


int
main (int argc, char *argv[])
{
	jsonpath_nodot();

	jsonpath_drilldown1();

	jsonpath_drilldown2();

	jsonpath_fail_lookup();

	jsonpath_all_nulls();

	jsonpath_null_json();

	jsonpath_null_key();

	jsonpath_key_with_dot();

	jsonpath_trim();

	test_trim();

	resolve_section();

	parsejson1();
	
	done_testing();

}
