#define _GNU_SOURCE
#define _REENTRANT

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <sys/stat.h>

#include "stringbuf.h"

#ifndef PKGDATADIR
#define PKGDATADIR "/usr/local/share/emergence"
#endif

char *found_resource = NULL;

static struct string_t *find_resource_in_root(const char *root, char *resource)
{
	struct stat buf;
	struct string_t *path = new_string_text("%s/%s", root, resource);

	if(stat(path->text, &buf) == 0)
		return path;

	free_string(path);
	return NULL;
}

char *find_resource(char *resource)
{
	static const char *search_roots[] = {
		PKGDATADIR,
		".",
		"../share/emergence",
		"../em-game/share",
		"../em-tools/share",
		"../em-tools",
		"../",
		"em-game/share",
		"em-tools/share",
		"em-tools",
	};

	struct string_t *s = NULL;
	size_t i;

	for(i = 0; i < sizeof(search_roots) / sizeof(search_roots[0]); i++)
	{
		s = find_resource_in_root(search_roots[i], resource);
		if(s)
			goto found;
	}

	goto error;

found:
	free(found_resource);
	printf("Found resource: %s\n", s->text);
	found_resource = s->text;
	free(s);
	return found_resource;

error:
	printf("Could not find resource: %s\n", resource);
	return NULL;
}
