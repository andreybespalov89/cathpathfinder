#include "fasta.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

static int is_allowed_aa(char c) {
    switch (c) {
        case 'A': case 'C': case 'D': case 'E': case 'F':
        case 'G': case 'H': case 'I': case 'K': case 'L':
        case 'M': case 'N': case 'P': case 'Q': case 'R':
        case 'S': case 'T': case 'V': case 'W': case 'Y':
        case 'X':
            return 1;
        default:
            return 0;
    }
}

static int ensure_seq_cap(FastaDB *db, int need) {
    if (db->cap >= need) return 1;
    int new_cap = db->cap ? db->cap * 2 : 128;
    if (new_cap < need) new_cap = need;
    FastaSeq *p = (FastaSeq *)realloc(db->seqs, sizeof(FastaSeq) * new_cap);
    if (!p) return 0;
    db->seqs = p;
    db->cap = new_cap;
    return 1;
}

static int ensure_pool_cap(FastaDB *db, size_t need) {
    if (db->pool_cap >= need) return 1;
    size_t new_cap = db->pool_cap ? db->pool_cap * 2 : 1024;
    if (new_cap < need) new_cap = need;
    char *p = (char *)realloc(db->pool, new_cap);
    if (!p) return 0;
    db->pool = p;
    db->pool_cap = new_cap;
    return 1;
}

static char *strdup_slice(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int finalize_seq(FastaDB *db, int idx, char *err, size_t err_cap) {
    if (idx < 0) return 1;
    FastaSeq *seq = &db->seqs[idx];
    if (seq->len <= 0) {
        snprintf(err, err_cap, "empty sequence for id '%s'", seq->id ? seq->id : "");
        return 0;
    }
    if (seq->len > db->max_len) db->max_len = seq->len;
    return 1;
}

int fasta_read(const char *path, int strict, FastaDB *db, char *err, size_t err_cap) {
    memset(db, 0, sizeof(*db));
    FILE *fp = fopen(path, "r");
    if (!fp) {
        snprintf(err, err_cap, "cannot open fasta: %s", path);
        return 0;
    }

    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len;
    int cur_idx = -1;

    while ((line_len = getline(&line, &line_cap, fp)) != -1) {
        if (line_len == 0) continue;
        if (line[0] == '>') {
            if (!finalize_seq(db, cur_idx, err, err_cap)) {
                fclose(fp);
                free(line);
                return 0;
            }
            if (!ensure_seq_cap(db, db->n + 1)) {
                snprintf(err, err_cap, "out of memory");
                fclose(fp);
                free(line);
                return 0;
            }
            char *header_start = line + 1;
            while (*header_start == ' ' || *header_start == '\t') header_start++;
            size_t header_len = strcspn(header_start, "\r\n");
            char *header = strdup_slice(header_start, header_len);
            if (!header) {
                snprintf(err, err_cap, "out of memory");
                fclose(fp);
                free(line);
                return 0;
            }
            size_t id_len = strcspn(header_start, " \t\r\n");
            if (id_len == 0) {
                snprintf(err, err_cap, "missing id in header");
                fclose(fp);
                free(header);
                free(line);
                return 0;
            }
            char *id = strdup_slice(header_start, id_len);
            if (!id) {
                snprintf(err, err_cap, "out of memory");
                fclose(fp);
                free(header);
                free(line);
                return 0;
            }
            FastaSeq *seq = &db->seqs[db->n];
            memset(seq, 0, sizeof(*seq));
            seq->id = id;
            seq->header = header;
            seq->offset = db->pool_len;
            seq->len = 0;
            cur_idx = db->n;
            db->n += 1;
            continue;
        }

        if (cur_idx < 0) {
            snprintf(err, err_cap, "sequence data before first header");
            fclose(fp);
            free(line);
            return 0;
        }

        for (ssize_t i = 0; i < line_len; i++) {
            char c = line[i];
            if (c == '\n' || c == '\r') continue;
            if (c == ' ' || c == '\t') continue;
            c = (char)toupper((unsigned char)c);
            if (c == '*' || c == '-') {
                if (strict) {
                    snprintf(err, err_cap, "invalid symbol '%c' in sequence '%s'", c, db->seqs[cur_idx].id);
                    fclose(fp);
                    free(line);
                    return 0;
                }
                continue;
            }
            if (!is_allowed_aa(c)) {
                if (strict) {
                    snprintf(err, err_cap, "invalid symbol '%c' in sequence '%s'", c, db->seqs[cur_idx].id);
                    fclose(fp);
                    free(line);
                    return 0;
                }
                c = 'X';
            }
            if (!ensure_pool_cap(db, db->pool_len + 1)) {
                snprintf(err, err_cap, "out of memory");
                fclose(fp);
                free(line);
                return 0;
            }
            db->pool[db->pool_len++] = c;
            db->seqs[cur_idx].len += 1;
        }
    }

    if (!finalize_seq(db, cur_idx, err, err_cap)) {
        fclose(fp);
        free(line);
        return 0;
    }

    fclose(fp);
    free(line);
    if (db->n == 0) {
        snprintf(err, err_cap, "no sequences found");
        return 0;
    }

    if (!ensure_pool_cap(db, db->pool_len + 1)) {
        snprintf(err, err_cap, "out of memory");
        return 0;
    }
    db->pool[db->pool_len] = '\0';
    return 1;
}

void fasta_free(FastaDB *db) {
    if (!db) return;
    for (int i = 0; i < db->n; i++) {
        free(db->seqs[i].id);
        free(db->seqs[i].header);
    }
    free(db->seqs);
    free(db->pool);
    memset(db, 0, sizeof(*db));
}
