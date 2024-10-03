#include "cvec.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>

static inline int imax(int x, int y)
{
    int mask = (y - x) >> (sizeof(int) * 8 - 1);
    return (x & mask) + (y & (~mask));
}



void capInc(cvec *vec, int to_cap)
{
    if (vec->cap >= to_cap)
        return;
    int add = imax((to_cap - vec->cap + 1) & ~1, ((vec->cap >> 1) + 2) & ~1);
    vec->data = (int*)realloc(vec->data, (vec->cap += add) * sizeof(int));
}


cvec *cvec_init()
{
    cvec *v;
    v = (cvec *)malloc(sizeof(cvec));
    v->sz = 0;
    v->cap = 0;
    v->data = NULL;
    return v;
}

void cvec_release(cvec *vec)
{
    if (vec->data != NULL)
        free(vec->data);
    free(vec);
}

int cvec_data(cvec *vec, int id)
{
    assert(id < vec->sz);
    return vec->data[id];
}

void cvec_setsize(cvec *vec, int v) 
{ 
    vec->sz = v; 
}

void cvec_push(cvec *vec, int v)
{
    if (vec->sz == vec->cap)
        capInc(vec, vec->sz + 1);
    vec->data[vec->sz++] = v;
}

void cvec_clear(cvec *vec) {
    if (vec->data != NULL)
        vec->sz = 0;
}