#include <sys/errno.h>

#include <ctype.h>
#include <err.h>
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
#define MAX_KEYSZ	1024
#define DOT		'.'


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


// Compute the size of an array
// that is large enough to hold one offset and one length value
// for each key and each value in the JSON.
// Allocate the array.
// Return the array and it's size.
int
size_index(const char *json, size_t jsonlen, unsigned short **indexp, unsigned int *iszp)
{
	const char	*p;
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

	for (p = json; !rval && p - json < jsonlen; p++) {
		n += (*p == ',');
		if (n >= UINT_MAX)
			rval = EX_TOO_MANY_KEYVAL_PAIRS;
	}

	if (!rval)
		*iszp = (unsigned int) n;

		/*
		 * The array holds short ints, but the array index
		 * is an unsigned int, so we check against UINT_MAX.
		 */

	if (!rval && (long) *iszp + extra > UINT_MAX / entriesPerComma)
		rval = EX_TOO_MANY_KEYVAL_PAIRS;

	if (!rval) {
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
index_json(const char *json, size_t jsonlen, unsigned short **indexp)
{
	int		rval = 0;
	unsigned int	isz;

	rval = size_index(json, jsonlen, indexp, &isz);

	if (!rval) {

		rval = js0n((const unsigned char *) json, jsonlen, *indexp, isz);

		if (rval)

			rval = EX_JSON_PARSE_ERROR;
	}

	return rval;

}

int
is_obj(const char *json, size_t jsonlen) 
{
	const char	*p;
	for (p = json; isspace(*p) && p - json < jsonlen; p++);
	return *p == '{';
}

// Given a (perhaps dotted) key, return the offset and length
// of the value of this key.
//
// If key is not found, offset is set to 0.
int
jsonpath(const char *json, size_t jsonlen, const char *key, unsigned short *offset, unsigned short *length)
{
	char		*keybuf = 0;
	char		*p = 0;
	unsigned short	*index = 0;
	unsigned short	suboffset = 0;
	unsigned short	sublength = 0;
	int		rval = 0;
	int		idx = 0;

	if (!offset || !length)
		return EX_LOGIC_ERROR;

	*offset = 0;
	*length = 0;

	debug_printf("jsonpath('%s', %lu, '%s')\n", json, jsonlen, key);

	if (!json || !strlen(json))
		return rval;

	if (!key || !strlen(key))
		return rval;

		/*
		 * Only JSON objects have keys.
		 * If we are not in an object, 
		 * we ran out of levels to parse down into
		 * and the key is not found.
		 */
	if ( ! is_obj(json, jsonlen) )
		return rval;

	rval = index_json(json, jsonlen, &index);

	if (!rval) {
		idx = j0g_val(key, (char *) json, index);

		/*
		 * If the key is found, then idx >= 2.
		 */

		if (idx != 0) {
			*offset = index[idx];
			*length = index[idx + 1];
		}
	}

		/*
		 * If key not found, look for it one level down.
		 */

	if (!rval && *offset == 0) {
		keybuf = calloc(strlen(key), 1);
		if (!keybuf)
			rval = ENOMEM;
	}

	if (!rval && *offset == 0) {
		strcpy(keybuf, key);
		p = strchr(keybuf, DOT);
		if (p) {
			*p++ = '\0';
			idx = j0g_val(keybuf, (char *) json, index);
			if (idx != 0) {
				suboffset = index[idx];
				sublength = index[idx + 1];
				json += suboffset;
				rval =  jsonpath(json, sublength, p, offset, length);

				if (!rval && *offset != 0)
					*offset += suboffset;
				else
					/* EMTPY -- if offset is zero, then key is not found. */
					;
			}
			else
				/* EMPTY -- if an part of key is not found, then the full key is not found. */
				;
		}
		else
			/* EMPTY -- no dots in key, so no subsection to look in ... the key's not found. */
			;
	}


	free(index);
	free(keybuf);
		
	return rval;

}


// Given key, return value in current json.
//
// If the key is a path (e.g., "a.b.c.d"),
// we use the last component of the path as the key (e.g., "d").
int
valcpy(const char *json, const char *keypath, char**val)
{
	int rval = 0;
	int idx = 0;
	char *key = 0;
	unsigned short	*index = 0;
	unsigned short	offset = 0;
	unsigned short	length = 0;

	debug_printf("valcpy('%s', '%s')\n", json, keypath);

	*val = 0;

	rval = index_json(json, strlen(json), &index);

	if (!rval) {
		if ( (key = strchr(keypath, DOT) ) == 0)
			key = (char *) keypath;
		else
			key++;

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

		/*
		 * Trim whitespace off ends of value.
		 */

		while (isspace(*(json + offset)) && length > 0) {
			offset++;
			length--;
		}

		while (isspace(*(json + offset + length - 1)) && length > 0) {
			length--;
		}

		*val = calloc(length + 1, 1);
		if (!*val)
			rval = ENOMEM;
	}

	if (!rval)
		strncpy(*val, json + offset, length);

	debug_printf("valcpy('%s', '%s') --> '%s'\n", json, keypath, *val);

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

	*child = strchr(*parent, DOT);

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

int
popup(char *section) 
{
	int		rval = 0;
	char		*p = 0;

	if (!section || !*section || !strlen(section))
		return rval;

	p = strrchr(section, DOT);

	if (p)
		*p = '\0';
	else
		*section = '\0';

	return rval;
}

// Lookup a key's value and copy it to *val.
// If the key is not found, *val is set to NULL.
int
get(const char *json, size_t jsonlen, const char *section, const char *key, char **val)
{
	char		*secbuf = 0;
	char		*p = 0;
	int		rval = 0;
	int		loop_limit = 1000;
	int		loop_i = 0;
	unsigned short	offset = 0;
	unsigned short	length = 0;

	debug_printf("get('%s', '%s', '%s', '%s')\n", json, section, key, *val);

	if (!val)
		return EX_LOGIC_ERROR;

	*val = 0;

		/*
		 * No data is not an error.
		 * All keys are simply not found.
		 */

	if (!json || !strlen(json))
		return 0;

	secbuf = calloc(strlen(section), 1);
	if (!secbuf)
		rval = ENOMEM;
	if (!rval)
		strcpy(secbuf, section);

	while (!rval && !*val && strlen(secbuf) && loop_i < loop_limit) {

		loop_i++;

		rval = jsonpath(json, jsonlen, secbuf, &offset, &length);

		/*
		 * The JSON has this section, see if the section has the key.
		 */

		if (!rval) {
			if (offset) {
				json += offset;
				rval = jsonpath(json, length, key, &offset, &length);
			}

			if (!rval) {
				if  (offset) {
		/*
		 * Victory, we found the key in this section.
		 */
					*val = calloc(length + 1, 1);
					if (*val)
						strncat(*val, json + offset, length);
					else
						rval = ENOMEM;
				}

		/*
		 * The section didn't have key, 
		 * loop and look for key in the parent section.
		 */

				else {
					p = strrchr(secbuf, DOT);
					if (p)
						*p = '\0';
					else
						secbuf = '\0';
				}
			}
		}
	}
	
	if (loop_i >= loop_limit)
		rval = EX_LOGIC_ERROR;

	free(secbuf);

	debug_printf("\"%s\" returns \"%s\"\n", key, *val);

	return rval;

}


// Look up value in JSON for the given key, and insert it into the result.
int
insert_value(const char *section, char *tag, char **qhtml, char *json, int raw)
{
	int 		rval = 0;
	char		*key = 0;
	char		*val = 0;
	char		*escaped = 0;

	debug_printf("insert_value('%s', '%s', '%s', '%s', %d)\n", section, tag, *qhtml, json, raw);

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

	if (!rval)
		rval = get(json, strlen(json), section, key, &val);


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

		/*
		 * Return true if the character is in the given, inclusive, range.
		 */
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
	if (*qtag - tag >= MAX_KEYSZ - 1)
		return EX_TAG_TOO_LONG;
	if (!isspace(c))
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

	if (strlen(tag) + strlen(section) + 1 >= MAX_KEYSZ)
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
		p = strrchr(section, DOT);
		if (p)

			popped = p + 1;

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

int
is_section_falsey(const char *json, const char *section, int *drop)
{
	int rval = 0;
	char *val = 0;

	if (!section || !strlen(section))

		*drop = 0;

	else {	
	
		rval = get(json, strlen(json), 0, section, &val);
		if (!rval) {
			if (!val)
				*drop = 1;
			else if (!strcmp(val, "false"))
				*drop = 1;
			else
				*drop = 0;
		}
		
	}

	debug_printf("is_section_falsey('%s', '%s') --> %d\n", json, section, *drop);

	return rval;
}


// Given a mustache template and some JSON, render the HTML.
int
render(const char *template, char *json, char **html)
{
	char		tag[MAX_KEYSZ] = {0};
	char		section[MAX_KEYSZ] = {0};
	const char	*cur= 0;
	char		prev;
	char		prevprev;
	char		*qhtml = 0;
	char		*qtag = 0;
	int		drop = 0;
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
		[ DOT ]		= &&l_bad_section,	// 46
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
		[ DOT ]		= &&l_bad_section,	// 46
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
		if (!drop)
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
		if (!rval)
			rval = is_section_falsey(json, section, &drop);
		memset(tag, 0, MAX_KEYSZ);
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
		if (!rval)
			rval = is_section_falsey(json, section, &drop);
		memset(tag, 0, MAX_KEYSZ);
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
		if (!drop)
			rval = insert_value(section, tag, &qhtml, json, 0);
		debug_printf("after insert, html = \"%s\"\n", *html);
		memset(tag, 0, MAX_KEYSZ);
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
		if (!drop)
			rval = insert_value(section, tag, &qhtml, json, 1);
		debug_printf("after insert raw, html = \"%s\"\n", *html);
		memset(tag, 0, MAX_KEYSZ);
		goto l_loop;

}
