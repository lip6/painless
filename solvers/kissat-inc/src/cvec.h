#ifndef _cvec_h_INCLUDED
#define _cvec_h_INCLUDED

typedef struct cvec cvec;
struct cvec
{
    int *data;
    int sz, cap;
};

cvec *cvec_init();
void cvec_release(cvec *vec);
int cvec_data(cvec *vec, int id);
void cvec_setsize(cvec *vec, int v);
void cvec_push(cvec *vec, int v);
void cvec_clear(cvec *vec);
#endif