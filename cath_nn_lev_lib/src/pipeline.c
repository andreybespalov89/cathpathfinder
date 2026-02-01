#include "pipeline.h"

#include "fasta.h"
#include "kmer_index.h"
#include "levenshtein.h"
#include "topm.h"
#include "util.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    int idx;
    int best_idx;
    int best_dist;
    int best_score;
    int candidates_examined;
} DirectedResult;

typedef struct {
    int a;
    int b;
    int dist;
} PairResult;

typedef struct {
    const FastaDB *db;
    const KmerIndex *index;
    int df_max;
    int M;
    int start;
    int end;
    int max_len;
    DirectedResult *results;
} ThreadWork;

static int cmp_pair(const void *a, const void *b) {
    const PairResult *pa = (const PairResult *)a;
    const PairResult *pb = (const PairResult *)b;
    if (pa->a != pb->a) return (pa->a < pb->a) ? -1 : 1;
    if (pa->b != pb->b) return (pa->b < pb->b) ? -1 : 1;
    return 0;
}

static void *thread_worker(void *arg) {
    ThreadWork *work = (ThreadWork *)arg;
    const FastaDB *db = work->db;
    const KmerIndex *index = work->index;
    int n = db->n;
    int k = index->k;

    int *score = (int *)malloc(sizeof(int) * n);
    TopmItem *heap_buf = (TopmItem *)malloc(sizeof(TopmItem) * work->M);
    int *prev = (int *)malloc(sizeof(int) * (2 * work->max_len + 3));
    int *curr = (int *)malloc(sizeof(int) * (2 * work->max_len + 3));

    if (!score || !heap_buf || !prev || !curr) {
        free(score); free(heap_buf); free(prev); free(curr);
        return NULL;
    }

    TopmHeap heap;
    topm_init(&heap, heap_buf, work->M);

    for (int q = work->start; q < work->end; q++) {
        memset(score, 0, sizeof(int) * n);
        score[q] = -2147483647;

        const FastaSeq *qseq = &db->seqs[q];
        const char *qs = db->pool + qseq->offset;

        if (qseq->len >= k) {
            for (int pos = 0; pos <= qseq->len - k; pos++) {
                uint32_t code = 0;
                if (!kmer_code_for_window(qs + pos, k, &code)) continue;
                size_t upos = 0;
                if (!kmer_index_find(index, code, &upos)) continue;
                if ((int)index->df[upos] > work->df_max) continue;
                uint32_t start = index->start[upos];
                uint32_t end = index->end[upos];
                for (uint32_t i = start; i < end; i++) {
                    uint32_t sid = index->entries[i].seq_id;
                    score[sid] += 1;
                }
            }
        }

        topm_reset(&heap);
        for (int i = 0; i < n; i++) {
            if (i == q) continue;
            topm_consider(&heap, score[i], i);
        }
        topm_sort_desc(&heap);

        int best_idx = -1;
        int best_dist = 2147483647;
        int best_score = -2147483647;
        int candidates_examined = 0;

        for (int c = 0; c < heap.size; c++) {
            int idx = heap.items[c].idx;
            int sc = heap.items[c].score;
            const FastaSeq *sseq = &db->seqs[idx];

            if (best_dist == 0) break;
            int len_diff = iabs_int(qseq->len - sseq->len);
            if (len_diff >= best_dist) continue;
            int limit = (best_dist == 2147483647) ? work->max_len : best_dist - 1;
            if (limit < 0) limit = 0;
            if (len_diff > limit) continue;

            candidates_examined++;
            int dist = levenshtein_banded(qs, qseq->len, db->pool + sseq->offset, sseq->len, limit, prev, curr);
            if (dist > limit) continue;

            if (dist < best_dist ||
                (dist == best_dist && sc > best_score) ||
                (dist == best_dist && sc == best_score && idx < best_idx)) {
                best_dist = dist;
                best_score = sc;
                best_idx = idx;
            }
        }

        if (best_idx < 0) {
            int fallback = (q == 0) ? 1 : 0;
            const FastaSeq *sseq = &db->seqs[fallback];
            int limit = work->max_len;
            int dist = levenshtein_banded(qs, qseq->len, db->pool + sseq->offset, sseq->len, limit, prev, curr);
            best_idx = fallback;
            best_dist = dist;
            best_score = score[best_idx];
        }

        DirectedResult *res = &work->results[q];
        res->idx = q;
        res->best_idx = best_idx;
        res->best_dist = best_dist;
        res->best_score = best_score;
        res->candidates_examined = candidates_examined;
    }

    free(score);
    free(heap_buf);
    free(prev);
    free(curr);
    return NULL;
}

static int write_directed(const char *path, const FastaDB *db, const DirectedResult *res, int n, char *err, size_t err_cap) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        snprintf(err, err_cap, "cannot write directed output: %s", path);
        return 0;
    }
    for (int i = 0; i < n; i++) {
        const DirectedResult *r = &res[i];
        const FastaSeq *s = &db->seqs[r->idx];
        const FastaSeq *b = &db->seqs[r->best_idx];
        fprintf(fp, "%d\t%s\t%d\t%s\t%d\t%d\t%d\n",
            r->idx, s->id, r->best_idx, b->id, r->best_dist, r->candidates_examined, r->best_score);
    }
    fclose(fp);
    return 1;
}

static int write_pairs(const char *path, const FastaDB *db, const DirectedResult *res, int n, char *err, size_t err_cap) {
    PairResult *pairs = (PairResult *)malloc(sizeof(PairResult) * n);
    if (!pairs) {
        snprintf(err, err_cap, "out of memory");
        return 0;
    }
    for (int i = 0; i < n; i++) {
        int a = res[i].idx;
        int b = res[i].best_idx;
        if (a > b) {
            int tmp = a; a = b; b = tmp;
        }
        pairs[i].a = a;
        pairs[i].b = b;
        pairs[i].dist = res[i].best_dist;
    }

    qsort(pairs, n, sizeof(PairResult), cmp_pair);

    FILE *fp = fopen(path, "w");
    if (!fp) {
        snprintf(err, err_cap, "cannot write pairs output: %s", path);
        free(pairs);
        return 0;
    }

    int last_a = -1, last_b = -1;
    for (int i = 0; i < n; i++) {
        if (pairs[i].a == last_a && pairs[i].b == last_b) continue;
        last_a = pairs[i].a;
        last_b = pairs[i].b;
        const FastaSeq *sa = &db->seqs[pairs[i].a];
        const FastaSeq *sb = &db->seqs[pairs[i].b];
        fprintf(fp, "%s\t%s\t%d\t%d\t%d\t%d\t%d\t%s\n",
            sa->id, sb->id,
            pairs[i].a, pairs[i].b,
            sa->len, sb->len,
            pairs[i].dist,
            "kmer_topm+lev");
    }

    fclose(fp);
    free(pairs);
    return 1;
}

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
) {
    double t0 = now_seconds();
    FastaDB db;
    KmerIndex index;
    memset(&db, 0, sizeof(db));
    memset(&index, 0, sizeof(index));

    if (!fasta_read(fasta_path, strict, &db, err, err_cap)) {
        return 0;
    }
    if (db.n < 2) {
        snprintf(err, err_cap, "need at least 2 sequences");
        fasta_free(&db);
        return 0;
    }
    double t_read = now_seconds();

    if (M <= 0) {
        snprintf(err, err_cap, "M must be positive");
        fasta_free(&db);
        return 0;
    }
    if (k <= 0) {
        snprintf(err, err_cap, "k must be positive");
        fasta_free(&db);
        return 0;
    }

    if (!kmer_index_build(&db, k, &index, err, err_cap)) {
        fasta_free(&db);
        return 0;
    }
    double t_index = now_seconds();

    int n = db.n;
    if (df_max <= 0) {
        int calc = (int)(0.02 * n);
        if (calc < 2000) calc = 2000;
        df_max = calc;
    }

    int cpu = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (threads <= 0) threads = cpu > 0 ? cpu : 1;
    if (threads > n) threads = n;

    int eff_M = M;
    if (eff_M > n - 1) eff_M = n - 1;

    DirectedResult *results = (DirectedResult *)calloc((size_t)n, sizeof(DirectedResult));
    if (!results) {
        snprintf(err, err_cap, "out of memory");
        kmer_index_free(&index);
        fasta_free(&db);
        return 0;
    }

    pthread_t *tids = (pthread_t *)malloc(sizeof(pthread_t) * threads);
    ThreadWork *works = (ThreadWork *)malloc(sizeof(ThreadWork) * threads);
    if (!tids || !works) {
        snprintf(err, err_cap, "out of memory");
        free(results);
        free(tids);
        free(works);
        kmer_index_free(&index);
        fasta_free(&db);
        return 0;
    }

    int chunk = (n + threads - 1) / threads;
    for (int t = 0; t < threads; t++) {
        int start = t * chunk;
        int end = start + chunk;
        if (end > n) end = n;
        works[t].db = &db;
        works[t].index = &index;
        works[t].df_max = df_max;
        works[t].M = eff_M;
        works[t].start = start;
        works[t].end = end;
        works[t].max_len = db.max_len;
        works[t].results = results;
        pthread_create(&tids[t], NULL, thread_worker, &works[t]);
    }
    for (int t = 0; t < threads; t++) {
        pthread_join(tids[t], NULL);
    }

    double t_query = now_seconds();

    if (directed_path && directed_path[0]) {
        if (!write_directed(directed_path, &db, results, n, err, err_cap)) {
            free(results);
            free(tids);
            free(works);
            kmer_index_free(&index);
            fasta_free(&db);
            return 0;
        }
    }

    if (!write_pairs(out_pairs_path, &db, results, n, err, err_cap)) {
        free(results);
        free(tids);
        free(works);
        kmer_index_free(&index);
        fasta_free(&db);
        return 0;
    }

    double t_done = now_seconds();

    if (stats) {
        stats->n_sequences = n;
        stats->total_kmers = index.n_entries;
        stats->k = k;
        stats->M = eff_M;
        stats->df_max = df_max;
        stats->threads_used = threads;
        stats->time_read_s = t_read - t0;
        stats->time_index_s = t_index - t_read;
        stats->time_query_s = t_query - t_index;
        stats->time_total_s = t_done - t0;
    }

    if (verbose >= 1) {
        fprintf(stderr, "read: %.3fs, index: %.3fs, query: %.3fs, total: %.3fs\n",
            t_read - t0, t_index - t_read, t_query - t_index, t_done - t0);
    }

    free(results);
    free(tids);
    free(works);
    kmer_index_free(&index);
    fasta_free(&db);
    return 1;
}
