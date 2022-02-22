#include "ply.h"
#include "../candle.h"
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_WORD 4095

enum value_type {
	PLY_NONE,
	PLY_FLOAT, PLY_DOUBLE,
	PLY_UCHAR, PLY_CHAR,
	PLY_INT8, PLY_INT, PLY_UINT8, PLY_UINT, PLY_LONG, PLY_ULONG,
	PLY_LIST
};

struct type_info
{
	char name[32];
	enum value_type type;
	char format[16];
};

static struct type_info g_types[] =
{
	{"float",   PLY_FLOAT,  " %lf"},
	{"float32", PLY_FLOAT,  " %lf"},
	{"float64", PLY_DOUBLE, " %lf"},
	{"double",  PLY_DOUBLE, " %lf"},

	{"uchar",   PLY_UCHAR,  " %c"},
	{"char",    PLY_CHAR,   " %c"},

	{"int8",    PLY_INT8,  " %ld"},
	{"int32",   PLY_INT,   " %ld"},
	{"int",     PLY_INT,   " %ld"},
	{"int64",   PLY_LONG,  " %ld"},

	{"uint8",   PLY_UINT8, " %ld"},
	{"uint32",  PLY_UINT,  " %ld"},
	{"uint",    PLY_UINT,  " %ld"},
	{"uint64",  PLY_ULONG, " %ld"},

	{"list", PLY_LIST},
	{"", PLY_NONE}
};



typedef struct
{
	char chars[MAX_WORD + 1];
} ply_word_t;

/* ////////////////////////////////////////////////////////////////////////// */
/*                                .PLY LOADER                                 */
/* ////////////////////////////////////////////////////////////////////////// */

static ply_word_t *get_words(FILE *fp, int *nwords)
{
#define _WORD_FORMAT(mx) " %" #mx "s"
#define WORD_FORMAT(mx) _WORD_FORMAT(mx)
	unsigned int i;

	ply_word_t *words;
	int num_words = 0;

	char word[MAX_WORD + 1];
	while(fscanf(fp, WORD_FORMAT(MAX_WORD), word) == 1)
	{
		num_words++;
	}

	rewind(fp);

	words = malloc(sizeof(ply_word_t) * num_words);

	for(i = 0; i < num_words; i++)
	{
		if(fscanf(fp, WORD_FORMAT(MAX_WORD), words[i].chars) == -1)
		{
			words[i].chars[0] = '\0';
		}
	}

	*nwords = num_words;
	return words;
}

static int get_type(const char *name)
{
	unsigned int i;
	for(i = 0;; i++)
	{
		struct type_info type = g_types[i];

		if(type.type == PLY_NONE) return -1;

		if(!strncmp(g_types[i].name, name, sizeof(g_types[i].name))) return i;
	}
	return -1;
}

void mesh_load_ply(mesh_t *self, const char *filename)
{
	unsigned int i;
	int nwords;
	ply_word_t *words;
	ply_word_t *word;
	FILE *fp;
	struct property
	{
		char name[32];
		int type, list_count_type, list_element_type;
	};
	struct element_type
	{
		char name[32];
		struct property props[10]; int props_num;
		int num;
	};
	struct element_type *elements = NULL;
	int elements_num = 0;
	int file_type = 0;


	strncpy(self->name, filename, sizeof(self->name) - 1);

    fp = fopen(filename, "r");

	if (!fp)
	{
		printf("Could not find object '%s'\n", filename);
		return;
	}


	#define FORMAT_ASCII		0x01
	#define FORMAT_BINARY_BE	0x02
	#define FORMAT_BINARY_LE	0x03

	words = get_words(fp, &nwords);

	if(!words || !!strcmp(words[0].chars, "ply"))
	{
		printf("Couldn't parse words in ply %s.\n", filename);
		exit(1);
	}

	/* READ HEADER */

	for(word = words; word->chars[0];)
	{
		if(!strcmp(word[0].chars, "format"))
		{
			word++;
			if(!strcmp(word->chars, "ascii"))
			{
				file_type = FORMAT_ASCII;
			}
			else if(!strcmp(word->chars, "binary_big_endian"))
			{
				file_type = FORMAT_BINARY_BE;
			}
			else if(!strcmp(word->chars, "binary_little_endian"))
			{
				file_type = FORMAT_BINARY_LE;
			}
			else
			{
				printf("Unknown ply format.\n");
				exit(1);
			}
			/* word[1] is version */
			word++;
		}
		else if(!strcmp(word->chars, "element"))
		{
			struct element_type *el;

			word++;

			i = elements_num++;
			elements = realloc(elements, sizeof(*elements) * elements_num);
			el = &elements[i];
			el->props_num = 0;
			
			memcpy(el->name, word->chars, sizeof(el->name)); word++;
			el->name[sizeof(el->name) - 1] = '\0';
			if(sscanf(word->chars, "%d", &el->num) < 0)
			{
				printf("Failed to parse ply.\n");
				exit(1);
			}
			word++;

			while(!strcmp(word->chars, "property"))
			{
				struct property *props;

				word++;

				props = &el->props[el->props_num];
				el->props_num++;

				props->type = get_type(word->chars); word++;

				if(g_types[props->type].type == PLY_LIST)
				{
					props->list_count_type = get_type(word->chars); word++;
					props->list_element_type = get_type(word->chars); word++;
				}
				strncpy(props->name, word->chars, sizeof(props->name) - 1); word++;
			}
		}
		else if(!strcmp(word->chars, "end_header"))
		{
			word++;
			break;
		}
		else word++;
	}

	/* READ DATA */

	if(file_type != FORMAT_ASCII)
	{
		printf("Format not yet supported.\n");
		exit(1);
	}

	for(i = 0; i < elements_num; i++)
	{
		struct element_type *el = &elements[i];

		int vertex = !strcmp(el->name, "vertex");
		int face = !strcmp(el->name, "face");

		unsigned int j, k;
		for(j = 0; j < el->num; j++)
		{
			double x, y, z;
			unsigned long count;
			unsigned long id[4];

			for(k = 0; k < el->props_num; k++)
			{
				struct property *prop = &el->props[k];
				struct type_info *type = &g_types[prop->type];
				if(type->type == PLY_LIST)
				{
					if(face)
					{
						unsigned long c;
						struct type_info *count_type =
							&g_types[prop->list_count_type];
						struct type_info *list_type =
							&g_types[prop->list_element_type];

						sscanf((word++)->chars, count_type->format, &count);
						for(c = 0; c < count; c++)
						{
							sscanf((word++)->chars, list_type->format, &id[c]);
						}
					}
				}
				else
				{
					if(vertex)
					{
						if(!strcmp(prop->name, "x"))
						{
							sscanf(word->chars, type->format, &x);
						}
						if(!strcmp(prop->name, "y"))
						{
							sscanf(word->chars, type->format, &y);
						}
						if(!strcmp(prop->name, "z"))
						{
							sscanf(word->chars, type->format, &z);
						}
						word++;
					}
				}
			}
			if(vertex) mesh_add_vert(self, VEC3((float)x, (float)y, (float)z));
			if(face)
			{

				vec3_t z3 = vec3(0.0f, 0.0f, 0.0f);
				vec2_t z2 = vec2(0.0f, 0.0f);
				if(count == 4)
				{
					mesh_add_quad(self,
							id[0], z3, z2,
							id[1], z3, z2,
							id[2], z3, z2,
							id[3], z3, z2);
				}
				else
				{
					mesh_add_triangle(self,
							id[0], z3, z2,
							id[1], z3, z2,
							id[2], z3, z2);
				}
			}
		}
	}
	self->has_texcoords = 0;

	free(elements);
	free(words);

    fclose(fp);
}

