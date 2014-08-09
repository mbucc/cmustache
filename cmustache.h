#define	EX_TAG_TOO_LONG				4201
#define	EX_TOO_MANY_KEYVAL_PAIRS		4202
#define	EX_JSON_PARSE_ERROR			4203
#define	EX_LOGIC_ERROR				4204
#define	EX_INVALID_CHAR				4205
#define	EX_POP_DOES_NOT_MATCH		4206
#define	EX_INVALID_SECTION_NAME		4207

int	render(const char* template, char *json, char **resultp);

int	index_json(const char *json, unsigned short **indexp);

int	size_index(const char *json, unsigned short **indexp, unsigned int *iszp);

int	get(const char *json, const char *key, char **val);

int	split_key(const char *key, char **parent, char **child);

int	valcpy(const char *json, const char *key, char**val);



