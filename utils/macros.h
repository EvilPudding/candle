#ifndef MACROS_H
#define MACROS_H
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define true 1
#define false 0
typedef uint32_t bool_t;

#define ECMHASH(str) murmur_hash(str, strlen(str), 0)
#define sig(str) murmur_hash(str, strlen(str), 0)
#define ref(str) murmur_hash(str, strlen(str), 0)
#define ent_comp_ref(cid, eid) murmur_hash(&cid, 4, eid)
#define TP2(a, b) a##b
#define TP(a, b) TP2(a, b)

#ifndef EMSCRIPTEN
__attribute__((always_inline))
__attribute__((optimize("unroll-loops")))
#endif
static inline uint32_t murmur_hash(const void *key, int32_t len, uint32_t seed)
{
    /* 32-Bit MurmurHash3: https://code.google.com/p/smhasher/wiki/MurmurHash3*/
    #define ROTL(x,r) ((x) << (r) | ((x) >> (32 - r)))
    /* union {const uint32_t *i; const char *b;} conv = {0}; */
    const char *data = (const char*)key;
    const int32_t nblocks = len/4;
    uint32_t h1 = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    const char *tail;
    /* const uint32_t *blocks; */
    uint32_t k1;
    int32_t i;
	const char *blocks = data + nblocks * 4;
    /* body */
    if (!key) return 0;
    /* conv.b = (data + nblocks*4); */
    /* blocks = (const uint32_t*)conv.i; */
    for (i = -nblocks; i; ++i) {
        /* k1 = blocks[i]; */
		memcpy(&k1, &blocks[i * 4], sizeof(uint32_t));
        k1 *= c1;
        k1 = ROTL(k1,15);
        k1 *= c2;

        h1 ^= k1;
        h1 = ROTL(h1,13);
        h1 = h1*5+0xe6546b64;
    }

    /* tail */
    tail = (const char*)(data + nblocks*4);
    k1 = 0;
    switch (len & 3) {
    case 3: k1 ^= (uint32_t)(tail[2] << 16); /* fallthrough */
    case 2: k1 ^= (uint32_t)(tail[1] << 8u); /* fallthrough */
    case 1: k1 ^= tail[0];
            k1 *= c1;
            k1 = ROTL(k1,15);
            k1 *= c2;
            h1 ^= k1;
            break;
    default: break;
    }

    /* finalization */
    h1 ^= (uint32_t)len;
    /* fmix32 */
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    #undef ROTL
    return h1;
}
static inline uint32_t murmur_hash_step(uint32_t h1, uint32_t block)
{
    /* 32-Bit MurmurHash3: https://code.google.com/p/smhasher/wiki/MurmurHash3*/
    #define ROTL(x,r) ((x) << (r) | ((x) >> (32 - r)))
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;
    uint32_t k1 = block;

    /* body */
	k1 *= c1;
	k1 = ROTL(k1,15);
	k1 *= c2;

	h1 ^= k1;
	h1 = ROTL(h1,13);
	h1 = h1*5+0xe6546b64;

    #undef ROTL
    return h1;
}

static inline uint32_t hash_ptr(void *ptr)
{
	return murmur_hash(&ptr, sizeof(ptr), 0);
}


#endif /* !MACROS_H */
