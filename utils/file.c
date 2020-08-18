#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ecs/ecm.h>
#include <third_party/tinydir/tinydir.h>

void path_join(char *path, size_t size, const char *other)
{
	char buffer[1024];
	size_t len;

	/* printf("\t\t%s + %s\n", path, other); */
	if(other == NULL) return;
	if(other[0] == '/') other++;
	if(other[0] == '\0') return;

	len = strlen(path);

	if(path[len] == '/') path[len] = '\0';
	strncpy(buffer, path, size);

	if(path[0] == '\0')
	{
		strncpy(path, other, size);
	}
	else
	{
		sprintf(path, "%s/%s", buffer, other);
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
	char *slash = strrchr(path_name, '/');

	if (!slash) slash = strrchr(path_name, '\\');

	if (slash) path_name = slash + 1;
	/* char *dot = strrchr(path_name, '.'); */
	/* if(dot) *dot = '\0'; */
	to_lower_case(path_name);
	return path_name;
}

int is_dir(const char *f)
{
	tinydir_file file;
	if (tinydir_file_open(&file, f) == -1)
	{
		return 0;
	}
	return file.is_dir;
}

static
char *file__strtok_r(char *str, const char *delim, char **nextp)
{
    char *ret;
    if (str == NULL) str = *nextp;
    str += strspn(str, delim);
    if (*str == '\0') return NULL;
    ret = str;
    str += strcspn(str, delim);
    if (*str) *str++ = '\0';
    *nextp = str;
    return ret;
}

sfile_t *sopen(const char *bytes, size_t bytes_num)
{
	char *saveptr;
	char *newline;
	char *str;
	sfile_t *fp = malloc(sizeof(sfile_t) + bytes_num + 1);
	memcpy(fp->bytes, bytes, bytes_num);
	fp->bytes[bytes_num] = '\0';
	fp->lines = malloc(sizeof(*fp->lines));
	fp->lines_num = 0;
	fp->line = 0;

	str = fp->bytes;
	while ((newline = file__strtok_r(str, "\n", &saveptr)))
	{
		str = NULL;
		fp->lines = realloc(fp->lines, (fp->lines_num + 1) * sizeof(*fp->lines));
		fp->lines[fp->lines_num] = newline;
		fp->lines_num++;
	}

	return fp;
}

void swind(sfile_t *fp)
{
	fp->line = 0;
}

char *sgets(sfile_t *fp)
{
	char *line = NULL;
	if (fp->line < fp->lines_num)
	{
		line = fp->lines[fp->line];
		fp->line++;
	}
	return line;
}

void sclose(sfile_t *fp)
{
	free(fp);
}

