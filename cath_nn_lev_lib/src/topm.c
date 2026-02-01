#include "topm.h"

#include <stdlib.h>

static int worse_than(const TopmItem *a, const TopmItem *b) {
    if (a->score != b->score) return a->score < b->score;
    return a->idx > b->idx;
}

static void sift_up(TopmHeap *h, int i) {
    while (i > 0) {
        int p = (i - 1) / 2;
        if (!worse_than(&h->items[i], &h->items[p])) break;
        TopmItem tmp = h->items[i];
        h->items[i] = h->items[p];
        h->items[p] = tmp;
        i = p;
    }
}

static void sift_down(TopmHeap *h, int i) {
    while (1) {
        int l = i * 2 + 1;
        int r = l + 1;
        int smallest = i;
        if (l < h->size && worse_than(&h->items[l], &h->items[smallest])) smallest = l;
        if (r < h->size && worse_than(&h->items[r], &h->items[smallest])) smallest = r;
        if (smallest == i) break;
        TopmItem tmp = h->items[i];
        h->items[i] = h->items[smallest];
        h->items[smallest] = tmp;
        i = smallest;
    }
}

void topm_init(TopmHeap *h, TopmItem *storage, int cap) {
    h->items = storage;
    h->size = 0;
    h->cap = cap;
}

void topm_reset(TopmHeap *h) {
    h->size = 0;
}

void topm_consider(TopmHeap *h, int score, int idx) {
    if (h->cap <= 0) return;
    TopmItem item = {score, idx};
    if (h->size < h->cap) {
        h->items[h->size] = item;
        sift_up(h, h->size);
        h->size++;
        return;
    }
    if (worse_than(&item, &h->items[0])) return;
    h->items[0] = item;
    sift_down(h, 0);
}

int topm_size(const TopmHeap *h) {
    return h->size;
}

static int cmp_desc(const void *a, const void *b) {
    const TopmItem *ia = (const TopmItem *)a;
    const TopmItem *ib = (const TopmItem *)b;
    if (ia->score != ib->score) return (ia->score > ib->score) ? -1 : 1;
    if (ia->idx < ib->idx) return -1;
    if (ia->idx > ib->idx) return 1;
    return 0;
}

void topm_sort_desc(TopmHeap *h) {
    qsort(h->items, h->size, sizeof(TopmItem), cmp_desc);
}
