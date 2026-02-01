#ifndef CATH_NN_LEV_PIPELINE_H
#define CATH_NN_LEV_PIPELINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int n_sequences;
    uint64_t total_kmers;
    int k;
    int M;
    int df_max;
    int threads_used;
    double time_read_s;
    double time_index_s;
    double time_query_s;
    double time_total_s;
} PipelineStats;

int pipeline_find_pairs(
    const char *fasta_path,
    const char *out_pairs_path,
    int k,
    int M,
    int df_max,
    int threads,
    int strict,
    const char *directed_path,
    int verbose,
    PipelineStats *stats,
    char *err,
    size_t err_cap
);

#ifdef __cplusplus
}
#endif

#endif
