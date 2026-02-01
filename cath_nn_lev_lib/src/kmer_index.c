#include "kmer_index.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int aa_map[256];
static int aa_map_init = 0;

static void init_aa_map(void) {
    if (aa_map_init) return;
    for (int i = 0; i < 256; i++) aa_map[i] = -1;
    const char *alphabet = "ACDEFGHIKLMNPQRSTVWY";
    for (int i = 0; alphabet[i]; i++) aa_map[(unsigned char)alphabet[i]] = i;
    aa_map[(unsigned char)'X'] = 20;
    aa_map_init = 1;
}

int kmer_code_for_window(const char *s, int k, uint32_t *code_out) {
    init_aa_map();
    uint32_t code = 0;
    for (int i = 0; i < k; i++) {
        int v = aa_map[(unsigned char)s[i]];
        if (v < 0) return 0;
        code = code * 21u + (uint32_t)v;
    }
    *code_out = code;
    return 1;
}

static int cmp_entries(const void *a, const void *b) {
    const KmerEntry *ea = (const KmerEntry *)a;
    const KmerEntry *eb = (const KmerEntry *)b;
    if (ea->code < eb->code) return -1;
    if (ea->code > eb->code) return 1;
    if (ea->seq_id < eb->seq_id) return -1;
    if (ea->seq_id > eb->seq_id) return 1;
    return 0;
}

int kmer_index_build(const FastaDB *db, int k, KmerIndex *index, char *err, size_t err_cap) {
    memset(index, 0, sizeof(*index));
    if (k <= 0) {
        snprintf(err, err_cap, "k must be positive");
        return 0;
    }
    size_t total = 0;
    for (int i = 0; i < db->n; i++) {
        if (db->seqs[i].len >= k) total += (size_t)(db->seqs[i].len - k + 1);
    }
    if (total == 0) {
        snprintf(err, err_cap, "no k-mers available with k=%d", k);
        return 0;
    }

    KmerEntry *entries = (KmerEntry *)malloc(sizeof(KmerEntry) * total);
    if (!entries) {
        snprintf(err, err_cap, "out of memory");
        return 0;
    }

    size_t pos = 0;
    for (int i = 0; i < db->n; i++) {
        const FastaSeq *seq = &db->seqs[i];
        if (seq->len < k) continue;
        const char *s = db->pool + seq->offset;
        for (int j = 0; j <= seq->len - k; j++) {
            uint32_t code = 0;
            if (!kmer_code_for_window(s + j, k, &code)) {
                free(entries);
                snprintf(err, err_cap, "invalid symbol in sequence '%s'", seq->id);
                return 0;
            }
            entries[pos].code = code;
            entries[pos].seq_id = (uint32_t)i;
            pos++;
        }
    }
    index->entries = entries;
    index->n_entries = pos;
    index->k = k;

    qsort(index->entries, index->n_entries, sizeof(KmerEntry), cmp_entries);

    size_t n_unique = 0;
    for (size_t i = 0; i < index->n_entries; i++) {
        if (i == 0 || index->entries[i].code != index->entries[i - 1].code) n_unique++;
    }

    uint32_t *codes = (uint32_t *)malloc(sizeof(uint32_t) * n_unique);
    uint32_t *start = (uint32_t *)malloc(sizeof(uint32_t) * n_unique);
    uint32_t *end = (uint32_t *)malloc(sizeof(uint32_t) * n_unique);
    uint32_t *df = (uint32_t *)malloc(sizeof(uint32_t) * n_unique);
    if (!codes || !start || !end || !df) {
        free(codes); free(start); free(end); free(df);
        free(entries);
        snprintf(err, err_cap, "out of memory");
        return 0;
    }

    size_t u = 0;
    size_t i = 0;
    while (i < index->n_entries) {
        size_t j = i + 1;
        while (j < index->n_entries && index->entries[j].code == index->entries[i].code) j++;
        codes[u] = index->entries[i].code;
        start[u] = (uint32_t)i;
        end[u] = (uint32_t)j;
        uint32_t uniq = 0;
        uint32_t last = (uint32_t)(-1);
        for (size_t t = i; t < j; t++) {
            uint32_t sid = index->entries[t].seq_id;
            if (sid != last) {
                uniq++;
                last = sid;
            }
        }
        df[u] = uniq;
        u++;
        i = j;
    }

    index->codes = codes;
    index->start = start;
    index->end = end;
    index->df = df;
    index->n_unique = n_unique;
    return 1;
}

void kmer_index_free(KmerIndex *index) {
    if (!index) return;
    free(index->entries);
    free(index->codes);
    free(index->start);
    free(index->end);
    free(index->df);
    memset(index, 0, sizeof(*index));
}

int kmer_index_find(const KmerIndex *index, uint32_t code, size_t *pos_out) {
    size_t lo = 0, hi = index->n_unique;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        uint32_t v = index->codes[mid];
        if (v == code) {
            *pos_out = mid;
            return 1;
        }
        if (v < code) lo = mid + 1; else hi = mid;
    }
    return 0;
}
