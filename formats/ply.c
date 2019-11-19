#include "ply.h"
#include <candle.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

enum value_type {
	NONE,
	FLOAT, DOUBLE,
	UCHAR, CHAR,
	INT8, INT, UINT8, UINT, LONG, ULONG,
	LIST
};

struct type_info
{
	char name[32];
	enum value_type type;
	char format[16];
};

static struct type_info g_types[] =
{
	{"float", 	FLOAT,	" %lf"},
	{"float32", FLOAT,	" %lf"},
	{"float64", DOUBLE,	" %lf"},
	{"double", DOUBLE,	" %lf"},

	{"uchar",	UCHAR,	" %c"},
	{"char",	CHAR,	" %c"},

	{"int8",	INT8,	" %ld"},
	{"int32",	INT,	" %ld"},
	{"int",		INT,	" %ld"},
	{"int64",	LONG,	" %ld"},

	{"uint8",	UINT8,	" %ld"},
	{"uint32",	UINT,	" %ld"},
	{"uint",	UINT,	" %ld"},
	{"uint64",	ULONG,	" %ld"},

	{"list", LIST},
	{"", NONE}
};




////////////////////////////////////////////////////////////////////////////////
//                                .PLY LOADER                                 //
////////////////////////////////////////////////////////////////////////////////

static char **get_words(FILE *fp, int *nwords)
{
#define MAX_WORD 4095
#define _WORD_FORMAT(mx) " %" #mx "s"
#define WORD_FORMAT(mx) _WORD_FORMAT(mx)
	unsigned int i;

	char **words;
	int num_words = 0;

	char word[MAX_WORD + 1];
	while(fscanf(fp, WORD_FORMAT(MAX_WORD), word) == 1)
	{
		num_words++;
	}

	rewind(fp);

	words =  malloc(sizeof(char *) * num_words);

	for(i = 0; i < num_words; i++)
	{
		if(fscanf(fp, WORD_FORMAT(MAX_WORD), word) == -1) word[0] = '\0';
		/* TODO: Make this memory contiguous */
		words[i] = strdup(word);
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

		if(type.type == NONE) return -1;

		if(!strncmp(g_types[i].name, name, sizeof(g_types[i].name))) return i;
	}
	return -1;
}

void mesh_load_ply(mesh_t *self, const char *filename)
{
	strncpy(self->name, filename, sizeof(self->name) - 1);

    FILE *fp = fopen(filename, "r");

	if(!fp)
	{
		printf("Could not find object '%s'\n", filename);
		return;
	}

	unsigned int i;
	int nwords;
	char **words;
	char **word;

	struct property
	{
		char name[32];
		int type,
			list_count_type,
			list_element_type;
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

	#define FORMAT_ASCII		0x01
	#define FORMAT_BINARY_BE	0x02
	#define FORMAT_BINARY_LE	0x03

	words = get_words(fp, &nwords);

	if(!words || !!strcmp(words[0], "ply")) exit(1);

	/* READ HEADER */

	for(word = words; *word;)
	{
		if(!strcmp(word[0], "format"))
		{
			word++;
			if(!strcmp(*word, "ascii"))
			{
				file_type = FORMAT_ASCII;
			}
			else if(!strcmp(*word, "binary_big_endian"))
			{
				file_type = FORMAT_BINARY_BE;
			}
			else if(!strcmp(*word, "binary_little_endian"))
			{
				file_type = FORMAT_BINARY_LE;
			}
			else
			{
				exit(1);
			}
			/* word[1] is version */
			word++;
		}
		else if(!strcmp(*word, "element"))
		{
			word++;

			i = elements_num++;
			elements = realloc(elements, sizeof(*elements) * elements_num);
			struct element_type *el = &elements[i];
			el->props_num = 0;
			
			strncpy(el->name, *word, sizeof(el->name) - 1); word++;
			if(sscanf(*word, "%d", &el->num) < 0) exit(1);
			word++;

			while(!strcmp(*word, "property"))
			{
				word++;

				struct property *props = &el->props[el->props_num];
				el->props_num++;

				props->type = get_type(*word); word++;

				if(g_types[props->type].type == LIST)
				{
					props->list_count_type = get_type(*word); word++;
					props->list_element_type = get_type(*word); word++;
				}
				strncpy(props->name, *word, sizeof(props->name) - 1); word++;
			}
		}
		else if(!strcmp(*word, "end_header"))
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
				if(type->type == LIST)
				{
					if(face)
					{
						unsigned long c;
						struct type_info *count_type =
							&g_types[prop->list_count_type];
						struct type_info *list_type =
							&g_types[prop->list_element_type];

						sscanf(*(word++), count_type->format, &count);
						for(c = 0; c < count; c++)
						{
							sscanf(*(word++), list_type->format, &id[c]);
						}
					}
				}
				else
				{
					if(vertex)
					{
						if(!strcmp(prop->name, "x"))
						{
							sscanf(*word, type->format, &x);
						}
						if(!strcmp(prop->name, "y"))
						{
							sscanf(*word, type->format, &y);
						}
						if(!strcmp(prop->name, "z"))
						{
							sscanf(*word, type->format, &z);
						}
						word++;
					}
				}
			}
			if(vertex) mesh_add_vert(self, VEC3((float)x, (float)y, (float)z));
			if(face)
			{

				vec3_t z3 = vec3(0.0);
				vec2_t z2 = vec2(0.0);
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
	for(i = 0; i < nwords; i++)
	{
		free(words[i]);
	}
	free(words);

    fclose(fp);
}

