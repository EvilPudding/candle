#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ecm.h>

void path_join(char *path, ulong size, const char *other)
{
	/* printf("\t\t%s + %s\n", path, other); */
	if(other == NULL) return;
	if(other[0] == '/') other++;
	if(other[0] == '\0') return;

	char buffer[size];

	ulong len = strlen(path);

	if(path[len] == '/') path[len] = '\0';
	strncpy(buffer, path, size);

	if(path[0] == '\0')
	{
		strncpy(path, other, size);
	}
	else
	{
		snprintf(path, size, "%s/%s", buffer, other);
	}
}

const char *path_relative(const char *path, const char *dir)
{
	while(path[0] == dir[0])
	{
		if(path[0] == '\0') return path;
		path++;
		dir++;
	}
	if(path[0] == '/') path++;
	return path;
}

void to_lower_case(char *str)
{
	char c;
	while((c = *str))
	{
		if(c >= 'A' && c <= 'Z')
		{
			*str = c + 'a' - 'A';
		}
		str++;
	}
}

char *filter_sauce_name(char *path_name)
{
	char *slash = strrchr(path_name, '/')?:strrchr(path_name, '\\');
	if(slash) path_name = slash + 1;
	char *dot = strrchr(path_name, '.');
	if(dot) *dot = '\0';
	to_lower_case(path_name);
	return path_name;
}

