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

#include "test.h"


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

char *
get(char *json, unsigned short *index, const char *key)
{
	char			*rval = 0;
	int			idx = 0;
	unsigned short	offset = 0;
	unsigned short	length = 0;

	idx = j0g_val(key, json, index);

	if (idx > 0) {
		offset = index[idx];
		length = index[idx + 1];
		json[offset + length] = 0;
		rval = json + offset;
	}

	return rval;

}

struct test *
get_test(char *tests, unsigned short *index)
{
	struct test *rval = 0;
	char *testjson = 0;
	unsigned short	*testindex = 0;
	unsigned short	offset = 0;
	unsigned short	length = 0;

	offset = *index;
	length = *(index + 1);

	tests[offset + length] = 0;
	testjson = tests + offset;

	index_json(testjson, &testindex);

	if (! *testindex)
		errx(1, "Error parsing %s\n", testjson);

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

	rval->json = get(testjson, testindex, "data");
	rval->template = get(testjson, testindex, "template");
	rval->expected = get(testjson, testindex, "expected");
	rval->description = get(testjson, testindex, "desc");

	free(testindex);

	return rval;

}

test_vec_t
parse_tests(char *json) 
{
	test_vec_t rval;
	unsigned short	*index = 0;
	char *tests = 0;

	index_json(json, &index);

	tests = get(json, index, "tests");

	index_json(tests, &index);

	vec_init(&rval);

	for (unsigned short *i = index; *i; i += 2) {

		vec_push(&rval, get_test(tests, i));
	}

	free(index);

	return rval;
	
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


int
main (int argc, char *argv[])
{
	struct test	*test;
	char		*result;
	int		i = 0;

	test_vec_t tests = get_tests(argv[1]);

	plan(tests.length);

	vec_foreach(&tests, test, i) {

		if (i < 2) {

			render( test->template, test->json, &result );

			is(result, test->expected, test->description);

		}

		else {

			skip(1, 1);

			ok(0);

			end_skip;

		}


	}

	done_testing();
}