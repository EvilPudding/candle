
#include "common.frag"
#line 4

layout (location = 0) out vec4 FragColor;

uniform pass_t buf;

void main()
{
    FragColor = pass_sample(buf, texcoord);
}

// vim: set ft=c:
