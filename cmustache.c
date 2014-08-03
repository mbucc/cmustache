#include <err.h>
#include <sys/errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "js0n.h"
#include "j0g.h"

#include "cmustache.h"

#define BUFSZ_DELTA	10240
#define MAX_TAGSZ	1024



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

		j0g(json, *indexp, isz);	
	
		if (**indexp == 0)
			rval = EX_JSON_PARSE_ERROR;
	}

	return rval;

}

// Look up value in JSON for the given key, and insert it into the result.
int
insert_tag(char *key, char **htmlp, char *json, unsigned short *indexp)
{
	// Stub.
	return 1;
}

// Given a mustache template and some JSON, render the HTML.
int
render(const char *template, char *json, char **html) 
{
	const char	*p = 0;
	const char	*last = 0;
	unsigned short	*index = 0;
	const char	*cur= 0;
	const char	*prev= 0;
	char		*qhtml = 0;
	char		*qtag = 0;
	char		tag[MAX_TAGSZ] = {0};
	int		rval = 0;

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
		 * Start in the HTML state.
		 */

	void **go = gohtml;

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
		 * Process template, one character at a time.
		 */

	
	for(cur = template; *cur && !rval; p++)
	{
		goto *go[(unsigned char) *cur];
		prev = cur;
		l_loop:;
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
		*go = gotagp;
		goto l_loop;

	l_no_tag:
		*qhtml++ = *prev;
		*qhtml++ = *cur;
		*go = gohtml;
		goto l_loop;

	l_yes_tag:
		*go = gotag;
		goto l_loop;



	l_htmlp:
		*go = gohtmlp;
		goto l_loop;

	l_no_html:
		*qtag++ = *prev;
		*qtag++ = *cur;
		*go = gotag;
		goto l_loop;

	l_yes_html:
		*go = gohtml;
		insert_tag(tag, html, json, index);
		memset(tag, 0, MAX_TAGSZ);
		goto l_loop;


}
