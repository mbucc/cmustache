// Unit test utility functions.
// @since Sat Aug  2 13:27:34 EDT 2014
// @author Mark Bucciarelli <mkbucc@gmail.com>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <sysexits.h>

#include "deps/vec/vec.h"

#include "../deps/js0n/js0n.h"
#include "../deps/js0n/j0g.h"

#include "test.h"

#define MAX_KEYVALUE_PAIRS		10000
#define MAX_INDEX_LEN			2 * MAX_KEYVALUE_PAIRS + 1

long
filesize( const char *name, FILE *fp)
{
	long		rval;

	if (fseek(fp, 0, SEEK_END) != 0)
		err(errno, "Can't seek to end of %s", name);

	rval = ftell(fp);

	if (fseek(fp, 0L, SEEK_SET) != 0)
		err(errno, "Can't seek back to beginning of %s", name);

	return rval;
}

unsigned char *
ftos(const char *name, unsigned int *len)
{
	unsigned char	*json;
	long			sz;
	FILE			*fp;
	
	if ((fp = fopen(name, "r")) == NULL)
		err(EX_NOINPUT, "Can't read %s", name);

	sz = filesize(name, fp);

	if ((json = calloc(sz + 1, 1)) == NULL)
		err(ENOMEM, "Can't allocate %lu bytes", sz + 1);

	if (fread(json, 1, sz, fp) < sz)
		err(errno, "Error reading %s", name);

	fclose(fp);

	if (sz > UINT_MAX - 1)
		err(EX_DATAERR, "JSON files is larger than %d", UINT_MAX);

	*len = (unsigned int) sz;
	
	return json;
}

unsigned int
count(const unsigned char *s, char c)
{
	unsigned int	n = 0;

	for(; *s; n += (*s++ == c)) ;

	return n;
}

char *
parse_tests(unsigned char *json, unsigned int len)
{
	unsigned int	ilen;
	unsigned short	*index;
	int			rval;

		/*
		 * If we size index array too small, js0n croaks.
		 */
	ilen = count(json, ',');
	ilen *= 2;
	if (ilen > MAX_INDEX_LEN)
		ilen = MAX_INDEX_LEN;
	ilen += 1;

	if ((index = calloc(ilen, sizeof(unsigned int))) == NULL)
		err(ENOMEM, "Can't allocate %u ints", ilen);

	rval = js0n(json, len, index, ilen);
	if (rval > 0) {
		json[51] = 0;
		err(EX_DATAERR, "Error %d parsing the JSON that starts: %s", rval, json);
	}

		/*
		 * XXX: think through the (char *) cast and UTF8.
		 */

	return j0g_str("tests", (char *) json, index);

}

test_vec_t
get_tests(const char *spec_file) 
{
	test_vec_t	rval;
	unsigned char	*json;
	char			*tests;
	unsigned int	len;

	vec_init(&rval);

	json = ftos(spec_file, &len);

	tests = parse_tests(json, len);

	return rval;
}
