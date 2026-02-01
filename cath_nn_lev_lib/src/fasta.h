#ifndef CATH_NN_LEV_FASTA_H
#define CATH_NN_LEV_FASTA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *id;
    char *header;
    size_t offset;
    int len;
} FastaSeq;

typedef struct {
    FastaSeq *seqs;
    int n;
    int cap;
    char *pool;
    size_t pool_len;
    size_t pool_cap;
    int max_len;
} FastaDB;

int fasta_read(const char *path, int strict, FastaDB *db, char *err, size_t err_cap);
void fasta_free(FastaDB *db);

#ifdef __cplusplus
}
#endif

#endif
