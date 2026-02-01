#ifndef CATH_NN_LEV_LEVENSHTEIN_H
#define CATH_NN_LEV_LEVENSHTEIN_H

#ifdef __cplusplus
extern "C" {
#endif

int levenshtein_banded(const char *s, int n, const char *t, int m, int limit, int *prev, int *curr);

#ifdef __cplusplus
}
#endif

#endif
