#ifndef FILE_H
#define FILE_H

void path_join(char *path, unsigned long size, const char *other);
const char *path_relative(const char *path, const char *dir);

#endif /* !FILE_H */
