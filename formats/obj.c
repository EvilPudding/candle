#include "obj.h"
#include <candle.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////
//                                .OBJ LOADER                                 //
////////////////////////////////////////////////////////////////////////////////

struct vert {
	int v, t, n;
};
struct face {
	int nv;
	union {
		struct vert v[4];
		int l[12];
	};
};

static void ignoreLine(FILE *fp)
{
    char ch;
    while((ch=fgetc(fp)) != EOF && ch!='\n'); //printf("%c",ch);
}

static int isToIgnore(char c)
{
	/* return c == '#' || c == 'u' || c == 'o' || c == 's' || c == 'm' ||
	 * c == 'g'; */
	return c != 'v' && c != 'f';
}


static void count(FILE *fp, int *numV, int *numVT, int *numVN, int *numF)
{
    char ch;
    (*numV) = 0;
    (*numVT) = 0;
    (*numVN) = 0;
    (*numF) = 0;
    rewind(fp);
    while((ch = fgetc(fp)) != EOF )
    {
        if(isToIgnore(ch))
		{
			ignoreLine(fp);
		}
        else
		{
			switch (ch)
			{
				case 'v':
					if((ch = fgetc(fp)) == 't')
						(*numVT)++;
					else if(ch == 'n')
						(*numVN)++;
					else (*numV)++;
					break;
				case 'f':(*numF)++;
			}
		}
    }
    rewind(fp);
}

static void readProperty(mesh_t *self, FILE * fp, vec3_t *tempNorm,
		vec2_t *tempText, struct face *tempFace)
{
    rewind(fp);
    int n1 = 0, n2 = 0, n3 = 0, n4 = 0;
    char ch;
	int ret;
    while((ch = fgetc(fp)) != EOF)
    {
        if(isToIgnore(ch))
		{
			ignoreLine(fp);
		}
        else
		{
			char *line = malloc(512);
			int i;
			switch(ch)
			{
				case 'v':
					if((ch=fgetc(fp))=='t')
					{
						fgetc(fp);
						ret = fscanf(fp,"%f", &(tempText[n1].x));fgetc(fp);
						if(!ret) exit(1);
						ret = fscanf(fp,"%f", &(tempText[n1].y));fgetc(fp);
						if(!ret) exit(1);
						n1++;
					}
					else if(ch=='n')
					{
						fgetc(fp);
						ret = fscanf(fp,"%f", &(tempNorm[n2].x));fgetc(fp);
						if(!ret) exit(1);
						ret = fscanf(fp,"%f", &(tempNorm[n2].y));fgetc(fp);
						if(!ret) exit(1);
						ret = fscanf(fp,"%f", &(tempNorm[n2].z));fgetc(fp);
						if(!ret) exit(1);
						n2++;
					}
					else
					{
						vec3_t pos;
						ret = fscanf(fp,"%f", &pos.x);fgetc(fp);
						if(!ret) exit(1);
						ret = fscanf(fp,"%f", &pos.y);fgetc(fp);
						if(!ret) exit(1);
						ret = fscanf(fp,"%f", &pos.z);fgetc(fp);
						mesh_add_vert(self, VEC3(_vec3(pos)));
						if(!ret) exit(1);
						n3++;
					}
					break;
				case 'f':
					if(fgets(line, 512, fp) == NULL) exit(1);
					struct face *tf = &tempFace[n4];

					char *line_i = line, *words, *token;
					for(i = 0;(words = strsep(&line_i, " ")) != NULL; i += 3)
					{
						char *words_i = words;
						if(words[0] == '\0')
						{
							i -= 3;
							continue;
						}
						int j;
						for(j = 0;(token = strsep(&words_i, "/")) != NULL; j++)
						{
							if((token[0] < '0' || token[0] > '9') ||
									sscanf(token, "%d", &(tf->l[i + j])) <= 0)
							{
								tf->l[i + j] = 0;
							}
							tf->l[i + j]--;
						}
					}
					tf->nv = i / 3;

					n4++;
			}
			free(line);
		}
    }
}

void mesh_load_obj(mesh_t *self, const char *filename)
{
	char buffer[2048];
	snprintf(buffer, sizeof(buffer), "resauces/models/%s", filename);

	strncpy(self->name, filename, sizeof(self->name));

    int i;
    FILE *fp = fopen(buffer, "r");
    int v, vt, vn, f;

	if(!fp)
	{
		printf("Could not find object '%s'\n", filename);
		return;
	}

    count(fp, &v, &vt, &vn, &f);
    vec3_t *tempNorm = malloc(vn * sizeof(vec3_t));
    vec2_t *tempText = malloc(vt * sizeof(vec2_t));
    struct face *tempFace = malloc(f * sizeof(struct face));

    readProperty(self, fp, tempNorm, tempText, tempFace);


	for(i=0;i<f;i++)
	{
		struct face *face = &tempFace[i];
		if(face->nv == 3)
		{
			mesh_add_triangle(self,
					face->v[0].v, tempNorm[face->v[0].n], tempText[face->v[0].t],
					face->v[1].v, tempNorm[face->v[1].n], tempText[face->v[1].t],
					face->v[2].v, tempNorm[face->v[2].n], tempText[face->v[2].t], 1);
		}
		else if(face->nv == 4)
		{
			mesh_add_quad(self,
					face->v[0].v, tempNorm[face->v[0].n], tempText[face->v[0].t],
					face->v[1].v, tempNorm[face->v[1].n], tempText[face->v[1].t],
					face->v[2].v, tempNorm[face->v[2].n], tempText[face->v[2].t],
					face->v[3].v, tempNorm[face->v[3].n], tempText[face->v[3].t]);
		}
	}

	free(tempNorm);
	free(tempText);
    free(tempFace);

    fclose(fp);

}


