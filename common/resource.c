#define _GNU_SOURCE
#define _REENTRANT

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/stat.h>

#include "stringbuf.h"
#include "prefix.h"

#ifndef PKGDATADIR
#define PKGDATADIR "/usr/local/share/emergence"
#endif

char *found_resource = NULL;

char *find_resource(char *resource)
{
	struct stat buf;
	struct string_t *s;

	s = new_string_text(PKGDATADIR "/");
	string_cat_text(s, resource);

	if(stat(s->text, &buf) == 0)
		goto found;

	free_string(s);
	s = new_string_text("./");
	string_cat_text(s, resource);

	if(stat(s->text, &buf) == 0)
		goto found;

	free_string(s);
	s = new_string_text("../share/emergence/");
	string_cat_text(s, resource);

	if(stat(s->text, &buf) < 0)
		goto error;

found:
	free(found_resource);
	printf("Found resource: %s\n", s->text);
	found_resource = s->text;
	free(s);
	return found_resource;

error:
	printf("Could not find resource: %s\n", resource);
	free_string(s);
	return NULL;
}
