#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdlib.h>
#include <string.h>


typedef enum {
    OP_MATCH = 0,
    OP_REPLACE = 1,
    OP_INSERT = 2,
    OP_DELETE = 3,
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
        if (curr_len > prev_len) {
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
        } else if (curr_len < prev_len) {
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
        } else {
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
    int *dp; 
} SearchCtx;

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
    return steps;
}

static int
search_paths(SearchCtx *ctx, Py_ssize_t i, Py_ssize_t j, Py_ssize_t depth) {
    if (i == ctx->m && j == ctx->n) {
        Py_ssize_t ops_len = depth;
        Op *forward = (Op *)PyMem_Malloc(sizeof(Op) * (size_t)ops_len);
        if (!forward) return -1;
        for (Py_ssize_t k = 0; k < ops_len; k++) {
            forward[k] = ctx->stack[k];
        }
        PyObject *steps = try_steps_with_validator(ctx, forward, ops_len);
        PyMem_Free(forward);
        if (steps) {
            ctx->steps_found = steps;
            return 1;
        }
        return 0;
    }

    if (depth >= ctx->max_ops) return 0;

    int current_val = ctx->dp[i * (ctx->n + 1) + j];

    if (i < ctx->m && j < ctx->n && ctx->a[i] == ctx->b[j] && 
        ctx->dp[(i + 1) * (ctx->n + 1) + (j + 1)] == current_val) {
        ctx->stack[depth] = (Op){OP_MATCH, ctx->a[i], 0};
        if (search_paths(ctx, i + 1, j + 1, depth + 1)) return 1;
    }

    if (i < ctx->m && ctx->dp[(i + 1) * (ctx->n + 1) + j] == current_val + 1) {
        ctx->stack[depth] = (Op){OP_DELETE, ctx->a[i], 0};
        if (search_paths(ctx, i + 1, j, depth + 1)) return 1;
    }

    if (j < ctx->n && ctx->dp[i * (ctx->n + 1) + (j + 1)] == current_val + 1) {
        ctx->stack[depth] = (Op){OP_INSERT, 0, ctx->b[j]};
        if (search_paths(ctx, i, j + 1, depth + 1)) return 1;
    }

    if (i < ctx->m && j < ctx->n && ctx->dp[(i + 1) * (ctx->n + 1) + (j + 1)] == current_val + 1) {
        ctx->stack[depth] = (Op){OP_REPLACE, ctx->a[i], ctx->b[j]};
        if (search_paths(ctx, i + 1, j + 1, depth + 1)) return 1;
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
    ctx.dp = dp;

    for (int extra = 0; extra < 5; extra++) {
        ctx.max_ops = (Py_ssize_t)min_dist + extra;
        if (ctx.max_ops < 0) ctx.max_ops = 0;
        ctx.stack = (Op *)PyMem_Malloc(sizeof(Op) * (size_t)(ctx.max_ops + 1));
        if (!ctx.stack) {
            Py_XDECREF(validator_to_use);
            PyMem_Free(a);
            PyMem_Free(b);
            PyMem_Free(dp);
            PyErr_NoMemory();
            return NULL;
        }
        int rc = search_paths(&ctx, 0, 0, 0); 
        PyMem_Free(ctx.stack);
        ctx.stack = NULL;
        if (rc < 0) {
            Py_XDECREF(validator_to_use);
            PyMem_Free(a);
            PyMem_Free(b);
            PyMem_Free(dp);
            return NULL;
        }
        if (rc > 0) {
            steps_found = ctx.steps_found;
            break;
        }
    }

    Py_XDECREF(validator_to_use);
    PyMem_Free(a);
    PyMem_Free(b);
    PyMem_Free(dp);

    if (!steps_found) {
        PyObject *none = PyTuple_Pack(3, Py_None, Py_None, Py_None);
        return none;
    }

    PyObject *ops_list = build_operations_from_steps(steps_found);
    if (!ops_list) {
        Py_DECREF(steps_found);
        return NULL;
    }
    Py_ssize_t ops_len = PyList_Size(ops_list);
    PyObject *dist_obj = PyLong_FromSsize_t(ops_len);
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
    "datasetgradualstep_c",
    "C implementation of datasetgradualstep algorithms.",
    -1,
    module_methods
};

PyMODINIT_FUNC
PyInit_datasetgradualstep_c(void) {
    return PyModule_Create(&moduledef);
}
