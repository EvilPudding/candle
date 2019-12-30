#ifndef STR_DEFAULT_CHUNK
#define STR_DEFAULT_CHUNK 32
#endif

#include <utils/str.h>
#include <stddef.h>

struct _str {
	size_t length;
	size_t chunk;
	size_t allocated;
	char bytes[1];
};

void strrepl(const char *, char *, const char *, const char *);

static struct _str *_str_head_ptr(const char *str)
{
	return (struct _str*)(str - offsetof(struct _str, bytes));
}

size_t str_chunk(const char *str)
{
	return _str_head_ptr(str)->chunk;
}

size_t str_alloc(const char *str)
{
	return _str_head_ptr(str)->allocated;
}

size_t str_len(const char *str)
{
	return _str_head_ptr(str)->length;
}

struct _str *_str_allocate(size_t size)
{
	return malloc(sizeof(struct _str) + (size + 1));
}

char *str_new_size(size_t size, size_t chunk)
{
	size_t real_size = (1 + ((size - 1) / chunk)) * chunk;

	struct _str *self = _str_allocate(real_size);

	self->allocated = real_size;

	self->length = 0;

	self->chunk = chunk;

	self->bytes[0] = '\0';

	return self->bytes;
}

char *str_new(size_t chunk)
{
	return str_new_size(0, chunk);
}

static void _str_cat(char **str1, const char *str2, size_t len2)
{
	size_t len1 = str_len(*str1);

	size_t final_len = len1 + len2;

	size_t allocated = str_alloc(*str1);
	if(final_len > allocated)
	{
		char *final_str = str_new_size(final_len, str_chunk(*str1));

		memcpy(final_str, (*str1), len1+1);

		str_free(*str1);
		(*str1) = final_str;

	}

	memcpy((*str1) + len1, str2, len2 + 1);
	/* (*str1)[final_len] = '\0'; */
	_str_head_ptr(*str1)->length = final_len;

}

void str_ncat(char **str1, const char *str2, size_t n)
{
	char *end = memchr(str2, '\0', n);
	const size_t len2 = end ? end - str2 : n;
	_str_cat(str1, str2, len2);
}

void str_cat(char **str1, const char *str2)
{
	const size_t len2 = strlen(str2);
	_str_cat(str1, str2, len2);
}

void str_catf( char **str, const char * format, ... )
{
	va_list args;
#if defined(STR_FORMAT_DYNAMIC)
	char *formated_str;
#elif defined(STR_FORMAT_SIZE)
	char formated_str[STR_FORMAT_SIZE];
#else
	char formated_str[STR_DEFAULT_FORMAT_SIZE];
#endif

	va_start(args, format);

#if defined(STR_FORMAT_DYNAMIC)
	formated_str = malloc(vsnprintf(NULL, 0, format, args) + 1);
#endif

	va_start(args, format);

	vsprintf(formated_str, format, args);
	/* vsprintf(buffer, format, args); */

	str_cat(str, formated_str);
	/* str_cat(str, buffer); */

	va_end(args);

#if defined(STR_FORMAT_DYNAMIC)
	free(formated_str);
#endif
}

char *str_dup(const char *str)
{
	size_t len = strlen(str);
	char *dup_str = str_new_size(len, STR_DEFAULT_CHUNK);

	memcpy(dup_str, str, len + 1);

	_str_head_ptr(dup_str)->length = len;

	return dup_str;
}

char *str_set_chunk(char *str, size_t chunk_size)
{
	_str_head_ptr(str)->chunk = chunk_size;
	return str;
}

char *str_new_copy(const char *str)
{
	size_t len = strlen(str);
	char *dup_str = str_new_size(len, str_chunk(str));

	memcpy(dup_str, str, len + 1);
	_str_head_ptr(dup_str)->length = len;

	return dup_str;
}

void str_free(const char *str)
{
	free(_str_head_ptr(str));
}

int str_count(const char *original, const char *search)
{
	int original_i = 0, search_i;
	int original_len = str_len(original);
	int search_len = strlen(search);
	int count = 0;

	while(original_i < original_len)
	{
		int match = 1;
		for(search_i = 0; search_i < search_len; search_i++)
		{
			if(original[original_i+search_i] != search[search_i])
			{
				match = 0;
				break;
			}
		}

		if(match)
		{
			count++;
			original_i += search_len;
		}
		else
		{
			original_i++;
		}

	}

	return count;
}

char *str_replace2(const char *original, const char *search, const char *replace)
{
	char *temp = str_replace(original, search, replace);
	str_free(original);
	return temp;
}

char *str_replace(const char *original, const char *search, const char *replace)
{
	size_t original_i = 0,
		output_i = 0,
		search_i,
		replace_i;

	size_t original_len = str_len(original);
	size_t search_len = strlen(search),
		replace_len = strlen(replace);
	size_t original_chunk = str_chunk(original);

	int count = str_count(original, search);
	size_t output_len =
		original_len + count * (replace_len - search_len);

	char *output = str_new_size(output_len, original_chunk);
	_str_head_ptr(output)->length = output_len;

	while(original_i < original_len)
	{
		int match = 1;
		output[output_i] = original[original_i];

		for(search_i = 0; search_i < search_len; search_i++)
		{
			if(original[original_i+search_i] != search[search_i])
			{
				match = 0;
				break;
			}
		}

		if(match)
		{
			count--;
			for(replace_i = 0; replace_i < replace_len; replace_i++, output_i++)
			{
				output[output_i] = replace[replace_i];
			}
			original_i += search_len;
			if(!count)
			{
				memcpy(output+output_i, original+original_i,
				       original_len - original_i);
				output_i += original_len - original_i;
				break;
			}
		}
		else
		{
			original_i++;
			output_i++;
		}

	}

	output[output_i] = '\0';

	return output;
}

void strrepl(const char *original, char *output,
		const char *search, const char *replace)
{
	int original_i = 0,
		output_i = 0,
		search_i,
		replace_i;

	int original_len = strlen(original);
	int search_len = strlen(search),
		replace_len = strlen(replace);

	while(original_i < original_len)
	{
		int match = 1;
		output[output_i] = original[original_i];

		for(search_i = 0; search_i < search_len; search_i++)
		{
			if(original[original_i+search_i] != search[search_i])
			{
				match = 0;
				break;
			}
		}

		if(match)
		{
			for(replace_i = 0; replace_i < replace_len;
					replace_i++, output_i++)
			{
				output[output_i] = replace[replace_i];
			}
			original_i += search_len;
		}
		else
		{
			original_i++;
			output_i++;
		}

	}
	output[output_i] = '\0';

}

char *str_readline(FILE *fp)
{
    char *line = str_new(512);
	char block[512];
    while (1)
	{
		char *last_char;

        if (fgets(block, 512, fp) == NULL)
		{
			break;
		}
		str_cat(&line, block);
		last_char = &block[strlen(block) - 1];
		if (*last_char == '\n' || *last_char == '\r')
		{
			/* *last_char = '\0'; */
			/* str_cat(&line, block); */
			break;
		}
    }
    return line;
}
