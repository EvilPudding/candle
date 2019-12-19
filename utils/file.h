#ifndef FILE_H
#define FILE_H

#include <stdlib.h>

void path_join(char *path, size_t size, const char *other);
const char *path_relative(const char *path, const char *dir);
char *filter_sauce_name(char *path_name);
void to_lower_case(char *str);
int is_dir(const char *f);


#endif /* !FILE_H */
