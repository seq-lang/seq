#include "lib.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#if PYBRIDGE
#include <Python.h>
#endif

void seq_py_init() {
#if PYBRIDGE
  Py_Initialize();
#endif
}

#if PYBRIDGE
#if PY_MAJOR_VERSION != 3
#error "Only Python 3 is supported."
#endif

// caution: this type must be consistent with PyException as defined in Seq
struct PyException {
  seq_str_t msg;
  seq_str_t type;
};

static seq_str_t seq_strdup(const char *s) {
  size_t len = strlen(s);
  auto *s2 = (char *)seq_alloc_atomic(len + 1);
  strcpy(s2, s);
  return {(seq_int_t)len, s2};
}

static const char *to_str(PyObject *obj) {
  PyObject *str = PyUnicode_AsEncodedString(obj, "utf-8", "~E~");
  const char *bytes = strdup(PyBytes_AS_STRING(str));
  Py_XDECREF(str);
  return bytes;
}

static void seq_py_exc_check() {
  // if an exception occurs, transform it into a Seq exception:
  PyObject *ptype, *pvalue, *ptraceback;
  PyErr_Fetch(&ptype, &pvalue, &ptraceback);

  if (ptype) {
    PyObject *pmsg = pvalue ? PyObject_Str(pvalue) : nullptr;
    const char *msg = pmsg ? to_str(pmsg) : "<empty Python message>";
    const char *type = ((PyTypeObject *)ptype)->tp_name;
    auto *seqExc = (PyException *)seq_alloc(sizeof(PyException));
    seqExc->msg = seq_strdup(msg);
    seqExc->type = seq_strdup(type);

    Py_DECREF(ptype);
    Py_XDECREF(pvalue);
    Py_XDECREF(ptraceback);
    Py_XDECREF(pmsg);

    const int seqExcType = (int)std::hash<std::string>()("PyException");
    seq_throw(seq_alloc_exc(seqExcType, seqExc));
    assert(0);
  }
}

#define SEQ_PY_RETURN(x)                                                       \
  do {                                                                         \
    auto __retval = (x);                                                       \
    seq_py_exc_check();                                                        \
    return __retval;                                                           \
  } while (0)

SEQ_FUNC PyObject *seq_py_import(char *name) {
  SEQ_PY_RETURN(PyImport_ImportModule(name));
}

SEQ_FUNC PyObject *seq_py_get_attr(PyObject *obj, char *name) {
  SEQ_PY_RETURN(PyObject_GetAttrString(obj, name));
}

SEQ_FUNC void seq_py_set_attr(PyObject *obj, char *name, PyObject *val) {
  PyObject_SetAttrString(obj, name, val);
  seq_py_exc_check();
}

SEQ_FUNC PyObject *seq_py_call(PyObject *func, PyObject *args) {
  SEQ_PY_RETURN(PyObject_CallObject(func, args));
}

SEQ_FUNC void seq_py_incref(PyObject *obj) { Py_XINCREF(obj); }

SEQ_FUNC void seq_py_decref(PyObject *obj) { Py_XDECREF(obj); }

/* conversions */

SEQ_FUNC PyObject *seq_int_to_py(seq_int_t n) { return PyLong_FromLong(n); }

SEQ_FUNC seq_int_t seq_int_from_py(PyObject *n) {
  SEQ_PY_RETURN(PyLong_AsLong(n));
}

SEQ_FUNC PyObject *seq_float_to_py(double f) { return PyFloat_FromDouble(f); }

SEQ_FUNC double seq_float_from_py(PyObject *f) {
  SEQ_PY_RETURN(PyFloat_AsDouble(f));
}

SEQ_FUNC PyObject *seq_bool_to_py(bool b) { return PyBool_FromLong((long)b); }

SEQ_FUNC bool seq_bool_from_py(PyObject *b) {
  SEQ_PY_RETURN(PyObject_IsTrue(b) != 0);
}

SEQ_FUNC PyObject *seq_str_to_py(seq_str_t s) {
  SEQ_PY_RETURN(PyUnicode_DecodeFSDefaultAndSize(s.str, (Py_ssize_t)s.len));
}

SEQ_FUNC seq_str_t seq_str_from_py(PyObject *s) {
  SEQ_PY_RETURN(seq_strdup(to_str(s)));
}

SEQ_FUNC PyObject *seq_byte_to_py(char c) { return seq_str_to_py({1, &c}); }

SEQ_FUNC char seq_byte_from_py(PyObject *c) {
  auto *s = to_str(c);
  seq_py_exc_check();
  return s[0];
}

SEQ_FUNC PyObject *seq_py_list_new(seq_int_t n) {
  SEQ_PY_RETURN(PyList_New((Py_ssize_t)n));
}

SEQ_FUNC seq_int_t seq_py_list_len(PyObject *list) {
  SEQ_PY_RETURN((seq_int_t)PyObject_Length(list));
}

SEQ_FUNC PyObject *seq_py_list_getitem(PyObject *list, seq_int_t idx) {
  SEQ_PY_RETURN(PyList_GetItem(list, (Py_ssize_t)idx));
}

SEQ_FUNC void seq_py_list_setitem(PyObject *list, seq_int_t idx,
                                  PyObject *item) {
  PyList_SetItem(list, (Py_ssize_t)idx, item);
  seq_py_exc_check();
}

SEQ_FUNC PyObject *seq_py_dict_new() { return PyDict_New(); }

SEQ_FUNC void seq_py_dict_setitem(PyObject *dict, PyObject *key,
                                  PyObject *val) {
  PyDict_SetItem(dict, key, val);
  seq_py_exc_check();
}

SEQ_FUNC bool seq_py_dict_next(PyObject *dict, seq_int_t *pos, PyObject **key,
                               PyObject **val) {
  auto pos2 = (Py_ssize_t)*pos;
  bool b = (PyDict_Next(dict, &pos2, key, val) != 0);
  *pos = (seq_int_t)pos2;
  SEQ_PY_RETURN(b);
}

SEQ_FUNC PyObject *seq_py_set_new() { return PySet_New(nullptr); }

SEQ_FUNC void seq_py_set_add(PyObject *set, PyObject *key) {
  PySet_Add(set, key);
  seq_py_exc_check();
}

SEQ_FUNC PyObject *seq_py_tuple_new(seq_int_t n) {
  SEQ_PY_RETURN(PyTuple_New((Py_ssize_t)n));
}

SEQ_FUNC PyObject *seq_py_tuple_getitem(PyObject *tuple, seq_int_t n) {
  SEQ_PY_RETURN(PyTuple_GetItem(tuple, (Py_ssize_t)n));
}

SEQ_FUNC void seq_py_tuple_setitem(PyObject *tuple, seq_int_t n,
                                   PyObject *val) {
  PyTuple_SetItem(tuple, (Py_ssize_t)n, val);
  seq_py_exc_check();
}

SEQ_FUNC PyObject *seq_py_none() { return Py_None; }

#endif /* PYBRIDGE */
