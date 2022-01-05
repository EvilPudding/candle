
#include "../third_party/tinydir/tinydir.h"
#include "../third_party/miniz.c"
#include "../third_party/pstdint.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef struct pack__file
{
	char path[512];
	uint64_t epoch_modified;
	enum {
		PACK__FILE_OLD,
		PACK__FILE_NEW
	} type;
} pack__file_t;

#ifdef _WIN32
#define PSEP "\\"
#include <windows.h>
#include <fileapi.h>
static
int file_epoch_modified(pack__file_t *file)
{
	HANDLE fh;
	FILETIME ft;
	ULARGE_INTEGER ft_int64;

	fh = CreateFile(file->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
	                NULL, OPEN_EXISTING, 0, NULL);

	if (fh == INVALID_HANDLE_VALUE)
	{
		CloseHandle(fh);
		return 1;
	}

	if (GetFileTime(fh, NULL, NULL, &ft) == 0)
	{
		CloseHandle(fh);
		return 1;
	}

	ft_int64.HighPart = ft.dwHighDateTime;
	ft_int64.LowPart = ft.dwLowDateTime;

	// Convert from hectonanoseconds since 01/01/1601 to seconds since 01/01/1970
	file->epoch_modified = ft_int64.QuadPart / 10000000ULL - 11644473600ULL - 1;

	CloseHandle(fh);
	return 0;
}
#else
#define PSEP "/"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
static
uint64_t file_epoch_modified(pack__file_t *file)
{
	struct stat st;
	stat(file->path, &st);
	file->epoch_modified = st.st_mtime - 1;
}
#endif

static
void path_join(char *path, size_t size, const char *other)
{
	char buffer[1024];
	size_t len;

	if(other == NULL) return;
	if(other[0] == '/' || other[0] == '\\') other++;
	if(other[0] == '\0') return;

	len = strlen(path);

	if(path[len] == '/' || path[len] == '\\') path[len] = '\0';
	strncpy(buffer, path, size);

	if(path[0] == '\0')
	{
		strncpy(path, other, size);
	}
	else
	{
		sprintf(path, "%s"PSEP"%s", buffer, other);
	}
}

void file_check(pack__file_t *file, mz_zip_archive *archive,
                int *rebuild_archive, int *new_files)
{
	mz_uint32 i;
	if (   mz_zip_get_type(archive) != MZ_ZIP_TYPE_INVALID
	    && mz_zip_reader_locate_file_v2(archive, file->path + 3, NULL, 0, &i))
	{
		mz_zip_archive_file_stat file_stat;
		if (mz_zip_reader_file_stat(archive, i, &file_stat))
		{
			if (file->epoch_modified <= file_stat.m_time)
			{
				file->type = PACK__FILE_OLD;
			}
			else
			{
				printf("file is newer %s %ld %ld\n", file->path,
				       file->epoch_modified, file_stat.m_time);
				file->type = PACK__FILE_NEW;
				*rebuild_archive = 1;
			}
			return;
		}
	}
	file->type = PACK__FILE_NEW;
	*new_files = 1;
}

static
void add_path(const char *dir_name, pack__file_t **files, int *num_files,
              mz_zip_archive *archive, int *rebuild_archive, int *new_files)
{
	tinydir_dir dir;

	if (tinydir_open(&dir, dir_name) == -1)
	{
		FILE *fp = fopen(dir_name, "rb");
		if (fp != NULL) {
			pack__file_t file;
			strncpy(file.path, dir_name, sizeof(file.path) - 1);
			file_epoch_modified(&file);
			file_check(&file, archive, rebuild_archive, new_files);

			(*num_files)++;
			*files = realloc(*files, (*num_files) * sizeof(**files));
			(*files)[*num_files - 1] = file;
			fclose(fp);

			return;
		}
	}
	while (dir.has_next)
	{
		tinydir_file dfile;
		char *dot;
		pack__file_t file;
		char buffer[256];

		if (tinydir_readfile(&dir, &dfile) == -1)
		{
			perror("Error getting file");
			goto next;
		}

		if (dfile.name[0] == '.')
		{
			goto next;
		}

		strncpy(file.path, dir_name, sizeof(file.path) - 1);
		path_join(file.path, sizeof(file.path), dfile.name);

		if (dfile.is_dir)
		{
			add_path(file.path, files, num_files, archive, rebuild_archive,
			         new_files);
			goto next;
		}

		strcpy(buffer, dfile.name);

		dot = strrchr(buffer, '.');
		if (!dot || dot[1] == '\0')
		{
			goto next;
		}

		file_epoch_modified(&file);
		file_check(&file, archive, rebuild_archive, new_files);

		(*num_files)++;
		*files = realloc(*files, (*num_files) * sizeof(**files));
		(*files)[*num_files - 1] = file;

next:
		if (tinydir_next(&dir) == -1)
		{
			perror("Error getting next file");
			return;
		}
	}
}

int main(int argc, char *argv[])
{
	int i;
	mz_bool status;
	mz_zip_archive read_archive = {0};
	mz_zip_archive write_archive = {0};

	pack__file_t *files = NULL;
	int num_files = 0;
	int rebuild_archive = 0;
	int new_files = 0;

	mz_zip_reader_init_file(&read_archive, "build" PSEP "data.zip", 0);

	for (i = 1; i < argc; ++i)
	{
		add_path(argv[i], &files, &num_files, &read_archive, &rebuild_archive,
		         &new_files);
	}

	/* NOTE: no delete code meant to only update or append, but not delete */
	for (i = 0; i < (int)mz_zip_reader_get_num_files(&read_archive); i++)
	{
		mz_zip_archive_file_stat file_stat;
		int found = 0;
		if (!mz_zip_reader_file_stat(&read_archive, i, &file_stat))
		{
			mz_zip_error err;
			while ((err = mz_zip_get_last_error(&read_archive)) != MZ_ZIP_NO_ERROR)
				puts(mz_zip_get_error_string(err));
			continue;
		}

		if (!mz_zip_reader_is_file_a_directory(&read_archive, i))
		{
			int j;
			for (j = 0; j < num_files; ++j)
			{
				if (!strcmp(files[j].path + 3, file_stat.m_filename))
				{
					found = 1;
					break;
				}
			}
			if (!found)
			{
				rebuild_archive = 1;
				/* pack__file_t file; */
				/* strcpy(file.path, "../"); */
				/* strcat(file.path, file_stat.m_filename); */
				/* file.type = PACK__FILE_OLD; */

				/* num_files++; */
				/* files = realloc(files, num_files * sizeof(*files)); */
				/* files[num_files - 1] = file; */
			}
		}
	}

	if (rebuild_archive)
	{
		if (!mz_zip_writer_init_file(&write_archive, "build"PSEP"new.zip", 0))
		{
			mz_zip_error err;
			printf("Failed to create new zip\n");
			while ((err = mz_zip_get_last_error(&write_archive)) != MZ_ZIP_NO_ERROR)
				puts(mz_zip_get_error_string(err));
			return 1;
		}

		for (i = 0; i < num_files; ++i)
		{
			if (files[i].type == PACK__FILE_NEW)
			{
				printf("archiving '%s'\n", files[i].path);
				if (!mz_zip_writer_add_file(&write_archive, files[i].path + 3,
				                            files[i].path, "", 0,
				                            MZ_BEST_COMPRESSION))
				{
					mz_zip_error err;
					printf("Failed update\n");
					while ((err = mz_zip_get_last_error(&write_archive)) != MZ_ZIP_NO_ERROR)
						puts(mz_zip_get_error_string(err));
					return 1;
				}
			}
			else
			{
				mz_uint32 j;
				if (   !mz_zip_reader_locate_file_v2(&read_archive,
				                                     files[i].path + 3,
				                                     NULL, 0, &j)
				    || !mz_zip_writer_add_from_zip_reader(&write_archive,
				                                          &read_archive, j))
				{
					mz_zip_error err;
					printf("Failed re-insert\n");
					while ((err = mz_zip_get_last_error(&write_archive)) != MZ_ZIP_NO_ERROR)
						puts(mz_zip_get_error_string(err));
					return 1;
				}
			}
		}

		if (   !mz_zip_writer_finalize_archive(&write_archive)
		    || !mz_zip_writer_end(&write_archive)
			|| !mz_zip_reader_end(&read_archive))
		{
			printf("Failed to rebuild\n");
			return 1;
		}

		remove("build"PSEP"data.zip");
		rename("build"PSEP"new.zip", "build"PSEP"data.zip");
	}
	else if (new_files)
	{
		if (mz_zip_writer_init_from_reader(&read_archive, "build"PSEP"data.zip"))
		{
			write_archive = read_archive;
		}
		else
		{
			mz_zip_reader_end(&read_archive);
			if (!mz_zip_writer_init_file(&write_archive, "build"PSEP"data.zip", 0))
			{
				printf("Failed create new\n");
				mz_zip_error err;
				while ((err = mz_zip_get_last_error(&write_archive)) != MZ_ZIP_NO_ERROR)
					puts(mz_zip_get_error_string(err));
				return 1;
			}
		}

		for (i = 0; i < num_files; ++i)
		{
			if (files[i].path[0] != '\0' && files[i].type == PACK__FILE_NEW)
			{
				printf("archiving '%s'\n", files[i].path);
				if (!mz_zip_writer_add_file(&write_archive, files[i].path + 3,
											files[i].path, "", 0,
											MZ_BEST_COMPRESSION))
				{
					mz_zip_error err;
					printf("Failed append\n");
					while ((err = mz_zip_get_last_error(&write_archive)) != MZ_ZIP_NO_ERROR)
						puts(mz_zip_get_error_string(err));
					return 1;
				}
			}
		}

		if (   !mz_zip_writer_finalize_archive(&write_archive)
		    || !mz_zip_writer_end(&write_archive))
		{
			printf("Failed to finalize new\n");
			return 1;
		}
	}
	return 0;
}
