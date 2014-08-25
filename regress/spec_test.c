// Unit test utility functions.
// @since Sat Aug  2 13:27:34 EDT 2014
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

#include "spec_test.h"


long
filesize( const char *filename, FILE *fp)
{
	long		rval;

	if (fseek(fp, 0, SEEK_END) != 0)
		err(errno, "Can't seek to end of %s", filename);

	rval = ftell(fp);

	if (fseek(fp, 0L, SEEK_SET) != 0)
		err(errno, "Can't seek back to beginning of %s", filename);

	return rval;
}

char *
ftos(const char *filename)
{
	char		*json;
	long		sz;
	FILE		*fp;

	if ((fp = fopen(filename, "r")) == NULL)
		err(EX_NOINPUT, "Can't read %s", filename);

	sz = filesize(filename, fp);

		/*
		 * js0n stores offset's as unsigned shorts, so we can
		 * only handle files with that many bytes.
		 */

	if (sz > USHRT_MAX)
		errx(EX_DATAERR, "JSON file is larger than %d", USHRT_MAX);

	if ((json = calloc(sz, 1)) == NULL)
		err(ENOMEM, "Can't allocate %lu bytes", sz);

	if (fread(json, 1, sz, fp) < sz)
		err(errno, "Error reading %s", filename);

	fclose(fp);

	if (json[sz - 1] != 0) {
		char *json2 = calloc(sz + 1, 1);
		if (!json2)
			err(ENOMEM, "Can't allocate %lu bytes", sz + 1);
		memcpy(json2, json, sz);
		free(json);
		json2[sz] = 0;
		json = json2;
	}

	return json;
}

struct test *
get_test(char *tests, unsigned short *index)
{
	struct test *rval = 0;
	char *testjson = 0;
	unsigned short	offset = 0;
	unsigned short	length = 0;

	offset = *index;
	length = *(index + 1);

	tests[offset + length] = 0;
	testjson = tests + offset;

	if ( (rval = calloc(1, sizeof(*rval) ) ) == NULL )
		err(ENOMEM, "can't allocate a struct test");

	/*
	 * This is what the tests section of a Mustache spec looks like:
	 *
	 *	[
	 *	        {
	 *	            "data": {},
	 *	            "desc": "Mustache-free templates should render as-is.",
	 *	            "expected": "Hello from {Mustache}!\n",
	 *	            "name": "No Interpolation",
	 *	            "template": "Hello from {Mustache}!\n"
	 *	        },
	 *		...
	 *	]
	 *
	 */

	get(testjson, strlen(testjson), 0, 0, "data", &rval->json);
	get(testjson, strlen(testjson), 0, 0, "template", &rval->template);
	get(testjson, strlen(testjson), 0, 0, "expected", &rval->expected);
	get(testjson, strlen(testjson), 0, 0, "desc", &rval->description);

	return rval;

}

test_vec_t
parse_tests(char *json)
{
	test_vec_t tests_vec;
	unsigned short	*index = 0;
	char *tests = 0;
	int rval = 0;

	vec_init(&tests_vec);

	rval = get(json, strlen(json), 0, 0, "tests", &tests);

	if (rval)
		errx(rval, "Failed to parse test JSON, get returned error %d", rval);

	if (!tests)
		return tests_vec;

	index_json(tests, strlen(tests), &index);

	for (unsigned short *i = index; *i; i += 2)
		vec_push(&tests_vec, get_test(tests, i));

	free(index);

	return tests_vec;

}


test_vec_t
get_tests(const char *spec_file)
{
	test_vec_t	rval;
	char 			*json = NULL;

	json = ftos(spec_file);

	rval = parse_tests(json);

	free(json);

	return rval;
}

const char *
dumptest(struct test *t)
{
	char			*buf;
	size_t		sz = 0;

	sz = strlen(t->json);
	sz += strlen(t->template);
	sz += strlen(t->expected);
	sz += strlen(t->description);
	sz += 200;

	buf = calloc(sz, 1);
	if (!buf)
		err(ENOMEM, "out of memory");

	sprintf(buf, "<struct test:\ndescription=%s\ntemplate='%s'\njson='%s'\nexpected='%s'>",
		t->description, t->template, t->json, t->expected);

	return buf;
}


int
main (int argc, char *argv[])
{
	struct test	*test;
	char		*result;
	int		i, test_number = 0;
	int		rval = 0;

	test_vec_t tests = get_tests(argv[1]);

	plan(tests.length * 2);

	vec_foreach(&tests, test, i) {

		/*
		 * Test numbers output by tap are one-based.
		 */
		test_number = i + 1;

		if (test_number <100 ) {

			rval = render( test->template, test->json, &result );

			ok(!rval, "%s returned %d", test->description, rval)
				|| printf("# %s\n", dumptest(test));

			is(result, test->expected, test->description)
				|| printf("# %s\n", dumptest(test));

		}

		else {

			skip(1, 2);

			ok(0);
			ok(0);

			end_skip;

		}


	}

	done_testing();
}
