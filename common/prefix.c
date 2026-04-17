#define _GNU_SOURCE
#define _REENTRANT

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "prefix.h"

char *br_strcat(const char *str1, const char *str2)
{
	char *result;

	if (!str1) str1 = "";
	if (!str2) str2 = "";

	result = (char *) calloc(strlen(str1) + strlen(str2) + 1, sizeof(char));
	strcpy(result, str1);
	strcat(result, str2);
	return result;
}

char *br_extract_dir(const char *path)
{
	char *end, *result;

	if (!path) return NULL;

	end = strrchr(path, '/');
	if (!end) return strdup(".");

	while (end > path && *end == '/')
		end--;

	result = strndup(path, end - path + 1);

	if (!*result)
	{
		free(result);
		return strdup("/");
	}

	return result;
}

char *br_extract_prefix(const char *path)
{
	char *end, *tmp, *result;

	if (!path) return NULL;

	if (!*path) return strdup("/");

	end = strrchr(path, '/');
	if (!end) return strdup(path);

	tmp = strndup(path, end - path);
	if (!*tmp)
	{
		free(tmp);
		return strdup("/");
	}

	end = strrchr(tmp, '/');
	if (!end) return tmp;

	result = strndup(tmp, end - tmp);
	free(tmp);

	if (!*result)
	{
		free(result);
		result = strdup("/");
	}

	return result;
}
