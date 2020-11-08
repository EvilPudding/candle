#include <candle.h>
#include "obj.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ////////////////////////////////////////////////////////////////////////// */
/*                                .OBJ LOADER                                 */
/* ////////////////////////////////////////////////////////////////////////// */

static char* obj__strsep(char** stringp, const char* delim)
{
	char* start = *stringp;
	char* p;

	p = (start != NULL) ? strpbrk(start, delim) : NULL;

	if (p == NULL)
	{
		*stringp = NULL;
	}
	else
	{
		*p = '\0';
		*stringp = p + 1;
	}

	return start;
}

struct vert {
	int v, t, n;
};
struct face {
	int nv;
	union {
		struct vert v[4];
		int l[12];
	} value;
};

static void ignore_line(const char **bytes, const char *end)
{
    while (*bytes < end)
	{
		const char ch = **bytes;
		++(*bytes);
		if (ch == '\n' || ch == '\r')
			return;
	}
}

static int should_ignore(char c)
{
	/* return c == '#' || c == 'u' || c == 'o' || c == 's' || c == 'm' ||
	 * c == 'g'; */
	return c != 'v' && c != 'f';
}

static int strgetf(const char **str, const char *end, float *value)
{
	int read_num = 0, ret;
	assert(*str < end);
	ret = sscanf(*str, " %f %n", value, &read_num);
	*str += read_num;
	return ret;
}

static bool_t strgets(const char **str, const char *end, char *line, size_t n)
{
	int i;

	line[n - 1] = '\0';
	for (i = 0; i < n && *str < end; ++i)
	{
		line[i] = **str;
		++(*str);
		if (line[i] == '\n' || line[i] == '\r')
			break;
	}
	if (i < n)
		line[i] = '\0';
	return i > 0;
}

static char strgetc(const char **str, const char *end)
{
	char ch;
	if (*str < end)
	{
		ch = **str;
		++(*str);
	}
	else
	{
		ch = EOF;
	}
	return ch;
}

static void count(const char *bytes, size_t bytes_num, int *numV, int *numVT,
                  int *numVN, int *numF)
{
	const char *str = bytes;
	const char *end = bytes + bytes_num;
	char ch;
    (*numV) = 0;
    (*numVT) = 0;
    (*numVN) = 0;
    (*numF) = 0;
	while ((ch = strgetc(&str, end)) != EOF)
    {
        if (should_ignore(ch))
		{
			ignore_line(&str, end);
		}
        else
		{
			switch (ch)
			{
				case 'v':
					ch = strgetc(&str, end);
					if (ch == 't')
					{
						(*numVT)++;
					}
					else if (ch == 'n')
					{
						(*numVN)++;
					}
					else
					{
						(*numV)++;
					}
					break;
				case 'f':
					(*numF)++;
					break;
			}
			ignore_line(&str, end);
		}
    }
}

static
char *obj__strtok_r(char *str, const char *delim, char **nextp)
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

static void read_prop(mesh_t *self, const char *bytes, size_t bytes_num,
                      vec3_t *tempNorm, vec2_t *tempText, struct face *tempFace)
{
    int n1 = 0, n2 = 0, n4 = 0;
	const char *str = bytes;
	const char *end = bytes + bytes_num;
	char line[512];
    while (strgets(&str, end, line, 512))
    {
        if (should_ignore(line[0]))
		{
			continue;
		}
        else
		{
			struct face *tf;
			char *line_i, *words, *token;

			int i;
			switch(line[0])
			{
				case 'v':
					if (line[1] == 't')
					{
						sscanf(line + 2, " %f %f", &tempText[n1].x, &tempText[n1].y);
						n1++;
					}
					else if (line[1] == 'n')
					{
						sscanf(line + 2, " %f %f %f", &tempNorm[n2].x,
						       &tempNorm[n2].y, &tempNorm[n2].z);
						n2++;
					}
					else
					{
						vec3_t pos;
						sscanf(line + 1, " %f %f %f", &pos.x, &pos.y, &pos.z);
						mesh_add_vert(self, VEC3(pos.x, pos.y, pos.z));
					}
					break;
				case 'f':
					tf = &tempFace[n4];
					line_i = line + 1;
					for (i = 0; (words = obj__strsep(&line_i, " ")) != NULL; i += 3)
					{
						int j;
						char *words_i = words;
						if (words[0] == '\0')
						{
							i -= 3;
							continue;
						}
						for (j = 0;(token = obj__strsep(&words_i, "/")) != NULL; j++)
						{
							if ((token[0] < '0' || token[0] > '9') ||
									sscanf(token, "%d", &(tf->value.l[i + j])) <= 0)
							{
								tf->value.l[i + j] = 0;
							}
							tf->value.l[i + j]--;
						}
					}
					tf->nv = i / 3;
					n4++;
			}
		}
    }
}

void mesh_load_obj(mesh_t *self, const char *bytes, size_t bytes_num)
{
    int i;
    int v, vt, vn, f;
	vec3_t *tempNorm;
	vec2_t *tempText;
	struct face *tempFace;

    count(bytes, bytes_num, &v, &vt, &vn, &f);
	printf("%s counts %d %d %d %d\n", self->name, v, vt, vn, f);
    tempNorm = malloc((vn + 1) * sizeof(vec3_t));
    tempText = malloc((vt + 1) * sizeof(vec2_t));
    tempFace = malloc(f * sizeof(struct face));

	tempNorm[0] = Z3;
	tempText[0] = Z2;
    read_prop(self, bytes, bytes_num, &tempNorm[1], &tempText[1], tempFace);
	for (i = 0; i < f; i++)
	{
		struct face *face = &tempFace[i];
		if (face->nv == 3)
		{
			if (face->value.v[0].t + 1 == 0)
				self->has_texcoords = false;

			mesh_add_triangle(self,
					face->value.v[0].v, tempNorm[face->value.v[0].n + 1], tempText[face->value.v[0].t + 1],
					face->value.v[1].v, tempNorm[face->value.v[1].n + 1], tempText[face->value.v[1].t + 1],
					face->value.v[2].v, tempNorm[face->value.v[2].n + 1], tempText[face->value.v[2].t + 1]);
		}
		else if (face->nv == 4)
		{
			mesh_add_quad(self,
					face->value.v[0].v, tempNorm[face->value.v[0].n + 1], tempText[face->value.v[0].t + 1],
					face->value.v[1].v, tempNorm[face->value.v[1].n + 1], tempText[face->value.v[1].t + 1],
					face->value.v[2].v, tempNorm[face->value.v[2].n + 1], tempText[face->value.v[2].t + 1],
					face->value.v[3].v, tempNorm[face->value.v[3].n + 1], tempText[face->value.v[3].t + 1]);
		}
	}

	free(tempNorm);
	free(tempText);
    free(tempFace);
	printf("\t\tend object %s\n", self->name);
}


