#include <stdlib.h>

#include "../deps/tap.c/tap.h"
#include "../deps/js0n/js0n.h"
#include "../deps/js0n/j0g.h"

#include "deps/vec/vec.h"

#include "test.h"

int
main ()
{
	struct test *t;
	test_vec_t tests = get_tests("../specs/interpolation.json");

	plan(tests.length);
	
    ok(3 == 3);
/*
    is("fnord", "eek", "two different strings not that way?");
    ok(3 <= 8732, "%d <= %d", 3, 8732);
    like("fnord", "f(yes|no)r*[a-f]$");
    cmp_ok(3, ">=", 10);
*/
	done_testing();
}