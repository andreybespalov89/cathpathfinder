#ifndef CATH_NN_LEV_TOPM_H
#define CATH_NN_LEV_TOPM_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int score;
    int idx;
} TopmItem;

typedef struct {
    TopmItem *items;
    int size;
    int cap;
} TopmHeap;

void topm_init(TopmHeap *h, TopmItem *storage, int cap);
void topm_reset(TopmHeap *h);
void topm_consider(TopmHeap *h, int score, int idx);
int topm_size(const TopmHeap *h);
void topm_sort_desc(TopmHeap *h);

#ifdef __cplusplus
}
#endif

#endif
