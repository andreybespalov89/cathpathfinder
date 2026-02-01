#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "pipeline.h"
#include "levenshtein.h"

#include <stdlib.h>
#include <string.h>

static PyObject *py_find_pairs(PyObject *self, PyObject *args, PyObject *kwargs) {
    const char *fasta_path = NULL;
    const char *out_pairs_path = NULL;
    int k = 4;
    int M = 500;
    PyObject *df_obj = Py_None;
    int threads = 0;
    int strict = 0;
    PyObject *directed_obj = Py_None;

    static char *kwlist[] = {
        "fasta_path",
        "out_pairs_path",
        "k",
        "M",
        "df_max",
        "threads",
        "strict",
        "write_directed_path",
        NULL
    };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss|iiOiiO", kwlist,
                                    &fasta_path, &out_pairs_path,
                                    &k, &M, &df_obj, &threads, &strict, &directed_obj)) {
        return NULL;
    }

    int df_max = -1;
    if (df_obj != Py_None) {
        df_max = (int)PyLong_AsLong(df_obj);
        if (PyErr_Occurred()) return NULL;
    }

    const char *directed_path = NULL;
    if (directed_obj != Py_None) {
        directed_path = PyUnicode_AsUTF8(directed_obj);
        if (!directed_path) return NULL;
    }

    PipelineStats stats;
    char err[512];
    int ok;

    Py_BEGIN_ALLOW_THREADS
    ok = pipeline_find_pairs(
        fasta_path,
        out_pairs_path,
        k,
        M,
        df_max,
        threads,
        strict,
        directed_path,
        0,
        &stats,
        err,
        sizeof(err)
    );
    Py_END_ALLOW_THREADS

    if (!ok) {
        if (strstr(err, "cannot open") || strstr(err, "cannot write")) {
            PyErr_SetString(PyExc_OSError, err);
        } else {
            PyErr_SetString(PyExc_ValueError, err);
        }
        return NULL;
    }

    PyObject *out = PyDict_New();
    if (!out) return NULL;
    PyDict_SetItemString(out, "n_sequences", PyLong_FromLong(stats.n_sequences));
    PyDict_SetItemString(out, "total_kmers", PyLong_FromUnsignedLongLong(stats.total_kmers));
    PyDict_SetItemString(out, "k", PyLong_FromLong(stats.k));
    PyDict_SetItemString(out, "M", PyLong_FromLong(stats.M));
    PyDict_SetItemString(out, "df_max", PyLong_FromLong(stats.df_max));
    PyDict_SetItemString(out, "threads_used", PyLong_FromLong(stats.threads_used));
    PyDict_SetItemString(out, "time_read_s", PyFloat_FromDouble(stats.time_read_s));
    PyDict_SetItemString(out, "time_index_s", PyFloat_FromDouble(stats.time_index_s));
    PyDict_SetItemString(out, "time_query_s", PyFloat_FromDouble(stats.time_query_s));
    PyDict_SetItemString(out, "time_total_s", PyFloat_FromDouble(stats.time_total_s));
    return out;
}

static PyObject *py_levenshtein_bounded(PyObject *self, PyObject *args) {
    const char *s = NULL;
    const char *t = NULL;
    int limit = 0;
    Py_ssize_t ns = 0;
    Py_ssize_t nt = 0;

    if (!PyArg_ParseTuple(args, "s#s#i", &s, &ns, &t, &nt, &limit)) {
        return NULL;
    }

    int max_len = (ns > nt) ? (int)ns : (int)nt;
    int *prev = (int *)malloc(sizeof(int) * (2 * max_len + 3));
    int *curr = (int *)malloc(sizeof(int) * (2 * max_len + 3));
    if (!prev || !curr) {
        free(prev); free(curr);
        PyErr_SetString(PyExc_MemoryError, "out of memory");
        return NULL;
    }

    int dist = levenshtein_banded(s, (int)ns, t, (int)nt, limit, prev, curr);
    free(prev);
    free(curr);
    return PyLong_FromLong(dist);
}

static PyMethodDef Methods[] = {
    {"find_pairs", (PyCFunction)py_find_pairs, METH_VARARGS | METH_KEYWORDS, "Find nearest neighbors and write TSV outputs."},
    {"_levenshtein_bounded", py_levenshtein_bounded, METH_VARARGS, "Internal: bounded Levenshtein distance."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "cath_nn_lev.cath_nn_lev",
    "C extension for nearest neighbor search with k-mer prefilter and Levenshtein distance",
    -1,
    Methods
};

PyMODINIT_FUNC PyInit_cath_nn_lev(void) {
    return PyModule_Create(&moduledef);
}
