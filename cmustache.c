#include <err.h>
#include <sys/errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "js0n.h"
#include "j0g.h"

#include "cmustache.h"

#define BUFSZ_DELTA	10240
#define MAX_TAGSZ	1024


		/*
		 * Add -DDEUG to CFLAGS in Makefile to turn on debugging output.
		 */
#ifdef DEBUG
#define DEBUG_PRINT 1
#else
#define DEBUG_PRINT 0
#endif

#define debug_printf(fmt, ...) \
        do { if (DEBUG_PRINT) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)


// Compute the size of an array
// that is large enough to hold one offset and one length value
// for each key and each value in the JSON.
// Allocate the array.
// Return the array and it's size.
int
size_index(const char *json, unsigned short **indexp, unsigned int *iszp)
{
	const char	*p;
	int		rval = 0;
	char		c = ',';

		/*
		 * (one key + one value) x (one offset + one length) = 4
		 */

	unsigned int entriesPerComma = 4;

		/*
		 * We need at least one extra slot (js0n zero-terminates
		 * the array) plus four for the last key/value pair.
		 * Add some more for safety ...
		 */

	unsigned int extra = 21;

		/*
		 * The number of commas in JSON is an upper limit
		 * on the number of key/value pairs - 1.
		 */

	*iszp = 0;
	for(p = json; *p && !rval; p++) {
		if (*iszp >= UINT_MAX)
			rval = EX_TOO_MANY_KEYVAL_PAIRS;
		*iszp += (*p == c);
	}

		/*
		 * The array holds short ints, but the array index
		 * is an unsigned int, so we check against UINT_MAX.
		 */

	if (!rval) {
		if (*iszp + extra > UINT_MAX / entriesPerComma)
			rval = EX_TOO_MANY_KEYVAL_PAIRS;

		*iszp *= entriesPerComma;
		*iszp += extra;
	}

	if (!rval)
		if ((*indexp = calloc(*iszp, sizeof(unsigned int))) == NULL)
			rval = ENOMEM;

	return rval;

}


// Allocate and load the set of offset and length pairs
// for the key/values in the given JSON.  Zero-terminate
// the array.
int
index_json(const char *json, unsigned short **indexp)
{
	int		rval = 0;
	unsigned int	isz;

	rval = size_index(json, indexp, &isz);

	if (!rval) {

		rval = js0n((const unsigned char *) json, strlen(json), *indexp, isz);

		if (rval)

			rval = EX_JSON_PARSE_ERROR;
	}

	return rval;

}

// In-place modification of JSON string.
// Find key, and stick a null at the end of it's value.
// Update *val to point to the start of the value.
// If key not found, *val is set to NULL.
int
get(char *json, unsigned short *index, const char *key, char **val)
{
	int		idx = 0;
	int		rval = 0;
	unsigned short	offset = 0;
	unsigned short	length = 0;

	idx = j0g_val(key, json, index);

	*val = 0;
	if (idx > 0) {
		offset = index[idx];
		length = index[idx + 1];
		json[offset + length] = 0;
		*val = json + offset;
	}

	debug_printf("\"%s\" returns \"%s\"\n", key, *val);

	return rval;

}


// Look up value in JSON for the given key, and insert it into the result.
int
insert_tag(char *key, char **qhtml, char *json, unsigned short *index)
{
	int 		rval = 0;
	char		*val;

	rval = get(json, index, key, &val);

	debug_printf("*qhtml = \"%s\"\n", val);

	if (!rval) {

		strcpy(*qhtml, val);

		*qhtml += strlen(*qhtml);

	}

	return rval;
}

// Given a mustache template and some JSON, render the HTML.
int
render(const char *template, char *json, char **html)
{
	char		tag[MAX_TAGSZ] = {0};
	char		*dbgfmt = "\t\t--> %s (%s)";
	const char	*cur= 0;
	char		prev;
	char		*qhtml = 0;
	char		*qtag = 0;
	unsigned short	*index = 0;
	int		rval = 0;

	debug_printf("%s\n", "Starting to render");

		/*
		 * The states.
		 */


	static void *gohtml[] =
	{
		[0 ... 122]	= &&l_html,
		[ '{' ]		= &&l_tagp,
		[124 ... 255]	= &&l_html
	};

	static void *gotagp[] =
	{
		[0 ... 122]	= &&l_no_tag,
		[ '{' ]		= &&l_yes_tag,
		[124 ... 255]	= &&l_no_tag
	};

	static void *gotag[] =
	{
		[0 ... 124]		= &&l_tag,
		[ '}' ]			= &&l_htmlp,
		[126 ... 255]	= &&l_tag
	};

	static void *gohtmlp[] =
	{
		[0 ... 124]	= &&l_no_html,
		[ '}' ]		= &&l_yes_html,
		[126 ... 255]	= &&l_no_html
	};

		/*
		 * Index the JSON.
		 */

	rval = index_json(json, &index);

		/*
		 * Allocate memory to hold HTML.
		 */

	if (!rval) {

		*html = (char *) calloc(BUFSZ_DELTA, 1);

		if  ( *html == NULL)
			rval = ENOMEM;

		qhtml = *html;
		qtag = tag;
	}

		/*
		 * Start in the HTML state.
		 */

	void **go = gohtml;

		/*
		 * Process template, one character at a time.
		 */


	for(cur = template; *cur && !rval; cur++)
	{
		debug_printf("%c\n", *cur);
		goto *go[(unsigned char) *cur];
		l_loop:;
		prev = *cur;

	}

	free(index);

	return rval;

		/*
		 * The state transitions.
		 */

	l_html:
		*qhtml++ = *cur;
		goto l_loop;



	l_tag:
		if (qtag - tag >= MAX_TAGSZ - 1)
			rval = EX_TAG_TOO_LONG;
		else
			*qtag++ = *cur;
		goto l_loop;


	l_tagp:
		go = gotagp;
		debug_printf("\t\t--> %s (%s)\n", "gotagp", "l_tagp");
		goto l_loop;

	l_no_tag:
		*qhtml++ = prev;
		*qhtml++ = *cur;
		go = gohtml;
		debug_printf("\t\t--> %s (%s)\n", "gohtml", "l_no_tag");
		goto l_loop;

	l_yes_tag:
		go = gotag;
		debug_printf("\t\t--> %s (%s)\n", "gotag", "l_yes_tag");
		goto l_loop;



	l_htmlp:
		go = gohtmlp;
		debug_printf("\t\t--> %s (%s)\n", "gohtmlp", "l_htmlp");
		goto l_loop;

	l_no_html:
		*qtag++ = prev;
		*qtag++ = *cur;
		go = gotag;
		debug_printf("\t\t--> %s (%s)\n", "gotag", "l_no_html");
		goto l_loop;

	l_yes_html:
		go = gohtml;
		debug_printf("\t\t--> %s (%s)\n", "gohtml", "l_yes_html");
		debug_printf("before insert, html = \"%s\"\n", html);
		insert_tag(tag, &qhtml, json, index);
		debug_printf("after insert, html = \"%s\"\n", html);
		memset(tag, 0, MAX_TAGSZ);
		goto l_loop;


}
