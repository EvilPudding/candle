#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void path_join(char *path, unsigned long size, const char *other)
{
	/* printf("\t\t%s + %s\n", path, other); */
	if(other == NULL) return;
	if(other[0] == '/') other++;
	if(other[0] == '\0') return;

	char buffer[size];

	unsigned long len = strlen(path);

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
