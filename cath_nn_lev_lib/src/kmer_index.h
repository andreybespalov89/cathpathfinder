#ifndef CATH_NN_LEV_KMER_INDEX_H
#define CATH_NN_LEV_KMER_INDEX_H

#include <stdint.h>
#include <stddef.h>
#include "fasta.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t code;
    uint32_t seq_id;
} KmerEntry;

typedef struct {
    uint32_t *codes;
    uint32_t *start;
    uint32_t *end;
    uint32_t *df;
    size_t n_unique;
    KmerEntry *entries;
    size_t n_entries;
    int k;
} KmerIndex;

int kmer_index_build(const FastaDB *db, int k, KmerIndex *index, char *err, size_t err_cap);
void kmer_index_free(KmerIndex *index);
int kmer_code_for_window(const char *s, int k, uint32_t *code_out);
int kmer_index_find(const KmerIndex *index, uint32_t code, size_t *pos_out);

#ifdef __cplusplus
}
#endif

#endif
