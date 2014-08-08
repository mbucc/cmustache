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
#define SEC_DELIM		'.'


		/*
		 * Add -DDEUG to CFLAGS in Makefile to turn on debug output.
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
/*

		debug_printf("parsing %s\n", json);
		debug_printf("with length %lu\n", strlen(json));
		debug_printf("and isze  %lu\n", isz);
*/

		rval = js0n((const unsigned char *) json, strlen(json), *indexp, isz);

		if (rval)

			rval = EX_JSON_PARSE_ERROR;
	}

	return rval;

}

int
valcpy(const char *json, const char *key, char**val)
{
	int rval = 0;
	int idx = 0;
	unsigned short	*index = 0;
	unsigned short	offset = 0;
	unsigned short	length = 0;

	*val = 0;

	rval = index_json(json, &index);

	if (!rval) {

		idx = j0g_val(key, (char *) json, index);

		/*
		 * If the key was not found in the indexed JSON,
		 * then the index will be zero.
		 * If the key was found, idx will never be zero,
		 * as idx is the index of the values offset
		 * that is associated with the key.
		 */
		if (idx == 0)
			return 0;

		offset = index[idx];
		length = index[idx + 1];
		*val = calloc(length + 1, 1);
		if (!*val)
			rval = ENOMEM;
	}

	if (!rval)
		strncpy(*val, json + offset, length);

	return rval;

}

int
split_key(const char *key, char **parent, char **child)
{
	*parent = 0;
	*child = 0;

	if (!key || !strlen(key))
		return 0;

	*parent = calloc(strlen(key), 1);
	if (!*parent)
		return ENOMEM;

	 strcpy(*parent, key);

	*child = strchr(*parent, SEC_DELIM);

		/*
		 * We should only get to this method
		 * if the key contains a section separator.
		 */

	if (! *child) {
		free(*parent);
		*parent = 0;
		return  EX_LOGIC_ERROR;
	}

	**child = 0;
	(*child)++;

	debug_printf("split_key: '%s' --> ('%s', '%s')\n", key, *parent, *child);

	if (!strlen(*parent)) {
		free(*parent);
		*parent = 0;
		return EX_LOGIC_ERROR;
	}

	debug_printf("split_key: '%s' --> ('%s', '%s')\n", key, *parent, *child);

	return 0;

}

// Lookup a key's value and copy it to *val.
// Recurses down a hierarchy
// (periods in the key mean go down a level).
// For example, a key of 'a.b' and JSON of  {'a': {'b': 1}}
// loads "1" into *val.
// If key not found, *val is set to NULL.
int
get(const char *json, const char *key, char **val)
{
	char		*p = 0;
	char		*parent_key = 0;
	char		*child_key = 0;
	char		*parent_json = 0;
	int		rval = 0;

	debug_printf("get('%s', '%s', '%s')\n", json, key, *val);


	if (!json || !strlen(json)) {

		/*
		 * We hit a "falsey" value in the parent json.
		 * Return the empty string for the value.
		 */

		*val = 0;

		return 0;

	}

	if ( (p = strchr(key, SEC_DELIM) ) == 0) {

		rval = valcpy(json, key, val);

	}
	else {
	
		/*
		 * Recurse.
		 */

		rval = split_key(key, &parent_key, &child_key);

		if (!rval) {

			rval = valcpy(json, parent_key, &parent_json);


		}

		if (!rval)

			rval = get(parent_json, child_key, val);

		free(parent_key);
	}

	debug_printf("\"%s\" returns \"%s\"\n", key, *val);

	return rval;

}


// Look up value in JSON for the given key, and insert it into the result.
int
insert_value(const char *section,  char *tag, char **qhtml, char *json, int raw)
{
	int 		rval = 0;
	char		*key = 0;
	char		*val = 0;
	char		*escaped = 0;
	size_t	keysz = 0;

	debug_printf("insert_value('%s', '%s', '%s', '%s', %d)\n", section, tag, *qhtml, json, raw);

	keysz = strlen(section) + 1 + strlen(tag);
	if (keysz >= MAX_TAGSZ)
		rval = EX_TAG_TOO_LONG;

	if (!rval) {
		/*
		 * A tag that starts with an ampersand
		 * is the same as a triple brace.
		 */
	
		if (tag[0] == '&') {
			raw = 1;
			tag = tag + 1;
		}
	}
debug_printf("%s\n", "MKB1");

	if (!rval) {

		key = calloc(keysz, 1);
	
		if (strlen(section)) {
			strcpy(key, section);
			strcat(key, ".");
		}
		strcat(key, tag);
	}
debug_printf("%s\n", "MKB2");


	if (!rval)
		rval = get(json, key, &val);
debug_printf("%s\n", "MKB3");


	if (!rval) {
		if (raw)
			escaped = val;
		else
			rval = htmlescape(val, &escaped);
	}

debug_printf("%s\n", "MKB4");

	if (!rval && escaped) {
		strcpy(*qhtml, escaped);
		*qhtml += strlen(*qhtml);
	}

debug_printf("%s\n", "MKB5");

	if (!raw)
		free(escaped);

debug_printf("%s\n", "MKB6");

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
	debug_printf("add_to_tag('%s', '%s', %c)\n", *qtag, tag, c);
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
push_section(char *tag,  char *section)
{
	int rval = 0;

	if (strlen(tag) + strlen(section) + 1 >= MAX_TAGSZ)
		rval = EX_TAG_TOO_LONG;

	if (!rval) {
		if (strlen(section))
			strcat(section, ".");
		strcat(section, tag);
	}

	debug_printf("		push_section('%s') --> '%s'\n", tag, section);

	return rval;
}


int
pop_section(char *tag, char *section)
{
	char *p = 0;
	int rval = 0;
	char *popped = 0;

	if (!section) {
		if (tag)
			rval = EX_POP_DOES_NOT_MATCH;
		else

		/*
		 * Ignore empty tags.
		 */

			return rval;
	}

	if (!rval) {
		p = strrchr(section, SEC_DELIM);
		if (p) {
			popped = p + 1;
			p++;
		}
		else {
			p = section;
			popped = section;
		}

		if (strcmp(tag, popped) != 0) {
			rval = EX_POP_DOES_NOT_MATCH;
		}
		else {
			*p = 0;
		}
	}

	debug_printf("		pop_section('%s') --> '%s'\n", tag, section);

	return rval;
}


// Given a mustache template and some JSON, render the HTML.
int
render(const char *template, char *json, char **html)
{
	char		tag[MAX_TAGSZ] = {0};
	char		section[MAX_TAGSZ] = {0};
	const char	*cur= 0;
	char		prev;
	char		prevprev;
	char		*qhtml = 0;
	char		*qtag = 0;
	int		rval = 0;

	debug_printf("%s\n", "Starting to render");

		/*
		 * The states and their transitions.
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
		[48 ... 122]	= &&l_no_rawtag,
		[ '{' ]		= &&l_yes_rawtag,	// 123
		[124 ... 255]	= &&l_no_rawtag
	};

	static void *gopush[] =
	{
		[0 ... 45 ]	= &&l_push,
		[ SEC_DELIM ]		= &&l_bad_section,	// 46
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
		[ SEC_DELIM ]		= &&l_bad_section,	// 46
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

	return rval;

		/*
		 * The behavior on transition.
		 */

	l_bad_section:
		rval = EX_INVALID_SECTION_NAME;
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
		rval = push_section(tag, section);
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
		rval = pop_section(tag, section);
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
		rval = insert_value(section, tag, &qhtml, json, 0);
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
		rval = insert_value(section, tag, &qhtml, json, 1);
		debug_printf("after insert raw, html = \"%s\"\n", *html);
		memset(tag, 0, MAX_TAGSZ);
		goto l_loop;

}
