#include <err.h>
#include <sys/errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "js0n.h"
#include "j0g.h"
#include "htmlescape.h"
#include "vec.h"

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


// Return the number of times the character c appears in the string s.
unsigned long
count(const char *s, char c)
{
	unsigned long		n = 0;

	for (; *s; s++, n += (*s == c));

	return n;
}

// Compute the size of an array
// that is large enough to hold one offset and one length value
// for each key and each value in the JSON.
// Allocate the array.
// Return the array and it's size.
int
size_index(const char *json, unsigned short **indexp, unsigned int *iszp)
{
	int		rval = 0;
	long		n = 0;

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


	n = count(json, ',');
	if (n >= UINT_MAX)
		rval = EX_TOO_MANY_KEYVAL_PAIRS;

	*iszp = (unsigned int) n;

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
insert_value(char *key, char **qhtml, char *json, unsigned short *index, int raw)
{
	int 		rval = 0;
	char		*val = 0;
	char		*escaped = 0;

		/*
		 * A tag that starts with an ampersand
		 * is the same as a triple brace.
		 */

	if (key[0] == '&') {
		raw = 1;
		key = key + 1;
	}

	rval = get(json, index, key, &val);

	if (!rval) {
		if (raw)
			escaped = val;
		else
			rval = htmlescape(val, &escaped);
	}

	if (!rval && escaped) {
		strcpy(*qhtml, escaped);
		*qhtml += strlen(*qhtml);
	}

	if (!raw)
		free(escaped);

	return rval;
}

int 
in(char c, char lo, char hi)
{
	return (c >= lo) && (c <= hi);
}

// http://en.wikipedia.org/wiki/Character_encodings_in_HTML#Illegal_characters
int
badchar(char c)
{
	return  in(c, 1, 8)
		|| in(c, 11, 12)
		|| in(c, 14, 31)
		|| in(c, 127, 159)
		;
}

int
add_to_tag(char **qtag, char *tag, char c)
{
	if (*qtag - tag >= MAX_TAGSZ - 1)
		return EX_TAG_TOO_LONG;
	*(*qtag)++ = c;
	return 0;
}

		/*
		 * As we drill deeper into sub and subsections,
		 * just mash them together with a period delimiter.
		 *
		 * Dotted names are section short-hand in the spec,
		 * so this should work.
		 *
		 * While not specified in the interpolation spec,
		 * we do prohibit periods in the section names.
		 */

int
push_section(char *tag,  vec_str_t sections)
{
	int rval = 0;
	char *sec = 0;

	sec = strcpy(tag);
	if (!sec)
		rval = ENOMEM;

	vec_push(sections, sec);

	return rval;
}


int
pop_section(char *tag, vec_str_t sections)
{
	int rval = 0;
	char *last = 0;

	last = vec_pop(sections);

	if (!strcmp(tag, last) != 0)
		rval = EX_POP_DOES_NOT_MATCH;

	free(last);

	return rval;
}


// Given a mustache template and some JSON, render the HTML.
int
render(const char *template, char *json, char **html)
{
	char		tag[MAX_TAGSZ] = {0};
	char		sections[MAX_TAGSZ] = {0};
	const char	*cur= 0;
	char		prev;
	char		prevprev;
	char		*qhtml = 0;
	char		*qtag = 0;
	char		*qsections = 0;
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
		[ '{' ]		= &&l_rawtagp,
		[124 ... 255]	= &&l_no_tag
	};

	static void *gorawtagp[] =
	{
		[0 ... 34]	= &&l_no_rawtag,
		[ '#' ]		= &&l_yes_push,	// 35
		[36 ... 46]	= &&l_no_rawtag,
		[ '/' ]		= &&l_yes_pop,	// 47
		[48 ... 124]	= &&l_no_rawtag,
		[ '{' ]		= &&l_yes_rawtag,	// 125
		[126 ... 255]	= &&l_no_rawtag
	};

	static void *gopush[] =
	{
		[0 ... 45 ]	= &&l_push,
		[ '.' ]		= &&l_bad_section,	// 46
		[47 ... 124 ]	= &&l_push,
		[ '}' ]		= &&l_xpushp,	// 125
		[126 ... 255]	= &&l_push
	};

	static void *goxpushp[] =
	{
		[0 ... 124 ]	= &&l_no_xpush,
		[ '}' ]		= &&l_yes_xpush,	// 125
		[126 ... 255]	= &&l_no_xpush
	};

	static void *gopop[] =
	{
		[0 ... 45 ]	= &&l_pop,
		[ '.' ]		= &&l_bad_section,	// 46
		[47 ... 124 ]	= &&l_pop,
		[ '}' ]		= &&l_xpopp,	// 125
		[126 ... 255]	= &&l_pop
	};

	static void *goxpopp[] =
	{
		[0 ... 124 ]	= &&l_no_xpop,
		[ '}' ]			= &&l_yes_xpop,	// 125
		[126 ... 255]	= &&l_no_xpop
	};

	static void *gotag[] =
	{
		[0 ... 124 ]	= &&l_tag,
		[ '}' ]			= &&l_xtagp,	// 125
		[126 ... 255]	= &&l_tag
	};

	static void *goxtagp[] =
	{
		[0 ... 124]	= &&l_no_xtag,
		[ '}' ]		= &&l_yes_xtag,
		[126 ... 255]	= &&l_no_xtag
	};

	static void *gorawtag[] =
	{
		[0 ... 124 ]	= &&l_rawtag,
		[ '}' ]			= &&l_xrawpp,	// 125
		[126 ... 255]	= &&l_rawtag
	};

	static void *goxrawpp[] =
	{
		[0 ... 124]	= &&l_no_xrawp,
		[ '}' ]		= &&l_yes_xrawp,
		[126 ... 255]	= &&l_no_xrawp
	};

	static void *goxrawp[] =
	{
		[0 ... 124]	= &&l_no_xraw,
		[ '}' ]		= &&l_yes_xraw,	// 125
		[126 ... 255]	= &&l_no_xraw
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
		if (badchar(*cur))
			rval = EX_INVALID_CHAR;
		else
			goto *go[(unsigned char) *cur];
		l_loop:
		prev = *cur;
		prevprev = prev;
	}

	free(index);

	return rval;

		/*
		 * The state transitions.
		 */

	l_bad_section:
		rval = EX_SECTION_NAME_HAS_DOT;
		goto l_loop;

	l_html:
		*qhtml++ = *cur;
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

	l_rawtagp:
		go = gorawtagp;
		debug_printf("\t\t--> %s (%s)\n", "gorawtagp", "l_rawtagp");
		goto l_loop;

	l_no_rawtag:
		qtag = tag;
		rval = add_to_tag(&qtag, tag, *cur);
		go = gotag;
		debug_printf("\t\t--> %s (%s)\n", "gotag", "l_no_rawtag");
		goto l_loop;

	l_yes_push:
		qtag = tag;
		go = gopush;
		debug_printf("\t\t--> %s (%s)\n", "gopush", "l_yes_push");
		goto l_loop;

	l_yes_pop:
		qtag = tag;
		go = gopop;
		debug_printf("\t\t--> %s (%s)\n", "gopop", "l_yes_pop");
		goto l_loop;

	l_yes_rawtag:
		qtag = tag;
		go = gorawtag;
		debug_printf("\t\t--> %s (%s)\n", "gorawtag", "l_yes_rawtag");
		goto l_loop;

	l_push:
		/* FALLTHROUGH */

	l_pop:
		/* FALLTHROUGH */

	l_rawtag:
		/* FALLTHROUGH */

	l_tag:
		rval = add_to_tag(&qtag, tag, *cur);
		goto l_loop;

	l_xpushp:
		go = goxpushp;
		debug_printf("\t\t--> %s (%s)\n", "goxpushp", "l_xpushp");
		goto l_loop;

	l_no_xpush:
		rval = add_to_tag(&qtag, tag, prev);
		if (!rval)
			rval = add_to_tag(&qtag, tag, *cur);
		go = gopush;
		debug_printf("\t\t--> %s (%s)\n", "gopush", "l_no_xpush");
		goto l_loop;

	l_yes_xpush:
		go = gohtml;
		debug_printf("\t\t--> %s (%s)\n", "gohtml", "l_yes_xpush");
		rval = push_section(tag, &qsections, sections);
		memset(tag, 0, MAX_TAGSZ);
		goto l_loop;

	l_xpopp:
		go = goxpopp;
		debug_printf("\t\t--> %s (%s)\n", "goxpopp", "l_xpopp");
		goto l_loop;

	l_no_xpop:
		rval = add_to_tag(&qtag, tag, prev);
		if (!rval)
			rval = add_to_tag(&qtag, tag, *cur);
		go = gopop;
		debug_printf("\t\t--> %s (%s)\n", "gopop", "l_no_xpop");
		goto l_loop;

	l_yes_xpop:
		go = gohtml;
		debug_printf("\t\t--> %s (%s)\n", "gohtml", "l_yes_xpop");
		rval = pop_section(tag, &qsections, sections);
		memset(tag, 0, MAX_TAGSZ);
		goto l_loop;

	l_xtagp:
		go = goxtagp;
		debug_printf("\t\t--> %s (%s)\n", "goxtagp", "l_xtagp");
		goto l_loop;

	l_no_xtag:
		rval = add_to_tag(&qtag, tag, prev);
		if (!rval)
			rval = add_to_tag(&qtag, tag, *cur);
		go = gotag;
		debug_printf("\t\t--> %s (%s)\n", "gotag", "l_no_xtag");
		goto l_loop;

	l_yes_xtag:
		go = gohtml;
		debug_printf("\t\t--> %s (%s)\n", "gohtml", "l_yes_xtag");
		debug_printf("before insert, html = \"%s\"\n", *html);
		rval = insert_value(tag, &qhtml, json, index, 0);
		debug_printf("after insert, html = \"%s\"\n", *html);
		memset(tag, 0, MAX_TAGSZ);
		goto l_loop;

	l_xrawpp:
		go = goxrawpp;
		debug_printf("\t\t--> %s (%s)\n", "goxrawpp", "l_xrawpp");
		goto l_loop;

	l_no_xrawp:
		rval = add_to_tag(&qtag, tag, prev);
		if (!rval)
			rval = add_to_tag(&qtag, tag, *cur);
		go = gorawtag;
		debug_printf("\t\t--> %s (%s)\n", "gorawtag", "l_no_xrawp");
		goto l_loop;

	l_yes_xrawp:
		go = goxrawp;
		debug_printf("\t\t--> %s (%s)\n", "goxrawp", "l_yes_xrawp");
		goto l_loop;

	l_no_xraw:
		rval = add_to_tag(&qtag, tag, prevprev);;
		if (!rval)
			rval = add_to_tag(&qtag, tag, prev);
		if (!rval)
			rval = add_to_tag(&qtag, tag, *cur);
		go = gorawtag;
		debug_printf("\t\t--> %s (%s)\n", "gorawtag", "l_no_xraw");
		goto l_loop;

	l_yes_xraw:
		go = gohtml;
		debug_printf("\t\t--> %s (%s)\n", "gohtml", "l_yes_xraw");
		debug_printf("before insert raw, html = \"%s\"\n", *html);
		rval = insert_value(tag, &qhtml, json, index, 1);
		debug_printf("after insert raw, html = \"%s\"\n", *html);
		memset(tag, 0, MAX_TAGSZ);
		goto l_loop;

}
