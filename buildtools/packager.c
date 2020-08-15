
#include "../third_party/tinydir/tinydir.h"
#include "../third_party/miniz.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

static
void path_join(char *path, size_t size, const char *other)
{
	char buffer[1024];
	size_t len;

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

static
void add_path(mz_zip_archive *archive, const char *dir_name)
{
	mz_bool status;
	tinydir_dir dir;
	FILE *fp = fopen(dir_name, "rb");
	if (fp != NULL) {
		status = mz_zip_writer_add_file(archive, dir_name + 3, dir_name,
		                                "", 0, MZ_BEST_COMPRESSION);
		fclose(fp);
		return;
	}

	if (tinydir_open(&dir, dir_name) == -1)
	{
		return;
	}
	while (dir.has_next)
	{
		tinydir_file file;
		char *dot;
		char path[512];
		char buffer[64];

		if (tinydir_readfile(&dir, &file) == -1)
		{
			perror("Error getting file");
			goto next;
		}

		if (file.name[0] == '.')
		{
			goto next;
		}

		strncpy(path, dir_name, sizeof(path) - 1);
		path_join(path, sizeof(path), file.name);

		if (file.is_dir)
		{
			add_path(archive, path);
			goto next;
		}

		strcpy(buffer, file.name);

		dot = strrchr(buffer, '.');
		if (!dot || dot[1] == '\0')
		{
			goto next;
		}

		status = mz_zip_writer_add_file(archive, path + 3, path,
		                                "", 0, MZ_BEST_COMPRESSION);

next:
		if (tinydir_next(&dir) == -1)
		{
			perror("Error getting next file");
			return;
		}
	}

}

/* static */
/* void hexembed(const char *data, size_t data_size) */
/* { */
/*     printf("const unsigned int g_sauces_archive_size = %ld;\n", data_size); */
/*     printf("const unsigned int g_sauces_archive[] = {\n"); */

/* 	if (data_size > 0) */
/* 	{ */
/* 		unsigned int *data_word = (unsigned int*)data; */
/* 		size_t word_size = data_size / sizeof(*data_word); */
/* 		unsigned int last_word = 0; */
/* 		unsigned int remainder = data_size - word_size * 4; */
/* 		size_t i; */
/* 		for (i = 0; i < word_size; ++i) */
/* 		{ */
/* 			printf("0x%06x,", data_word[i]); */
/* 		} */

/* 		if (remainder) { */
/* 			memcpy(&last_word, &data[data_size % 4], remainder); */
/* 			printf("0x%06x", last_word); */
/* 		} else { */
/* 			printf("0x0", last_word); */
/* 		} */
/* 	} */
/*     printf("\n};\n"); */
/* } */

int main(int argc, char *argv[])
{
	int i;
	mz_bool status;
	mz_zip_archive archive = {0};

	if (   !mz_zip_reader_init_file(&archive, "build/data.zip", 0)
		|| !mz_zip_writer_init_from_reader(&archive, "build/data.zip"))
	{
		if (!mz_zip_writer_init_file(&archive, "build/data.zip", 2e+7))
		{
			return 1;
		}
	}

	for (i = 1; i < argc; ++i)
	{
		add_path(&archive, argv[i]);
	}
	if (!mz_zip_writer_finalize_archive(&archive))
	{
		return 1;
	}
	if (!mz_zip_writer_end(&archive))
	{
		return 1;
	}
	return 0;
}
