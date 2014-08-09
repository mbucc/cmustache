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
split()
{
	char		*parent = 0;
	char		*child = 0;
	int		rval = 0;


	rval = split_key("parent.child", &parent, &child);
	ok(!rval, "split_key returned %d", rval);
	is(parent, "parent");
	is(child, "child");
	free(parent);
}

void
get_nosection()
{
	char		*val = 0;
	int		rval = 0;

	rval = get("{\"a\": 1}", "a", &val);

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

	rval = get(json, "a", &val);
	ok(!rval, "rval is %d", rval);
	is(val, "{\"b\": 2}");
	free(val);

	rval = get(json, "a.b", &val);
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

int
main (int argc, char *argv[])
{

	plan(11);

	split();
	get_nosection();
	get_section();
	valcpy_trim();

	done_testing();
}
