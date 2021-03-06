// Must include sys/queue.h before this.

#define	EX_TAG_TOO_LONG				4201
#define	EX_TOO_MANY_KEYVAL_PAIRS		4202
#define	EX_JSON_PARSE_ERROR			4203
#define	EX_LOGIC_ERROR				4204
#define	EX_INVALID_CHAR				4205
#define	EX_POP_DOES_NOT_MATCH		4206
#define	EX_INVALID_SECTION_NAME		4207
#define	EX_TOO_MANY_SECTIONS		4208

#define MAX_KEYSZ				1024
#define MAX_SECTION_DEPTH		20

enum jsontype {
	string_type,
	number_type,
	object_type,
	array_type,
	true_type,
	false_type,
	null_type
};

SLIST_HEAD(json, jsonpair);

struct jsonpair {
	unsigned short	offset;
	unsigned short length;
	unsigned short valoffset;
	unsigned short vallength;
	enum jsontype type;
	struct json children;
	SLIST_ENTRY(jsonpair) link;
};

int	render(const char* template, char *json, char **resultp);

int	size_index(const char *json, size_t jsonlen, unsigned short **indexp, unsigned int *iszp);

int	index_json(const char *json, size_t jsonlen, unsigned short **indexp);

int	parsejson(const char *json, size_t jsonlen, struct json *jp);

int	get(const char *json, size_t jsonlen, char section[][MAX_KEYSZ], int sectionidx, const char *key, char **val);

int	jsonpath(const char *json, size_t jsonlen, const char *key, unsigned short *offset, unsigned short *length);

void	trim(const char *json, unsigned short *offset, unsigned short *length);



