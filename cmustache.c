#include <sys/errno.h>

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "js0n.h"
#include "j0g.h"
#include "htmlescape.h"
#include "queue.h"
#include "vec.h"

#include "cmustache.h"

#define BUFSZ_DELTA	10240
#define DOT			'.'


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


// The js0n library scans the json string
// and records the (offset, length) pair for each key and each value
// found in the json string.
//
// This routine calculates the size of the array (\*iszp) required
// for the given a json string
// and allocates a storage array of unsigned shorts (\*\*indexp).
//
// If the memory allocation fails, it returns ENOMEM.
// 
// If the required length overflows an unsigned int, 
// it returns EX_TOO_MANY_KEYVAL_PAIRS.
int
size_index(const char *json, size_t jsonlen, unsigned short **indexp, unsigned int *iszp)
{
	const char	*p;
	int		rval = 0;
	size_t		n = 0;

		/*
		 * (one key + one value) x (one offset + one length) = 4
		 */

	unsigned int entries_per_comma = 4;

		/*
		 * We need at least one extra slot (js0n zero-terminates
		 * the array) plus four for the last key/value pair.
		 * Add some more for safety ...
		 */

	unsigned int extra = 21;

		/*
		 * The number of commas in JSON is an upper limit
		 * on the number of key/value pairs - 1.
		 * 
		 * Note: On my macbook pro, it takes 8.62 seconds to count
		 * to UINT_MAX by one's.  Probably could use a lower limit.
		 */

	for (p = json; !rval && p - json < jsonlen; p++) {
		n += (*p == ',');
		if (n >= UINT_MAX)
			rval = EX_TOO_MANY_KEYVAL_PAIRS;
	}
		/*
		 * The array holds short ints, but the array index
		 * is an unsigned int, so we check against UINT_MAX.
		 */

	if (!rval && n > UINT_MAX / entries_per_comma - extra)
		rval = EX_TOO_MANY_KEYVAL_PAIRS;

	if (!rval) {
		*iszp = (unsigned int) n;
		*iszp *= entries_per_comma;
		*iszp += extra;
		if ((*indexp = calloc(*iszp, sizeof(unsigned int))) == NULL)
			rval = ENOMEM;
	}

	return rval;

}



// Allocate and load the set of offset and length pairs
// for the key/values in the given JSON.  Zero-terminate
// the array.
//
// Returns 0 on success, 
// EX_JSON_PARSE_ERROR if there was an error parsing JSON.
int
index_json(const char *json, size_t jsonlen, unsigned short **indexp)
{
	int		rval = 0;
	unsigned int	isz;

	if (!json || !strlen(json) || !indexp)
		return rval;

	rval = size_index(json, jsonlen, indexp, &isz);

	if (!rval) {

		rval = js0n((const unsigned char *) json, jsonlen, *indexp, isz);

		if (rval)

			rval = EX_JSON_PARSE_ERROR;
	}

	return rval;

}

// Return 1 if the first non-whitespace character in json is a '{', 0 otherwise.
int
is_obj(const char *json, size_t jsonlen) 
{
	const char	*p;
	for (p = json; isspace(*p) && p - json < jsonlen; p++);
	return *p == '{';
}




// Move the offset and length so
// the character sequence they define
// does not start or end with whitespace.
void
trim(const char *json, unsigned short *offset, unsigned short *length)
{
	if (!offset || !length || !json)
		return;

	while (isspace(*(json + *offset)) && *length > 0) {
		(*offset)++;
		(*length)--;
	}

	while (isspace(*(json + *offset + *length - 1)) && *length > 0) {
		(*length)--;
	}

}

// Given a json string and a key, 
// return the offset and length
// of the key's value.
//
// If key is not found, offset is set to 0.
int
jsonpath(const char *json, size_t jsonlen, const char *key, 
		unsigned short *offset, unsigned short *length)
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

	// An empty key never has a value.
	if (!key || !strlen(key))
		return 0;

	debug_printf("jsonpath('%s', %lu, '%s')\n", json, jsonlen, key);

	// Nor does a key in empty json.
	if (!json || !strlen(json))
		return rval;

	// Only json objects have keys.
	if ( ! is_obj(json, jsonlen) )
		return rval;

	// Look for key in current JSON object.
	rval = index_json(json, jsonlen, &index);
	if (!rval) {
		idx = j0g_val(key, (char *) json, index);

		if (idx != 0) {
			// We found the key.  
			// Set offset, length, and trim off whitespace.
			*offset = index[idx];
			*length = index[idx + 1];
			trim(json, offset, length);
		}
	}

	// We didn't find the key so try to recurse down through json.
	//
	// For example,
	// if the key = "a.b"
	// and we didn't find "a.b" as an attribute of  the top-level json object,
	// then ...

	if (!rval && *offset == 0) {
		keybuf = calloc(strlen(key), 1);
		if (!keybuf)
			rval = ENOMEM;
	}

	if (!rval && *offset == 0) {
		strcpy(keybuf, key);
		p = strchr(keybuf, DOT);
		if (p) {
			//
			// ... we look for a top-level attribute named "a" ...
			//
			*p++ = '\0';
			idx = j0g_val(keybuf, (char *) json, index);
			if (idx != 0) {
				suboffset = index[idx];
				sublength = index[idx + 1];
				json += suboffset;
				//
				// ... and look for the key "b" in that.
				//
				rval =  jsonpath(json, sublength, p, offset, length);

				if (!rval && *offset != 0) {
					trim(json, offset, length);
					*offset += suboffset;
				}
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

	debug_printf("		--> jsonpath returns offset, length = %u, %u\n", *offset, *length);
		
	return rval;

}

int
get_json_section(const char  *json, size_t len, 
		char section[][MAX_KEYSZ], int base, int depth,
		unsigned short *offset, unsigned short *length)
		
{
	int		rval = 0;

	for (int i = base; !rval && i < depth && len > 0;i++) {
		rval = jsonpath(json, len, section[i], offset, length);
		json += *offset;
		len = *length;
	}
	return rval;
} 

// Return true if a section is not defined.
//
// For example, if section = `{"a", "b"}`
// and json = `{"a": {"b": 1}}`,
// is_section_falsey returns false.
//
// This simple concept can get complicated fast.
// For example, the section `{ "a", "b", "c"}`
// is true for json = `{"a": 1, "b": 1, "c": 1}`
// and false for json = `{"b": 1, "c": {"a": 1}}`

int
is_section_falsey(const char *json, size_t jsonlen,
		char section[][MAX_KEYSZ], int sections_n, int *drop)
{
	int		rval = 0;
	unsigned short	offset = 0;
	unsigned short	length = 0;

	*drop = 0;

	rval = get_json_section(json, jsonlen, section, 0, sections_n,
		&offset, &length);

	if (!rval) {
		if (length == 5 && strncmp(json + offset, "false", 5) == 0)
			*drop = 1;
	}

	return rval;
}


int
peeloff_sections_from_top(const char  * const json, size_t jsonlen, 
		char section[][MAX_KEYSZ], int sections_n, 
		const char *key, char **val)
{
	int		rval = 0;
	unsigned short	offset = 0;
	unsigned short	length = 0;
	unsigned short	section_offset = 0;
	unsigned short	section_length = 0;

	// Loop through all contexts, starting with first section base.
	for (int base = 0; !rval && !*val && base < sections_n; base++) {

		rval = get_json_section(json, jsonlen, section, base, sections_n, &offset, &length);

/*

		// Get json starting at current section base.
		p = json;
		len = jsonlen;
		for (int i = base; !rval && i < sections_n && len > 0;i++) {
			rval = jsonpath(p, len, section[i], &offset, &length);
			p += offset;
			len = length;
		}
*/

		// Look for key is in this section's json (aka "context").
		if (!rval && length)
			rval = jsonpath(json + section_offset, section_length, key, &offset, &length);

		if (!rval && offset) {
			// We found the key so make a copy of its value.
			*val = calloc(length + 1, 1);
			if (*val)
				strncat(*val, json + offset, length);
			else
				rval = ENOMEM;
		}
	}

	return rval;


}

// Look up key, first in deepest section.
// If not found, peel off sections one-by-one looking for key in each.
//
// For example, if section array is `{"a", "b"}`, 
// first look for key in `{a: {b: {<here>} } }`
// then in `{a: {<here>} }`,
// and finally in root object `{<here>}`.
int
peeloff_sections_from_bottom(const char *json, size_t jsonlen,
		char section[][MAX_KEYSZ], int sections_n, 
		const char *key, char **val)
{
	const char	*p = 0;
	size_t		len = 0;
	int		rval = 0;
	unsigned short	offset = 0;
	unsigned short	length = 0;

	// Loop through all contexts, starting at full section depth.
	for (int depth = sections_n; !rval && !*val && depth > 0; depth--) {

		// Get json at current section depth.
		p = json;
		len = jsonlen;
		for (int i = 0; !rval && i < depth && len > 0;i++) {
			rval = jsonpath(p, len, section[i], &offset, &length);
			p += offset;
			len = length;
		}

		// Look for key is in this section's json (aka "context").
		if (!rval && len)
			rval = jsonpath(p, len, key, &offset, &length);

		if (!rval && offset) {
			// We found the key so make a copy of its value.
			*val = calloc(length + 1, 1);
			if (*val)
				strncat(*val, p + offset, length);
			else
				rval = ENOMEM;
		}
	}

	return rval;

}


// Lookup a key's value and copy it to \*val.
// If the key is not found, \*val is set to NULL.
//
// If sections_n > 0, the key is looked up in the context of the given section.
// See the file specs/resolution.json for details on how that works.
int
get(const char  *json, size_t jsonlen, 
		char section[][MAX_KEYSZ], int sections_n, 
		const char *key, char **val)
{
	int		rval = 0;
	int		drop = 0;
	unsigned short	offset = 0;
	unsigned short	length = 0;

	debug_printf("get('%s', %lu, '%s', '%s')\n", json, jsonlen, section[sections_n], key);

	// The val pointer must be allocated.
	if (!val)
		return EX_LOGIC_ERROR;

	*val = 0;

	// An empty json string is not an error.
	if (!json || !strlen(json))
		return 0;

	// If any section is falsey, key is not found.
	for (int i = 0; i < sections_n; i++) {
		rval = is_section_falsey(json, jsonlen, section, sections_n, &drop);
		if (drop)
			return 0;
	}


	if (!rval)
		rval = peeloff_sections_from_top(json, jsonlen, section, sections_n, key, val);


	if (!rval && !*val)
		rval = peeloff_sections_from_bottom(json, jsonlen, section, sections_n, key, val);

		/*
		 * If not found in section, try global context.
		 */

	if (!rval && !*val) {

		rval = jsonpath(json, jsonlen, key, &offset, &length);

		if (!rval) {
			if  (offset) {
				*val = calloc(length + 1, 1);
				if (*val)
					strncat(*val, json + offset, length);
				else
					rval = ENOMEM;
			}
		}
				
	}

	debug_printf("\"%s\" returns \"%s\" (rval = %d)\n", key, *val, rval);

	return rval;

}


// Look up value in JSON for the given key, and insert it into the result.
int
insert_value(char section[][MAX_KEYSZ], int sections_n, char *tag, char **qhtml, char *json, int raw)
{
	int 		rval = 0;
	char		*val = 0;
	char		*escaped = 0;

	debug_printf("insert_value('%s', '%s', '%s', '%s', %d)\n", section[sections_n], tag, *qhtml, json, raw);

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
		rval = get(json, strlen(json), section, sections_n, tag, &val);


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
push_section(char *tag,  char section[][MAX_KEYSZ], int *sections_n)
{
	int rval = 0;
	if (!tag || !strlen(tag))
		return rval;

	if (strlen(tag) + 1 >= MAX_KEYSZ)
		rval = EX_TAG_TOO_LONG;

	if (*sections_n >= MAX_SECTION_DEPTH - 1)
		rval = EX_TOO_MANY_SECTIONS;

	if (!rval)
		strcpy(section[(*sections_n)++], tag);
	
	if (!rval)
		debug_printf("		push_section('%s') --> 'sections_n = %d'\n", tag, *sections_n);
	else
		debug_printf("		push_section('%s') --> 'rval = %d'\n", tag, rval);

	return rval;
}


int
pop_section(char *tag, char section[][MAX_KEYSZ], int *sections_n)
{
	int rval = 0;

	if (!tag || !strlen(tag))

		/*
		 * Ignore empty tags.
		 */

		return rval;

	if (sections_n == 0 || strcmp(tag, section[*sections_n - 1]) )
			rval = EX_POP_DOES_NOT_MATCH;

	if (!rval)
		section[(*sections_n)--][0] = '\0';

	if (!rval)
		debug_printf("		pop_section('%s') --> sections_n = %d\n", tag, *sections_n);
	else
		debug_printf("		pop_section('%s') --> rval = %d\n", tag, rval);

	return rval;
}

int
index_sections(const char *json, char ***section_index)
{
	int		rval = 0;

	return rval;
}


// Given a mustache template and some JSON, render the HTML.
int
render(const char *template, char *json, char **html)
{
	char		section[MAX_SECTION_DEPTH][MAX_KEYSZ] = {{0}};
	char		**section_index;
	char		tag[MAX_KEYSZ] = {0};
	const char	*cur= 0;
	char		prev;
	char		prevprev;
	char		*qhtml = 0;
	char		*qtag = 0;
	int		sections_n = 0;
	int		drop = 0;
	int		rval = 0;

	debug_printf("%s\n", "Starting to render");


	//
	// The states and their transitions.
	//

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

	// Allocate memory to hold HTML.
	if (!rval) {

		/*
		 * XXX: Track size and reallocate when necessary.
		 */

		*html = (char *) calloc(BUFSZ_DELTA, 1);

		if  ( *html == NULL)
			rval = ENOMEM;

		qhtml = *html;
	}

	// Index the sections in the json.
	if (!rval)
		rval = index_sections(json, &section_index);

	// Start in the HTML state.
	void **go = gohtml;

	// Process template, one character at a time.
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

	// The action on a state transition.
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
		rval = push_section(tag, section, &sections_n);
		if (!rval)
			rval = is_section_falsey(json, strlen(json), section, sections_n, &drop);
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
		rval = pop_section(tag, section, &sections_n);
		if (!rval)
			rval = is_section_falsey(json, strlen(json), section, sections_n, &drop);
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
			rval = insert_value(section, sections_n, tag, &qhtml, json, 0);
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
			rval = insert_value(section, sections_n, tag, &qhtml, json, 1);
		debug_printf("after insert raw, html = \"%s\"\n", *html);
		memset(tag, 0, MAX_KEYSZ);
		goto l_loop;

}
