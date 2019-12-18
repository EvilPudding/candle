#ifndef FILE_H
#define FILE_H

#ifndef WIN32
#include <unistd.h>
#endif
#include <dirent.h>

#include <stdlib.h>
#include <utils/stdint.h>

void path_join(char *path, uint64_t size, const char *other);
const char *path_relative(const char *path, const char *dir);
char *filter_sauce_name(char *path_name);
void to_lower_case(char *str);

static int is_dir(const char *f)
{
	DIR *dir = opendir(f);
	if(dir == NULL) return 0;
	closedir(dir);
	return 1;
}


#endif /* !FILE_H */
