#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>

typedef enum {
    OP_MATCH = 0,
    OP_REPLACE = 1,
    OP_INSERT = 2,
    OP_DELETE = 3
} OpType;

typedef struct {
    OpType type;
    Py_UCS4 a;
    Py_UCS4 b;
} Op;

static int
unicode_to_ucs4_array(PyObject *u, Py_UCS4 **out, Py_ssize_t *out_len) {
    Py_ssize_t len = PyUnicode_GetLength(u);
    if (len < 0) {
        return -1;
    }
    Py_UCS4 *buf = (Py_UCS4 *)PyMem_Malloc(sizeof(Py_UCS4) * (size_t)len);
    if (!buf) {
        PyErr_NoMemory();
        return -1;
    }
    for (Py_ssize_t i = 0; i < len; i++) {
        buf[i] = PyUnicode_ReadChar(u, i);
    }
    *out = buf;
    *out_len = len;
    return 0;
}

static PyObject *
ucs4_array_to_unicode(Py_UCS4 *buf, Py_ssize_t len) {
    return PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, buf, len);
}

static PyObject *
op_to_tuple(const Op *op) {
    PyObject *t = NULL;
    switch (op->type) {
        case OP_MATCH: {
            PyObject *name = PyUnicode_FromString("match");
            PyObject *ch = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &op->a, 1);
            if (!name || !ch) {
                Py_XDECREF(name);
                Py_XDECREF(ch);
                return NULL;
            }
            t = PyTuple_Pack(2, name, ch);
            Py_DECREF(name);
            Py_DECREF(ch);
            break;
        }
        case OP_REPLACE: {
            PyObject *name = PyUnicode_FromString("replace");
            PyObject *oldc = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &op->a, 1);
            PyObject *newc = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &op->b, 1);
            if (!name || !oldc || !newc) {
                Py_XDECREF(name);
                Py_XDECREF(oldc);
                Py_XDECREF(newc);
                return NULL;
            }
            t = PyTuple_Pack(3, name, oldc, newc);
            Py_DECREF(name);
            Py_DECREF(oldc);
            Py_DECREF(newc);
            break;
        }
        case OP_INSERT: {
            PyObject *name = PyUnicode_FromString("insert");
            PyObject *ch = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &op->b, 1);
            if (!name || !ch) {
                Py_XDECREF(name);
                Py_XDECREF(ch);
                return NULL;
            }
            t = PyTuple_Pack(2, name, ch);
            Py_DECREF(name);
            Py_DECREF(ch);
            break;
        }
        case OP_DELETE: {
            PyObject *name = PyUnicode_FromString("delete");
            PyObject *ch = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &op->a, 1);
            if (!name || !ch) {
                Py_XDECREF(name);
                Py_XDECREF(ch);
                return NULL;
            }
            t = PyTuple_Pack(2, name, ch);
            Py_DECREF(name);
            Py_DECREF(ch);
            break;
        }
        default:
            PyErr_SetString(PyExc_RuntimeError, "Unknown op type");
            return NULL;
    }
    return t;
}

static PyObject *
build_operations_list(const Op *ops, Py_ssize_t count) {
    PyObject *list = PyList_New(count);
    if (!list) {
        return NULL;
    }
    for (Py_ssize_t i = 0; i < count; i++) {
        PyObject *t = op_to_tuple(&ops[i]);
        if (!t) {
            Py_DECREF(list);
            return NULL;
        }
        PyList_SET_ITEM(list, i, t);
    }
    return list;
}

static PyObject *
build_steps_from_ops(const Py_UCS4 *a, Py_ssize_t a_len, const Op *ops, Py_ssize_t ops_len) {
    Py_UCS4 *current = (Py_UCS4 *)PyMem_Malloc(sizeof(Py_UCS4) * (size_t)(a_len + ops_len + 1));
    if (!current) {
        PyErr_NoMemory();
        return NULL;
    }
    memcpy(current, a, sizeof(Py_UCS4) * (size_t)a_len);
    Py_ssize_t cur_len = a_len;

    PyObject *steps = PyList_New(0);
    if (!steps) {
        PyMem_Free(current);
        return NULL;
    }
    PyObject *initial = ucs4_array_to_unicode(current, cur_len);
    if (!initial) {
        Py_DECREF(steps);
        PyMem_Free(current);
        return NULL;
    }
    if (PyList_Append(steps, initial) < 0) {
        Py_DECREF(initial);
        Py_DECREF(steps);
        PyMem_Free(current);
        return NULL;
    }
    Py_DECREF(initial);

    Py_ssize_t i = 0;
    for (Py_ssize_t k = 0; k < ops_len; k++) {
        Op op = ops[k];
        if (op.type == OP_MATCH) {
            i += 1;
        } else if (op.type == OP_REPLACE) {
            if (i < 0 || i >= cur_len) {
                PyErr_SetString(PyExc_RuntimeError, "Replace index out of range");
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            current[i] = op.b;
            PyObject *s = ucs4_array_to_unicode(current, cur_len);
            if (!s) {
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            if (PyList_Append(steps, s) < 0) {
                Py_DECREF(s);
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            Py_DECREF(s);
            i += 1;
        } else if (op.type == OP_INSERT) {
            if (i < 0 || i > cur_len) {
                PyErr_SetString(PyExc_RuntimeError, "Insert index out of range");
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            memmove(current + i + 1, current + i, sizeof(Py_UCS4) * (size_t)(cur_len - i));
            current[i] = op.b;
            cur_len += 1;
            PyObject *s = ucs4_array_to_unicode(current, cur_len);
            if (!s) {
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            if (PyList_Append(steps, s) < 0) {
                Py_DECREF(s);
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            Py_DECREF(s);
            i += 1;
        } else if (op.type == OP_DELETE) {
            if (i < 0 || i >= cur_len) {
                PyErr_SetString(PyExc_RuntimeError, "Delete index out of range");
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            memmove(current + i, current + i + 1, sizeof(Py_UCS4) * (size_t)(cur_len - i - 1));
            cur_len -= 1;
            PyObject *s = ucs4_array_to_unicode(current, cur_len);
            if (!s) {
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            if (PyList_Append(steps, s) < 0) {
                Py_DECREF(s);
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            Py_DECREF(s);
        }
    }

    PyMem_Free(current);
    return steps;
}

static PyObject *
build_operations_from_steps(PyObject *steps) {
    Py_ssize_t count = PyList_Size(steps);
    PyObject *ops = PyList_New(0);
    if (!ops) {
        return NULL;
    }
    for (Py_ssize_t i = 0; i + 1 < count; i++) {
        PyObject *prev = PyList_GetItem(steps, i);
        PyObject *curr = PyList_GetItem(steps, i + 1);
        if (!PyUnicode_Check(prev) || !PyUnicode_Check(curr)) {
            PyErr_SetString(PyExc_TypeError, "steps must be list of strings");
            Py_DECREF(ops);
            return NULL;
        }
        Py_ssize_t prev_len = PyUnicode_GetLength(prev);
        Py_ssize_t curr_len = PyUnicode_GetLength(curr);
        PyObject *tuple = NULL;
        if (curr_len == prev_len) {
            int is_reverse = 1;
            for (Py_ssize_t j = 0; j < curr_len; j++) {
                if (PyUnicode_ReadChar(curr, j) != PyUnicode_ReadChar(prev, curr_len - 1 - j)) {
                    is_reverse = 0;
                    break;
                }
            }
            if (is_reverse) {
                PyObject *name = PyUnicode_FromString("reverse");
                if (!name) {
                    Py_DECREF(ops);
                    return NULL;
                }
                tuple = PyTuple_Pack(1, name);
                Py_DECREF(name);
            } else {
                Py_ssize_t diff1 = -1;
                Py_ssize_t diff2 = -1;
                int diff_count = 0;
                for (Py_ssize_t j = 0; j < curr_len; j++) {
                    if (PyUnicode_ReadChar(curr, j) != PyUnicode_ReadChar(prev, j)) {
                        diff_count += 1;
                        if (diff1 < 0) diff1 = j;
                        else if (diff2 < 0) diff2 = j;
                    }
                }
                if (diff_count == 2 && diff1 >= 0 && diff2 >= 0) {
                    Py_UCS4 a1 = PyUnicode_ReadChar(prev, diff1);
                    Py_UCS4 a2 = PyUnicode_ReadChar(prev, diff2);
                    if (PyUnicode_ReadChar(curr, diff1) == a2 && PyUnicode_ReadChar(curr, diff2) == a1) {
                        PyObject *name = PyUnicode_FromString("swap");
                        PyObject *iobj = PyLong_FromSsize_t(diff1);
                        PyObject *jobj = PyLong_FromSsize_t(diff2);
                        if (!name || !iobj || !jobj) {
                            Py_XDECREF(name);
                            Py_XDECREF(iobj);
                            Py_XDECREF(jobj);
                            Py_DECREF(ops);
                            return NULL;
                        }
                        tuple = PyTuple_Pack(3, name, iobj, jobj);
                        Py_DECREF(name);
                        Py_DECREF(iobj);
                        Py_DECREF(jobj);
                    }
                }
            }
        }
        if (!tuple && curr_len > prev_len) {
            Py_ssize_t j;
            for (j = 0; j < curr_len; j++) {
                if (j >= prev_len || PyUnicode_ReadChar(curr, j) != PyUnicode_ReadChar(prev, j)) {
                    Py_UCS4 ch = PyUnicode_ReadChar(curr, j);
                    PyObject *name = PyUnicode_FromString("insert");
                    PyObject *chobj = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &ch, 1);
                    if (!name || !chobj) {
                        Py_XDECREF(name);
                        Py_XDECREF(chobj);
                        Py_DECREF(ops);
                        return NULL;
                    }
                    tuple = PyTuple_Pack(2, name, chobj);
                    Py_DECREF(name);
                    Py_DECREF(chobj);
                    break;
                }
            }
        } else if (!tuple && curr_len < prev_len) {
            Py_ssize_t j;
            for (j = 0; j < prev_len; j++) {
                if (j >= curr_len || PyUnicode_ReadChar(curr, j) != PyUnicode_ReadChar(prev, j)) {
                    Py_UCS4 ch = PyUnicode_ReadChar(prev, j);
                    PyObject *name = PyUnicode_FromString("delete");
                    PyObject *chobj = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &ch, 1);
                    if (!name || !chobj) {
                        Py_XDECREF(name);
                        Py_XDECREF(chobj);
                        Py_DECREF(ops);
                        return NULL;
                    }
                    tuple = PyTuple_Pack(2, name, chobj);
                    Py_DECREF(name);
                    Py_DECREF(chobj);
                    break;
                }
            }
        } else if (!tuple) {
            int diff_found = 0;
            for (Py_ssize_t j = 0; j < curr_len; j++) {
                Py_UCS4 c1 = PyUnicode_ReadChar(curr, j);
                Py_UCS4 c2 = PyUnicode_ReadChar(prev, j);
                if (c1 != c2) {
                    PyObject *name = PyUnicode_FromString("replace");
                    PyObject *oldc = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &c2, 1);
                    PyObject *newc = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &c1, 1);
                    if (!name || !oldc || !newc) {
                        Py_XDECREF(name);
                        Py_XDECREF(oldc);
                        Py_XDECREF(newc);
                        Py_DECREF(ops);
                        return NULL;
                    }
                    tuple = PyTuple_Pack(3, name, oldc, newc);
                    Py_DECREF(name);
                    Py_DECREF(oldc);
                    Py_DECREF(newc);
                    diff_found = 1;
                    break;
                }
            }
            if (!diff_found) {
                PyObject *name = PyUnicode_FromString("match");
                PyObject *chobj = NULL;
                if (curr_len > 0) {
                    Py_UCS4 ch = PyUnicode_ReadChar(curr, 0);
                    chobj = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, &ch, 1);
                } else {
                    chobj = PyUnicode_New(0, 0);
                }
                if (!name || !chobj) {
                    Py_XDECREF(name);
                    Py_XDECREF(chobj);
                    Py_DECREF(ops);
                    return NULL;
                }
                tuple = PyTuple_Pack(2, name, chobj);
                Py_DECREF(name);
                Py_DECREF(chobj);
            }
        }

        if (!tuple) {
            PyErr_SetString(PyExc_RuntimeError, "Failed to build operation tuple");
            Py_DECREF(ops);
            return NULL;
        }
        if (PyList_Append(ops, tuple) < 0) {
            Py_DECREF(tuple);
            Py_DECREF(ops);
            return NULL;
        }
        Py_DECREF(tuple);
    }
    return ops;
}

static int
compute_dp(const Py_UCS4 *a, Py_ssize_t m, const Py_UCS4 *b, Py_ssize_t n, int **out_dp) {
    size_t size = (size_t)(m + 1) * (size_t)(n + 1);
    int *dp = (int *)PyMem_Malloc(sizeof(int) * size);
    if (!dp) {
        PyErr_NoMemory();
        return -1;
    }
    for (Py_ssize_t i = 0; i <= m; i++) {
        dp[i * (n + 1)] = (int)i;
    }
    for (Py_ssize_t j = 0; j <= n; j++) {
        dp[j] = (int)j;
    }
    for (Py_ssize_t i = 1; i <= m; i++) {
        for (Py_ssize_t j = 1; j <= n; j++) {
            if (a[i - 1] == b[j - 1]) {
                dp[i * (n + 1) + j] = dp[(i - 1) * (n + 1) + (j - 1)];
            } else {
                int del = dp[(i - 1) * (n + 1) + j] + 1;
                int ins = dp[i * (n + 1) + (j - 1)] + 1;
                int rep = dp[(i - 1) * (n + 1) + (j - 1)] + 1;
                int min = del;
                if (ins < min) min = ins;
                if (rep < min) min = rep;
                dp[i * (n + 1) + j] = min;
            }
        }
    }
    *out_dp = dp;
    return 0;
}

static PyObject *
reverse_ops_from_dp(const Py_UCS4 *a, Py_ssize_t m, const Py_UCS4 *b, Py_ssize_t n, const int *dp, Py_ssize_t *out_len) {
    Py_ssize_t cap = (m + n + 4);
    Op *ops = (Op *)PyMem_Malloc(sizeof(Op) * (size_t)cap);
    if (!ops) {
        PyErr_NoMemory();
        return NULL;
    }
    Py_ssize_t count = 0;
    Py_ssize_t i = m;
    Py_ssize_t j = n;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && a[i - 1] == b[j - 1]) {
            ops[count++] = (Op){OP_MATCH, a[i - 1], 0};
            i -= 1;
            j -= 1;
        } else {
            if (i > 0 && j > 0 && dp[i * (n + 1) + j] == dp[(i - 1) * (n + 1) + (j - 1)] + 1) {
                ops[count++] = (Op){OP_REPLACE, a[i - 1], b[j - 1]};
                i -= 1;
                j -= 1;
            } else if (j > 0 && dp[i * (n + 1) + j] == dp[i * (n + 1) + (j - 1)] + 1) {
                ops[count++] = (Op){OP_INSERT, 0, b[j - 1]};
                j -= 1;
            } else if (i > 0 && dp[i * (n + 1) + j] == dp[(i - 1) * (n + 1) + j] + 1) {
                ops[count++] = (Op){OP_DELETE, a[i - 1], 0};
                i -= 1;
            } else {
                PyMem_Free(ops);
                PyErr_SetString(PyExc_RuntimeError, "Failed to reconstruct operations");
                return NULL;
            }
        }
    }
    for (Py_ssize_t k = 0; k < count / 2; k++) {
        Op tmp = ops[k];
        ops[k] = ops[count - 1 - k];
        ops[count - 1 - k] = tmp;
    }
    PyObject *capsule = PyCapsule_New(ops, "ops", NULL);
    if (!capsule) {
        PyMem_Free(ops);
        return NULL;
    }
    *out_len = count;
    return capsule;
}

static void
free_ops_capsule(PyObject *capsule) {
    Op *ops = (Op *)PyCapsule_GetPointer(capsule, "ops");
    if (ops) {
        PyMem_Free(ops);
    }
}

static PyObject *
algo_seq_dynamic_run(PyObject *self, PyObject *args) {
    PyObject *a_obj;
    PyObject *b_obj;
    if (!PyArg_ParseTuple(args, "UU", &a_obj, &b_obj)) {
        return NULL;
    }

    Py_UCS4 *a = NULL;
    Py_UCS4 *b = NULL;
    Py_ssize_t m = 0;
    Py_ssize_t n = 0;
    if (unicode_to_ucs4_array(a_obj, &a, &m) < 0) {
        return NULL;
    }
    if (unicode_to_ucs4_array(b_obj, &b, &n) < 0) {
        PyMem_Free(a);
        return NULL;
    }

    int *dp = NULL;
    if (compute_dp(a, m, b, n, &dp) < 0) {
        PyMem_Free(a);
        PyMem_Free(b);
        return NULL;
    }

    Py_ssize_t ops_len = 0;
    PyObject *ops_capsule = reverse_ops_from_dp(a, m, b, n, dp, &ops_len);
    if (!ops_capsule) {
        PyMem_Free(a);
        PyMem_Free(b);
        PyMem_Free(dp);
        return NULL;
    }

    Op *ops = (Op *)PyCapsule_GetPointer(ops_capsule, "ops");
    if (!ops) {
        Py_DECREF(ops_capsule);
        PyMem_Free(a);
        PyMem_Free(b);
        PyMem_Free(dp);
        return NULL;
    }

    PyObject *steps = build_steps_from_ops(a, m, ops, ops_len);
    if (!steps) {
        Py_DECREF(ops_capsule);
        PyMem_Free(a);
        PyMem_Free(b);
        PyMem_Free(dp);
        return NULL;
    }
    PyObject *ops_list = build_operations_list(ops, ops_len);
    if (!ops_list) {
        Py_DECREF(steps);
        Py_DECREF(ops_capsule);
        PyMem_Free(a);
        PyMem_Free(b);
        PyMem_Free(dp);
        return NULL;
    }

    int min_dist = dp[m * (n + 1) + n];

    PyObject *dist_obj = PyLong_FromLong(min_dist);
    if (!dist_obj) {
        Py_DECREF(steps);
        Py_DECREF(ops_list);
        free_ops_capsule(ops_capsule);
        Py_DECREF(ops_capsule);
        PyMem_Free(a);
        PyMem_Free(b);
        PyMem_Free(dp);
        return NULL;
    }
    PyObject *result = PyTuple_Pack(3, steps, ops_list, dist_obj);
    Py_DECREF(dist_obj);
    Py_DECREF(steps);
    Py_DECREF(ops_list);
    free_ops_capsule(ops_capsule);
    Py_DECREF(ops_capsule);
    PyMem_Free(a);
    PyMem_Free(b);
    PyMem_Free(dp);
    return result;
}

typedef struct {
    const Py_UCS4 *a;
    const Py_UCS4 *b;
    Py_ssize_t m;
    Py_ssize_t n;
    PyObject *validator;
    PyObject *steps_found;
    Op *stack;
    Py_ssize_t max_ops;
    int verbose;
    Py_ssize_t nodes_visited;
    Py_ssize_t validations;
    Py_ssize_t validation_failures;
    Py_ssize_t steps_checked;
    Py_ssize_t log_every;
    Py_ssize_t log_fail_limit;
    Py_ssize_t max_nodes;
    Py_ssize_t beam_size;
    int max_unvalidated_steps;
    double time_limit_s;
    double start_time_s;
} SearchCtx;

typedef struct SeqBuf {
    Py_UCS4 *data;
    Py_ssize_t len;
    int refcnt;
} SeqBuf;

static SeqBuf *seqbuf_new(const Py_UCS4 *src, Py_ssize_t len) {
    SeqBuf *sb = (SeqBuf *)PyMem_Malloc(sizeof(SeqBuf));
    if (!sb) return NULL;
    sb->data = (Py_UCS4 *)PyMem_Malloc(sizeof(Py_UCS4) * (size_t)len);
    if (!sb->data) {
        PyMem_Free(sb);
        return NULL;
    }
    memcpy(sb->data, src, sizeof(Py_UCS4) * (size_t)len);
    sb->len = len;
    sb->refcnt = 1;
    return sb;
}

static void seqbuf_inc(SeqBuf *sb) {
    if (sb) sb->refcnt += 1;
}

static void seqbuf_dec(SeqBuf *sb) {
    if (!sb) return;
    sb->refcnt -= 1;
    if (sb->refcnt <= 0) {
        PyMem_Free(sb->data);
        PyMem_Free(sb);
    }
}

static SeqBuf *seqbuf_apply(const SeqBuf *src, OpType type, Py_UCS4 b, Py_ssize_t pos, Py_ssize_t *out_pos) {
    if (type == OP_MATCH) {
        *out_pos = pos + 1;
        return (SeqBuf *)src;
    }
    if (type == OP_REPLACE) {
        if (pos < 0 || pos >= src->len) return NULL;
        SeqBuf *sb = seqbuf_new(src->data, src->len);
        if (!sb) return NULL;
        sb->data[pos] = b;
        *out_pos = pos + 1;
        return sb;
    }
    if (type == OP_INSERT) {
        if (pos < 0 || pos > src->len) return NULL;
        SeqBuf *sb = (SeqBuf *)PyMem_Malloc(sizeof(SeqBuf));
        if (!sb) return NULL;
        sb->data = (Py_UCS4 *)PyMem_Malloc(sizeof(Py_UCS4) * (size_t)(src->len + 1));
        if (!sb->data) {
            PyMem_Free(sb);
            return NULL;
        }
        memcpy(sb->data, src->data, sizeof(Py_UCS4) * (size_t)pos);
        sb->data[pos] = b;
        memcpy(sb->data + pos + 1, src->data + pos, sizeof(Py_UCS4) * (size_t)(src->len - pos));
        sb->len = src->len + 1;
        sb->refcnt = 1;
        *out_pos = pos + 1;
        return sb;
    }
    if (type == OP_DELETE) {
        if (pos < 0 || pos >= src->len) return NULL;
        SeqBuf *sb = (SeqBuf *)PyMem_Malloc(sizeof(SeqBuf));
        if (!sb) return NULL;
        sb->data = (Py_UCS4 *)PyMem_Malloc(sizeof(Py_UCS4) * (size_t)(src->len - 1));
        if (!sb->data) {
            PyMem_Free(sb);
            return NULL;
        }
        memcpy(sb->data, src->data, sizeof(Py_UCS4) * (size_t)pos);
        memcpy(sb->data + pos, src->data + pos + 1, sizeof(Py_UCS4) * (size_t)(src->len - pos - 1));
        sb->len = src->len - 1;
        sb->refcnt = 1;
        *out_pos = pos;
        return sb;
    }
    return NULL;
}

typedef struct {
    Py_ssize_t i;
    Py_ssize_t j;
    Py_ssize_t pos;
    int g;
    int f;
    int parent;
    Op op;
    int steps_since_validation;
} BFNode;

typedef struct {
    int *heap;
    size_t size;
    size_t cap;
    BFNode *nodes;
    size_t nodes_size;
    size_t nodes_cap;
    size_t beam_limit;
} BFQueue;

static int bfq_cmp(const BFNode *a, const BFNode *b) {
    if (a->f != b->f) return a->f < b->f;
    return a->g < b->g;
}

static int bfq_init(BFQueue *q, size_t cap) {
    q->heap = (int *)PyMem_Malloc(sizeof(int) * cap);
    if (!q->heap) return 0;
    q->size = 0;
    q->cap = cap;
    q->nodes = (BFNode *)PyMem_Malloc(sizeof(BFNode) * cap);
    if (!q->nodes) {
        PyMem_Free(q->heap);
        return 0;
    }
    q->nodes_size = 0;
    q->nodes_cap = cap;
    q->beam_limit = 0;
    return 1;
}

static void bfq_free(BFQueue *q) {
    if (!q) return;
    PyMem_Free(q->heap);
    PyMem_Free(q->nodes);
    q->heap = NULL;
    q->nodes = NULL;
    q->size = 0;
    q->cap = 0;
    q->nodes_size = 0;
    q->nodes_cap = 0;
}

static int bfq_push_node(BFQueue *q, BFNode node) {
    if (q->beam_limit > 0 && q->size >= q->beam_limit) {
        return 1;
    }
    if (q->nodes_size >= q->nodes_cap) {
        size_t new_cap = q->nodes_cap ? q->nodes_cap * 2 : 1024;
        BFNode *nn = (BFNode *)PyMem_Realloc(q->nodes, sizeof(BFNode) * new_cap);
        if (!nn) return 0;
        q->nodes = nn;
        q->nodes_cap = new_cap;
    }
    int idx = (int)q->nodes_size;
    q->nodes[q->nodes_size++] = node;
    if (q->size >= q->cap) {
        size_t new_cap = q->cap ? q->cap * 2 : 1024;
        int *nh = (int *)PyMem_Realloc(q->heap, sizeof(int) * new_cap);
        if (!nh) return 0;
        q->heap = nh;
        q->cap = new_cap;
    }
    size_t i = q->size++;
    q->heap[i] = idx;
    while (i > 0) {
        size_t p = (i - 1) / 2;
        BFNode *ni = &q->nodes[q->heap[i]];
        BFNode *np = &q->nodes[q->heap[p]];
        if (bfq_cmp(np, ni)) break;
        int tmp = q->heap[i];
        q->heap[i] = q->heap[p];
        q->heap[p] = tmp;
        i = p;
    }
    return 1;
}

static int bfq_pop(BFQueue *q, int *out_idx, BFNode *out) {
    if (q->size == 0) return 0;
    int top_idx = q->heap[0];
    *out = q->nodes[top_idx];
    if (out_idx) *out_idx = top_idx;
    q->size--;
    if (q->size > 0) {
        q->heap[0] = q->heap[q->size];
        size_t i = 0;
        while (1) {
            size_t l = i * 2 + 1;
            size_t r = l + 1;
            size_t s = i;
            if (l < q->size) {
                BFNode *nl = &q->nodes[q->heap[l]];
                BFNode *ns = &q->nodes[q->heap[s]];
                if (!bfq_cmp(ns, nl)) s = l;
            }
            if (r < q->size) {
                BFNode *nr = &q->nodes[q->heap[r]];
                BFNode *ns = &q->nodes[q->heap[s]];
                if (!bfq_cmp(ns, nr)) s = r;
            }
            if (s == i) break;
            int tmp = q->heap[i];
            q->heap[i] = q->heap[s];
            q->heap[s] = tmp;
            i = s;
        }
    }
    return 1;
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static int time_exceeded(const SearchCtx *ctx) {
    if (ctx->time_limit_s <= 0.0) return 0;
    return (now_seconds() - ctx->start_time_s) >= ctx->time_limit_s;
}

static PyObject *
try_steps_with_validator(const SearchCtx *ctx, const Op *ops, Py_ssize_t ops_len) {
    Py_UCS4 *current = (Py_UCS4 *)PyMem_Malloc(sizeof(Py_UCS4) * (size_t)(ctx->m + ops_len + 1));
    if (!current) {
        PyErr_NoMemory();
        return NULL;
    }
    memcpy(current, ctx->a, sizeof(Py_UCS4) * (size_t)ctx->m);
    Py_ssize_t cur_len = ctx->m;

    PyObject *steps = PyList_New(0);
    if (!steps) {
        PyMem_Free(current);
        return NULL;
    }
    PyObject *initial = ucs4_array_to_unicode(current, cur_len);
    if (!initial) {
        Py_DECREF(steps);
        PyMem_Free(current);
        return NULL;
    }
    if (PyList_Append(steps, initial) < 0) {
        Py_DECREF(initial);
        Py_DECREF(steps);
        PyMem_Free(current);
        return NULL;
    }
    Py_DECREF(initial);

    Py_ssize_t i = 0;
    for (Py_ssize_t k = 0; k < ops_len; k++) {
        Op op = ops[k];
        if (op.type == OP_MATCH) {
            i += 1;
        } else if (op.type == OP_REPLACE) {
            if (i < 0 || i >= cur_len) {
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            current[i] = op.b;
            PyObject *chain = ucs4_array_to_unicode(current, cur_len);
            if (!chain) {
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            if (ctx->validator) {
                if (ctx->verbose) {
                    ((SearchCtx *)ctx)->validations += 1;
                }
                PyObject *valid = PyObject_CallFunctionObjArgs(ctx->validator, chain, NULL);
                if (!valid) {
                    Py_DECREF(chain);
                    Py_DECREF(steps);
                    PyMem_Free(current);
                    return NULL;
                }
                int ok = PyObject_IsTrue(valid);
                Py_DECREF(valid);
                if (!ok) {
                    if (ctx->verbose) {
                        ((SearchCtx *)ctx)->validation_failures += 1;
                        if (((SearchCtx *)ctx)->validation_failures <= ctx->log_fail_limit) {
                            fprintf(stdout, "[dsc] validator reject op=REPLACE k=%zd\n", k);
                            fflush(stdout);
                        }
                    }
                    Py_DECREF(chain);
                    Py_DECREF(steps);
                    PyMem_Free(current);
                    return NULL;
                }
            }
            if (PyList_Append(steps, chain) < 0) {
                Py_DECREF(chain);
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            Py_DECREF(chain);
            i += 1;
        } else if (op.type == OP_INSERT) {
            if (i < 0 || i > cur_len) {
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            memmove(current + i + 1, current + i, sizeof(Py_UCS4) * (size_t)(cur_len - i));
            current[i] = op.b;
            cur_len += 1;
            PyObject *chain = ucs4_array_to_unicode(current, cur_len);
            if (!chain) {
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            if (ctx->validator) {
                if (ctx->verbose) {
                    ((SearchCtx *)ctx)->validations += 1;
                }
                PyObject *valid = PyObject_CallFunctionObjArgs(ctx->validator, chain, NULL);
                if (!valid) {
                    Py_DECREF(chain);
                    Py_DECREF(steps);
                    PyMem_Free(current);
                    return NULL;
                }
                int ok = PyObject_IsTrue(valid);
                Py_DECREF(valid);
                if (!ok) {
                    if (ctx->verbose) {
                        ((SearchCtx *)ctx)->validation_failures += 1;
                        if (((SearchCtx *)ctx)->validation_failures <= ctx->log_fail_limit) {
                            fprintf(stdout, "[dsc] validator reject op=INSERT k=%zd\n", k);
                            fflush(stdout);
                        }
                    }
                    Py_DECREF(chain);
                    Py_DECREF(steps);
                    PyMem_Free(current);
                    return NULL;
                }
            }
            if (PyList_Append(steps, chain) < 0) {
                Py_DECREF(chain);
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            Py_DECREF(chain);
            i += 1;
        } else if (op.type == OP_DELETE) {
            if (i < 0 || i >= cur_len) {
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            memmove(current + i, current + i + 1, sizeof(Py_UCS4) * (size_t)(cur_len - i - 1));
            cur_len -= 1;
            PyObject *chain = ucs4_array_to_unicode(current, cur_len);
            if (!chain) {
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            if (ctx->validator) {
                if (ctx->verbose) {
                    ((SearchCtx *)ctx)->validations += 1;
                }
                PyObject *valid = PyObject_CallFunctionObjArgs(ctx->validator, chain, NULL);
                if (!valid) {
                    Py_DECREF(chain);
                    Py_DECREF(steps);
                    PyMem_Free(current);
                    return NULL;
                }
                int ok = PyObject_IsTrue(valid);
                Py_DECREF(valid);
                if (!ok) {
                    if (ctx->verbose) {
                        ((SearchCtx *)ctx)->validation_failures += 1;
                        if (((SearchCtx *)ctx)->validation_failures <= ctx->log_fail_limit) {
                            fprintf(stdout, "[dsc] validator reject op=DELETE k=%zd\n", k);
                            fflush(stdout);
                        }
                    }
                    Py_DECREF(chain);
                    Py_DECREF(steps);
                    PyMem_Free(current);
                    return NULL;
                }
            }
            if (PyList_Append(steps, chain) < 0) {
                Py_DECREF(chain);
                Py_DECREF(steps);
                PyMem_Free(current);
                return NULL;
            }
            Py_DECREF(chain);
        }
    }

    PyMem_Free(current);
    if (ctx->verbose) {
        ((SearchCtx *)ctx)->steps_checked += 1;
        if (ctx->log_every > 0 && ctx->steps_checked % ctx->log_every == 0) {
            fprintf(stdout, "[dsc] checked paths=%zd validations=%zd failures=%zd\n",
                    ctx->steps_checked, ctx->validations, ctx->validation_failures);
            fflush(stdout);
        }
    }
    return steps;
}

static int
validate_ops_prefix(SearchCtx *ctx, const Op *ops, Py_ssize_t ops_len,
                    PyObject **out_steps, SeqBuf **out_seq,
                    Py_ssize_t *out_i, Py_ssize_t *out_j, Py_ssize_t *out_pos, int *out_g, int *out_invalid_streak) {
    SeqBuf *cur = seqbuf_new(ctx->a, ctx->m);
    if (!cur) {
        PyErr_NoMemory();
        return -1;
    }
    PyObject *steps = PyList_New(0);
    if (!steps) {
        seqbuf_dec(cur);
        return -1;
    }
    PyObject *initial = ucs4_array_to_unicode(cur->data, cur->len);
    if (!initial) {
        Py_DECREF(steps);
        seqbuf_dec(cur);
        return -1;
    }
    if (PyList_Append(steps, initial) < 0) {
        Py_DECREF(initial);
        Py_DECREF(steps);
        seqbuf_dec(cur);
        return -1;
    }
    Py_DECREF(initial);

    Py_ssize_t i = 0;
    Py_ssize_t j = 0;
    Py_ssize_t pos = 0;
    int g = 0;
    int invalid_streak = 0;

    for (Py_ssize_t k = 0; k < ops_len; k++) {
        Op op = ops[k];
        if (op.type == OP_MATCH) {
            i += 1;
            j += 1;
            pos += 1;
            continue;
        }

        Py_ssize_t new_pos = pos;
        SeqBuf *next = seqbuf_apply(cur, op.type, op.b, pos, &new_pos);
        if (!next) {
            Py_DECREF(steps);
            seqbuf_dec(cur);
            return -1;
        }

        if (ctx->validator) {
            ctx->validations += 1;
            PyObject *chain = ucs4_array_to_unicode(next->data, next->len);
            if (!chain) {
                seqbuf_dec(next);
                Py_DECREF(steps);
                seqbuf_dec(cur);
                return -1;
            }
            PyObject *valid = PyObject_CallFunctionObjArgs(ctx->validator, chain, NULL);
            Py_DECREF(chain);
            if (!valid) {
                seqbuf_dec(next);
                Py_DECREF(steps);
                seqbuf_dec(cur);
                return -1;
            }
            int ok = PyObject_IsTrue(valid);
            Py_DECREF(valid);
            if (!ok) {
                ctx->validation_failures += 1;
                invalid_streak += 1;
                if (invalid_streak > ctx->max_unvalidated_steps) {
                    seqbuf_dec(next);
                    *out_steps = steps;
                    *out_seq = cur;
                    *out_i = i;
                    *out_j = j;
                    *out_pos = pos;
                    *out_g = g;
                    *out_invalid_streak = invalid_streak > 0 ? invalid_streak - 1 : 0;
                    return 0;
                }
            } else {
                invalid_streak = 0;
            }
        }

        PyObject *chain = ucs4_array_to_unicode(next->data, next->len);
        if (!chain) {
            seqbuf_dec(next);
            Py_DECREF(steps);
            seqbuf_dec(cur);
            return -1;
        }
        if (PyList_Append(steps, chain) < 0) {
            Py_DECREF(chain);
            seqbuf_dec(next);
            Py_DECREF(steps);
            seqbuf_dec(cur);
            return -1;
        }
        Py_DECREF(chain);

        seqbuf_dec(cur);
        cur = next;

        if (op.type == OP_REPLACE) {
            i += 1;
            j += 1;
        } else if (op.type == OP_INSERT) {
            j += 1;
        } else if (op.type == OP_DELETE) {
            i += 1;
        }
        pos = new_pos;
        g += 1;
    }

    *out_steps = steps;
    *out_seq = cur;
    *out_i = i;
    *out_j = j;
    *out_pos = pos;
    *out_g = g;
    *out_invalid_streak = invalid_streak;
    return 1;
}

static PyObject *
merge_steps(PyObject *prefix, PyObject *suffix) {
    if (!prefix) {
        Py_INCREF(suffix);
        return suffix;
    }
    if (!suffix) {
        Py_INCREF(prefix);
        return prefix;
    }
    Py_ssize_t a = PyList_Size(prefix);
    Py_ssize_t b = PyList_Size(suffix);
    if (a < 0 || b < 0) return NULL;
    PyObject *out = PyList_New(0);
    if (!out) return NULL;
    for (Py_ssize_t i = 0; i < a; i++) {
        PyObject *item = PyList_GetItem(prefix, i);
        PyList_Append(out, item);
    }
    for (Py_ssize_t i = 1; i < b; i++) {
        PyObject *item = PyList_GetItem(suffix, i);
        PyList_Append(out, item);
    }
    return out;
}

static PyObject *
build_chain_from_ops(const Op *ops, Py_ssize_t ops_len, const Py_UCS4 *a, Py_ssize_t m) {
    Py_UCS4 *buf = (Py_UCS4 *)PyMem_Malloc(sizeof(Py_UCS4) * (size_t)(m + ops_len + 1));
    if (!buf) {
        PyErr_NoMemory();
        return NULL;
    }
    memcpy(buf, a, sizeof(Py_UCS4) * (size_t)m);
    Py_ssize_t cur_len = m;
    Py_ssize_t pos = 0;
    for (Py_ssize_t k = 0; k < ops_len; k++) {
        Op op = ops[k];
        if (op.type == OP_MATCH) {
            pos += 1;
        } else if (op.type == OP_REPLACE) {
            if (pos < 0 || pos >= cur_len) {
                PyMem_Free(buf);
                PyErr_SetString(PyExc_RuntimeError, "Replace index out of range");
                return NULL;
            }
            buf[pos] = op.b;
            pos += 1;
        } else if (op.type == OP_INSERT) {
            if (pos < 0 || pos > cur_len) {
                PyMem_Free(buf);
                PyErr_SetString(PyExc_RuntimeError, "Insert index out of range");
                return NULL;
            }
            memmove(buf + pos + 1, buf + pos, sizeof(Py_UCS4) * (size_t)(cur_len - pos));
            buf[pos] = op.b;
            cur_len += 1;
            pos += 1;
        } else if (op.type == OP_DELETE) {
            if (pos < 0 || pos >= cur_len) {
                PyMem_Free(buf);
                PyErr_SetString(PyExc_RuntimeError, "Delete index out of range");
                return NULL;
            }
            memmove(buf + pos, buf + pos + 1, sizeof(Py_UCS4) * (size_t)(cur_len - pos - 1));
            cur_len -= 1;
        }
    }
    PyObject *chain = ucs4_array_to_unicode(buf, cur_len);
    PyMem_Free(buf);
    return chain;
}

static PyObject *
best_first_search_from(SearchCtx *ctx, const int *dp, const int *dp_rev, int max_extra,
                       Py_ssize_t start_i, Py_ssize_t start_j, Py_ssize_t start_pos,
                       int start_g, int start_steps_since_validation) {
    int min_dist = dp[ctx->m * (ctx->n + 1) + ctx->n];
    int max_dist = min_dist + max_extra;

    BFQueue q;
    if (!bfq_init(&q, 1024)) {
        PyErr_NoMemory();
        return NULL;
    }
    q.beam_limit = (size_t)ctx->beam_size;

    BFNode start;
    start.i = start_i;
    start.j = start_j;
    start.pos = start_pos;
    start.g = start_g;
    start.f = start_g + dp_rev[(ctx->m - start_i) * (ctx->n + 1) + (ctx->n - start_j)];
    start.parent = -1;
    start.op = (Op){OP_MATCH, 0, 0};
    start.steps_since_validation = start_steps_since_validation;
    if (!bfq_push_node(&q, start)) {
        bfq_free(&q);
        PyErr_NoMemory();
        return NULL;
    }

    while (q.size > 0) {
        if (time_exceeded(ctx)) {
            bfq_free(&q);
            return NULL;
        }
        BFNode cur;
        int cur_idx = -1;
        if (!bfq_pop(&q, &cur_idx, &cur)) break;

        if (ctx->verbose) {
            ctx->nodes_visited += 1;
            if (ctx->log_every > 0 && ctx->nodes_visited % ctx->log_every == 0) {
                fprintf(stdout, "[dsc] nodes=%zd i=%zd j=%zd g=%d f=%d queue=%zu\n",
                        ctx->nodes_visited, cur.i, cur.j, cur.g, cur.f, q.size);
                fflush(stdout);
            }
        }
        if (ctx->max_nodes > 0 && ctx->nodes_visited >= ctx->max_nodes) break;
        if (cur.f > max_dist) continue;

        if (cur.i == ctx->m && cur.j == ctx->n) {
            Py_ssize_t steps_len = 0;
            for (int t = cur_idx; t >= 0; t = q.nodes[t].parent) {
                steps_len++;
                if (q.nodes[t].parent < 0) break;
            }
            Op *ops = (Op *)PyMem_Malloc(sizeof(Op) * (size_t)(steps_len > 0 ? steps_len - 1 : 0));
            if (!ops && steps_len > 1) {
                bfq_free(&q);
                PyErr_NoMemory();
                return NULL;
            }
            Py_ssize_t idx = steps_len - 1;
            for (int t = cur_idx; t >= 0; t = q.nodes[t].parent) {
                if (q.nodes[t].parent < 0) break;
                ops[--idx] = q.nodes[t].op;
            }
            PyObject *steps = build_steps_from_ops(ctx->a, ctx->m, ops, steps_len - 1);
            PyMem_Free(ops);
            bfq_free(&q);
            return steps;
        }

        if (cur.i < ctx->m && cur.j < ctx->n && ctx->a[cur.i] == ctx->b[cur.j]) {
            BFNode next;
            next.i = cur.i + 1;
            next.j = cur.j + 1;
            next.g = cur.g;
            int h = dp_rev[(ctx->m - next.i) * (ctx->n + 1) + (ctx->n - next.j)];
            next.f = next.g + h;
            if (next.f <= max_dist) {
                next.parent = cur_idx;
                next.op = (Op){OP_MATCH, ctx->a[cur.i], 0};
                next.pos = cur.pos + 1;
                next.steps_since_validation = cur.steps_since_validation + 1;
                if (!bfq_push_node(&q, next)) {
                    bfq_free(&q);
                    PyErr_NoMemory();
                    return NULL;
                }
            }
        }

        if (cur.i < ctx->m && cur.j < ctx->n) {
            Py_ssize_t new_pos = cur.pos + 1;
            int next_steps = cur.steps_since_validation + 1;
            if (ctx->validator && ctx->max_unvalidated_steps >= 0 && next_steps >= ctx->max_unvalidated_steps) {
                Py_ssize_t steps_len = 0;
                for (int t = cur_idx; t >= 0; t = q.nodes[t].parent) {
                    steps_len++;
                    if (q.nodes[t].parent < 0) break;
                }
                Op *ops = (Op *)PyMem_Malloc(sizeof(Op) * (size_t)steps_len);
                if (!ops) {
                    bfq_free(&q);
                    PyErr_NoMemory();
                    return NULL;
                }
                Py_ssize_t idx = steps_len;
                for (int t = cur_idx; t >= 0; t = q.nodes[t].parent) {
                    if (q.nodes[t].parent < 0) break;
                    ops[--idx] = q.nodes[t].op;
                }
                ops[steps_len - 1] = (Op){OP_REPLACE, ctx->a[cur.i], ctx->b[cur.j]};
                PyObject *chain = build_chain_from_ops(ops, steps_len, ctx->a, ctx->m);
                PyMem_Free(ops);
                if (!chain) {
                    bfq_free(&q);
                    return NULL;
                }
                ctx->validations += 1;
                PyObject *valid = PyObject_CallFunctionObjArgs(ctx->validator, chain, NULL);
                Py_DECREF(chain);
                if (!valid) {
                    bfq_free(&q);
                    return NULL;
                }
                int ok = PyObject_IsTrue(valid);
                Py_DECREF(valid);
                if (!ok) {
                    ctx->validation_failures += 1;
                    goto skip_replace;
                }
                next_steps = 0;
            }
            BFNode next;
            next.i = cur.i + 1;
            next.j = cur.j + 1;
            next.g = cur.g + 1;
            int h = dp_rev[(ctx->m - next.i) * (ctx->n + 1) + (ctx->n - next.j)];
            next.f = next.g + h;
            if (next.f <= max_dist) {
                next.parent = cur_idx;
                next.op = (Op){OP_REPLACE, ctx->a[cur.i], ctx->b[cur.j]};
                next.pos = new_pos;
                next.steps_since_validation = next_steps;
                if (!bfq_push_node(&q, next)) {
                    bfq_free(&q);
                    PyErr_NoMemory();
                    return NULL;
                }
            }
        }
        skip_replace:

        if (cur.j < ctx->n) {
            Py_ssize_t new_pos = cur.pos + 1;
            int next_steps = cur.steps_since_validation + 1;
            if (ctx->validator && ctx->max_unvalidated_steps >= 0 && next_steps >= ctx->max_unvalidated_steps) {
                Py_ssize_t steps_len = 0;
                for (int t = cur_idx; t >= 0; t = q.nodes[t].parent) {
                    steps_len++;
                    if (q.nodes[t].parent < 0) break;
                }
                Op *ops = (Op *)PyMem_Malloc(sizeof(Op) * (size_t)steps_len);
                if (!ops) {
                    bfq_free(&q);
                    PyErr_NoMemory();
                    return NULL;
                }
                Py_ssize_t idx = steps_len;
                for (int t = cur_idx; t >= 0; t = q.nodes[t].parent) {
                    if (q.nodes[t].parent < 0) break;
                    ops[--idx] = q.nodes[t].op;
                }
                ops[steps_len - 1] = (Op){OP_INSERT, 0, ctx->b[cur.j]};
                PyObject *chain = build_chain_from_ops(ops, steps_len, ctx->a, ctx->m);
                PyMem_Free(ops);
                if (!chain) {
                    bfq_free(&q);
                    return NULL;
                }
                ctx->validations += 1;
                PyObject *valid = PyObject_CallFunctionObjArgs(ctx->validator, chain, NULL);
                Py_DECREF(chain);
                if (!valid) {
                    bfq_free(&q);
                    return NULL;
                }
                int ok = PyObject_IsTrue(valid);
                Py_DECREF(valid);
                if (!ok) {
                    ctx->validation_failures += 1;
                    goto skip_insert;
                }
                next_steps = 0;
            }
            BFNode next;
            next.i = cur.i;
            next.j = cur.j + 1;
            next.g = cur.g + 1;
            int h = dp_rev[(ctx->m - next.i) * (ctx->n + 1) + (ctx->n - next.j)];
            next.f = next.g + h;
            if (next.f <= max_dist) {
                next.parent = cur_idx;
                next.op = (Op){OP_INSERT, 0, ctx->b[cur.j]};
                next.pos = new_pos;
                next.steps_since_validation = next_steps;
                if (!bfq_push_node(&q, next)) {
                    bfq_free(&q);
                    PyErr_NoMemory();
                    return NULL;
                }
            }
        }
        skip_insert:

        if (cur.i < ctx->m) {
            Py_ssize_t new_pos = cur.pos;
            int next_steps = cur.steps_since_validation + 1;
            if (ctx->validator && ctx->max_unvalidated_steps >= 0 && next_steps >= ctx->max_unvalidated_steps) {
                Py_ssize_t steps_len = 0;
                for (int t = cur_idx; t >= 0; t = q.nodes[t].parent) {
                    steps_len++;
                    if (q.nodes[t].parent < 0) break;
                }
                Op *ops = (Op *)PyMem_Malloc(sizeof(Op) * (size_t)steps_len);
                if (!ops) {
                    bfq_free(&q);
                    PyErr_NoMemory();
                    return NULL;
                }
                Py_ssize_t idx = steps_len;
                for (int t = cur_idx; t >= 0; t = q.nodes[t].parent) {
                    if (q.nodes[t].parent < 0) break;
                    ops[--idx] = q.nodes[t].op;
                }
                ops[steps_len - 1] = (Op){OP_DELETE, ctx->a[cur.i], 0};
                PyObject *chain = build_chain_from_ops(ops, steps_len, ctx->a, ctx->m);
                PyMem_Free(ops);
                if (!chain) {
                    bfq_free(&q);
                    return NULL;
                }
                ctx->validations += 1;
                PyObject *valid = PyObject_CallFunctionObjArgs(ctx->validator, chain, NULL);
                Py_DECREF(chain);
                if (!valid) {
                    bfq_free(&q);
                    return NULL;
                }
                int ok = PyObject_IsTrue(valid);
                Py_DECREF(valid);
                if (!ok) {
                    ctx->validation_failures += 1;
                    goto skip_delete;
                }
                next_steps = 0;
            }
            BFNode next;
            next.i = cur.i + 1;
            next.j = cur.j;
            next.g = cur.g + 1;
            int h = dp_rev[(ctx->m - next.i) * (ctx->n + 1) + (ctx->n - next.j)];
            next.f = next.g + h;
            if (next.f <= max_dist) {
                next.parent = cur_idx;
                next.op = (Op){OP_DELETE, ctx->a[cur.i], 0};
                next.pos = new_pos;
                next.steps_since_validation = next_steps;
                if (!bfq_push_node(&q, next)) {
                    bfq_free(&q);
                    PyErr_NoMemory();
                    return NULL;
                }
            }
        }
        skip_delete:
        ;
    }

    bfq_free(&q);
    return NULL;
}

static int
search_paths(SearchCtx *ctx, Py_ssize_t i, Py_ssize_t j, Py_ssize_t depth) {
    if (ctx->verbose) {
        ctx->nodes_visited += 1;
        if (ctx->log_every > 0 && ctx->nodes_visited % ctx->log_every == 0) {
            fprintf(stdout, "[dsc] nodes=%zd depth=%zd i=%zd j=%zd\n",
                    ctx->nodes_visited, depth, i, j);
            fflush(stdout);
        }
    }
    if (i == 0 && j == 0) {
        Py_ssize_t ops_len = depth;
        Op *forward = (Op *)PyMem_Malloc(sizeof(Op) * (size_t)ops_len);
        if (!forward) {
            PyErr_NoMemory();
            return -1;
        }
        for (Py_ssize_t k = 0; k < ops_len; k++) {
            forward[k] = ctx->stack[ops_len - 1 - k];
        }
        PyObject *steps = try_steps_with_validator(ctx, forward, ops_len);
        PyMem_Free(forward);
        if (steps) {
            ctx->steps_found = steps;
            return 1;
        }
        if (PyErr_Occurred()) {
            return -1;
        }
        return 0;
    }

    if (depth >= ctx->max_ops) {
        return 0;
    }

    int rc;
    if (i > 0 && j > 0 && ctx->a[i - 1] == ctx->b[j - 1]) {
        ctx->stack[depth] = (Op){OP_MATCH, ctx->a[i - 1], 0};
        rc = search_paths(ctx, i - 1, j - 1, depth + 1);
        if (rc != 0) return rc;
    }
    if (i > 0 && j > 0) {
        ctx->stack[depth] = (Op){OP_REPLACE, ctx->a[i - 1], ctx->b[j - 1]};
        rc = search_paths(ctx, i - 1, j - 1, depth + 1);
        if (rc != 0) return rc;
    }
    if (j > 0) {
        ctx->stack[depth] = (Op){OP_INSERT, 0, ctx->b[j - 1]};
        rc = search_paths(ctx, i, j - 1, depth + 1);
        if (rc != 0) return rc;
    }
    if (i > 0) {
        ctx->stack[depth] = (Op){OP_DELETE, ctx->a[i - 1], 0};
        rc = search_paths(ctx, i - 1, j, depth + 1);
        if (rc != 0) return rc;
    }
    return 0;
}

static PyObject *
algo_seq_dynamic_with_validation_run(PyObject *self, PyObject *args, PyObject *kwargs) {
    PyObject *a_obj;
    PyObject *b_obj;
    PyObject *validator = Py_None;
    static char *kwlist[] = {"a_seq", "b_seq", "validator", NULL};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "UU|O", kwlist, &a_obj, &b_obj, &validator)) {
        return NULL;
    }
    if (validator == Py_None) {
        validator = NULL;
    } else if (!PyCallable_Check(validator)) {
        PyErr_SetString(PyExc_TypeError, "validator must be callable");
        return NULL;
    }

    Py_UCS4 *a = NULL;
    Py_UCS4 *b = NULL;
    Py_ssize_t m = 0;
    Py_ssize_t n = 0;
    if (unicode_to_ucs4_array(a_obj, &a, &m) < 0) {
        return NULL;
    }
    if (unicode_to_ucs4_array(b_obj, &b, &n) < 0) {
        PyMem_Free(a);
        return NULL;
    }

    int *dp = NULL;
    if (compute_dp(a, m, b, n, &dp) < 0) {
        PyMem_Free(a);
        PyMem_Free(b);
        return NULL;
    }
    int min_dist = dp[m * (n + 1) + n];

    Py_UCS4 *a_rev = (Py_UCS4 *)PyMem_Malloc(sizeof(Py_UCS4) * (size_t)m);
    Py_UCS4 *b_rev = (Py_UCS4 *)PyMem_Malloc(sizeof(Py_UCS4) * (size_t)n);
    if (!a_rev || !b_rev) {
        PyMem_Free(a_rev);
        PyMem_Free(b_rev);
        PyMem_Free(a);
        PyMem_Free(b);
        PyMem_Free(dp);
        PyErr_NoMemory();
        return NULL;
    }
    for (Py_ssize_t i = 0; i < m; i++) a_rev[i] = a[m - 1 - i];
    for (Py_ssize_t j = 0; j < n; j++) b_rev[j] = b[n - 1 - j];

    int *dp_rev = NULL;
    if (compute_dp(a_rev, m, b_rev, n, &dp_rev) < 0) {
        PyMem_Free(a_rev);
        PyMem_Free(b_rev);
        PyMem_Free(a);
        PyMem_Free(b);
        PyMem_Free(dp);
        return NULL;
    }

    PyObject *steps_found = NULL;
    PyObject *validator_to_use = validator ? validator : NULL;
    if (validator_to_use) {
        Py_INCREF(validator_to_use);
    }

    SearchCtx ctx;
    ctx.a = a;
    ctx.b = b;
    ctx.m = m;
    ctx.n = n;
    ctx.validator = validator_to_use;
    ctx.steps_found = NULL;
    ctx.stack = NULL;
    ctx.max_ops = 0;
    ctx.verbose = 0;
    ctx.nodes_visited = 0;
    ctx.validations = 0;
    ctx.validation_failures = 0;
    ctx.steps_checked = 0;
    ctx.log_every = 10000;
    ctx.log_fail_limit = 5;
    ctx.max_nodes = 2000000;
    ctx.beam_size = 50000;
    ctx.max_unvalidated_steps = 0;
    ctx.time_limit_s = 0.0;
    ctx.start_time_s = 0.0;

    const char *verbose_env = getenv("DSC_VERBOSE");
    if (verbose_env && verbose_env[0] != '\0' && verbose_env[0] != '0') {
        ctx.verbose = 1;
    }
    const char *log_every_env = getenv("DSC_LOG_EVERY");
    if (log_every_env && log_every_env[0] != '\0') {
        long v = strtol(log_every_env, NULL, 10);
        if (v > 0) ctx.log_every = (Py_ssize_t)v;
    }
    const char *log_fail_env = getenv("DSC_LOG_FAILS");
    if (log_fail_env && log_fail_env[0] != '\0') {
        long v = strtol(log_fail_env, NULL, 10);
        if (v >= 0) ctx.log_fail_limit = (Py_ssize_t)v;
    }
    const char *max_nodes_env = getenv("DSC_MAX_NODES");
    if (max_nodes_env && max_nodes_env[0] != '\0') {
        long v = strtol(max_nodes_env, NULL, 10);
        if (v > 0) ctx.max_nodes = (Py_ssize_t)v;
    }
    const char *beam_env = getenv("DSC_BEAM_SIZE");
    if (beam_env && beam_env[0] != '\0') {
        long v = strtol(beam_env, NULL, 10);
        if (v > 0) ctx.beam_size = (Py_ssize_t)v;
    }
    const char *unval_env = getenv("DSC_MAX_UNVALIDATED_STEPS");
    if (unval_env && unval_env[0] != '\0') {
        long v = strtol(unval_env, NULL, 10);
        if (v >= 0) ctx.max_unvalidated_steps = (int)v;
    }
    const char *time_env = getenv("DSC_MAX_SECONDS");
    if (time_env && time_env[0] != '\0') {
        char *endp = NULL;
        double v = strtod(time_env, &endp);
        if (endp != time_env && v > 0.0) ctx.time_limit_s = v;
    }
    double t0 = 0.0;
    if (ctx.verbose) {
        t0 = now_seconds();
        fprintf(stdout, "[dsc] start m=%zd n=%zd min_dist=%d\n", m, n, min_dist);
        fflush(stdout);
    }
    ctx.start_time_s = now_seconds();

    Py_ssize_t ops_len = 0;
    PyObject *ops_capsule = reverse_ops_from_dp(a, m, b, n, dp, &ops_len);
    if (!ops_capsule) {
        Py_XDECREF(validator_to_use);
        PyMem_Free(a);
        PyMem_Free(b);
        PyMem_Free(dp);
        PyMem_Free(a_rev);
        PyMem_Free(b_rev);
        PyMem_Free(dp_rev);
        return NULL;
    }
    Op *ops = (Op *)PyCapsule_GetPointer(ops_capsule, "ops");
    if (!ops) {
        Py_DECREF(ops_capsule);
        Py_XDECREF(validator_to_use);
        PyMem_Free(a);
        PyMem_Free(b);
        PyMem_Free(dp);
        PyMem_Free(a_rev);
        PyMem_Free(b_rev);
        PyMem_Free(dp_rev);
        return NULL;
    }

    if (!validator_to_use) {
        steps_found = build_steps_from_ops(a, m, ops, ops_len);
    } else {
        PyObject *prefix_steps = NULL;
        SeqBuf *cur_seq = NULL;
        Py_ssize_t ci = 0, cj = 0, cpos = 0;
        int cg = 0;
        int cstreak = 0;
    int rc = validate_ops_prefix(&ctx, ops, ops_len, &prefix_steps, &cur_seq, &ci, &cj, &cpos, &cg, &cstreak);
        if (rc < 0) {
            Py_DECREF(ops_capsule);
            Py_XDECREF(validator_to_use);
            PyMem_Free(a);
            PyMem_Free(b);
            PyMem_Free(dp);
            PyMem_Free(a_rev);
            PyMem_Free(b_rev);
            PyMem_Free(dp_rev);
            return NULL;
        }
        if (rc == 1) {
            steps_found = prefix_steps;
            seqbuf_dec(cur_seq);
        } else {
            PyObject *suffix_steps = best_first_search_from(&ctx, dp, dp_rev, 4, ci, cj, cpos, cg, cstreak);
            if (suffix_steps) {
                steps_found = merge_steps(prefix_steps, suffix_steps);
                Py_DECREF(suffix_steps);
            }
            Py_DECREF(prefix_steps);
            seqbuf_dec(cur_seq);
        }
    }
    Py_DECREF(ops_capsule);

    if (!steps_found && PyErr_Occurred()) {
        Py_XDECREF(validator_to_use);
        PyMem_Free(a);
        PyMem_Free(b);
        PyMem_Free(dp);
        PyMem_Free(a_rev);
        PyMem_Free(b_rev);
        PyMem_Free(dp_rev);
        return NULL;
    }

    if (ctx.verbose) {
        double t1 = now_seconds();
        fprintf(stdout,
                "[dsc] done nodes=%zd paths_checked=%zd validations=%zd failures=%zd elapsed=%.3fs\n",
                ctx.nodes_visited, ctx.steps_checked, ctx.validations, ctx.validation_failures, t1 - t0);
        fflush(stdout);
    }

    Py_XDECREF(validator_to_use);
    PyMem_Free(a);
    PyMem_Free(b);
    PyMem_Free(dp);
    PyMem_Free(a_rev);
    PyMem_Free(b_rev);
    PyMem_Free(dp_rev);

    if (!steps_found) {
        PyObject *none = PyTuple_Pack(3, Py_None, Py_None, Py_None);
        return none;
    }

    PyObject *ops_list = build_operations_from_steps(steps_found);
    if (!ops_list) {
        Py_DECREF(steps_found);
        return NULL;
    }
    Py_ssize_t ops_len2 = PyList_Size(ops_list);
    Py_ssize_t dist_weight = 0;
    for (Py_ssize_t i = 0; i < ops_len2; i++) {
        PyObject *op = PyList_GetItem(ops_list, i);
        if (!PyTuple_Check(op) || PyTuple_Size(op) < 1) {
            dist_weight += 1;
            continue;
        }
        PyObject *name = PyTuple_GetItem(op, 0);
        if (name && PyUnicode_Check(name)) {
            if (PyUnicode_CompareWithASCIIString(name, "reverse") == 0) {
                continue;
            }
        }
        dist_weight += 1;
    }
    PyObject *dist_obj = PyLong_FromSsize_t(dist_weight);
    if (!dist_obj) {
        Py_DECREF(steps_found);
        Py_DECREF(ops_list);
        return NULL;
    }
    PyObject *result = PyTuple_Pack(3, steps_found, ops_list, dist_obj);
    Py_DECREF(dist_obj);
    Py_DECREF(steps_found);
    Py_DECREF(ops_list);
    return result;
}

static PyMethodDef module_methods[] = {
    {"algo_seq_dynamic_run", (PyCFunction)algo_seq_dynamic_run, METH_VARARGS, "Run AlgoSeqDynamic on two strings."},
    {"algo_seq_dynamic_with_validation_run", (PyCFunction)algo_seq_dynamic_with_validation_run, METH_VARARGS | METH_KEYWORDS, "Run AlgoSeqDynamicWithValidation on two strings."},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "_core",
    "C implementation of datasetgradualstep algorithms.",
    -1,
    module_methods
};

PyMODINIT_FUNC
PyInit__core(void) {
    return PyModule_Create(&moduledef);
}
