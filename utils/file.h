#ifndef FILE_H
#define FILE_H

#include <stdlib.h>

void path_join(char *path, size_t size, const char *other);
const char *path_relative(const char *path, const char *dir);
char *filter_sauce_name(char *path_name);
void to_lower_case(char *str);
int is_dir(const char *f);

typedef struct sfile
{
	char **lines;
	size_t lines_num;
	size_t line;
	char bytes[1];
} sfile_t;

sfile_t *sopen(const char *bytes, size_t bytes_num);
void swind(sfile_t *fp);
char *sgets(sfile_t *fp);
void sclose(sfile_t *fp);

#endif /* !FILE_H */
