#include "levenshtein.h"

#include "util.h"

static int min3(int a, int b, int c) {
    int m = a < b ? a : b;
    return m < c ? m : c;
}

int levenshtein_banded(const char *s, int n, const char *t, int m, int limit, int *prev, int *curr) {
    if (limit < 0) return limit + 1;
    if (n == 0) return m;
    if (m == 0) return n;

    if (n > m) {
        const char *tmp_s = s; s = t; t = tmp_s;
        int tmp_n = n; n = m; m = tmp_n;
    }

    if (iabs_int(n - m) > limit) return limit + 1;

    int prev_start = 0;
    int prev_end = limit < m ? limit : m;
    for (int j = prev_start; j <= prev_end; j++) {
        prev[j - prev_start] = j;
    }

    for (int i = 1; i <= n; i++) {
        int curr_start = i - limit;
        if (curr_start < 0) curr_start = 0;
        int curr_end = i + limit;
        if (curr_end > m) curr_end = m;

        int min_row = limit + 1;
        for (int j = curr_start; j <= curr_end; j++) {
            int del = (j >= prev_start && j <= prev_end) ? prev[j - prev_start] + 1 : limit + 1;
            int ins = (j > curr_start) ? curr[(j - 1) - curr_start] + 1 : limit + 1;
            int sub = (j - 1 >= prev_start && j - 1 <= prev_end) ? prev[(j - 1) - prev_start] + (s[i - 1] != t[j - 1]) : limit + 1;
            int v = min3(del, ins, sub);
            curr[j - curr_start] = v;
            if (v < min_row) min_row = v;
        }
        if (min_row > limit) return limit + 1;

        int *tmp = prev;
        prev = curr;
        curr = tmp;
        prev_start = curr_start;
        prev_end = curr_end;
    }

    if (m < prev_start || m > prev_end) return limit + 1;
    return prev[m - prev_start];
}
