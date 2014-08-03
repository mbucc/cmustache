#include <err.h>
#include <stdlib.h>

#include "cmustache.h"

#define BUFSZ 10240

int
render(const char *template, char *json, char **resultp) 
{
	const char	*p = 0;
	char		*q = 0;
	int		rval = 0;

	*resultp = (char *) calloc(BUFSZ, 1);

	if  ( *resultp == NULL)
		rval = 1;

	q = *resultp;

	static void *gostruct[] = 
	{
		[0 ... 255] = &&l_copy
	};

	void **go = gostruct;
	
	for(p = template; *p && !rval; p++)
	{
		goto *go[*p];
		l_loop:;
	}

	return rval;
	
	l_copy:
		*q++ = *p;
		//q++;
		goto l_loop;


}