#define PY_SSIZE_T_CLEAN
#include <memory>
#include <string_view>
#include <histograms/histograms.hh>
#include <Python.h>

namespace histograms::python {

PyObject* py(double x) { return PyFloat_FromDouble(x); }
PyObject* py(float  x) { return PyFloat_FromDouble((double)x); }
PyObject* py(long   x) { return PyLong_FromLong(x); }
PyObject* py(unsigned long x) { return PyLong_FromUnsignedLong(x); }
// PyObject* py(size_t x) { return PyLong_FromSize_t(x); }
PyObject* py(std::string_view x) {
  // return PyBytes_FromStringAndSize(x.data(),x.size());
  return PyUnicode_FromStringAndSize(x.data(),x.size());
}

struct py_decref_deleter {
  void operator()(PyObject* ref) { Py_DECREF(ref); }
};

template <typename Bin>
struct py_hist {
  PyObject_HEAD
  histogram<
    Bin,
    std::vector< std::shared_ptr<poly_axis_base<double>> >
  > h;
};

/*
PyTypeObject py_hist_double = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "histograms.histogram_double",
  .tp_doc = "Histogram with double bins",
  .tp_basicsize = sizeof(py_hist<double>),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_new = PyType_GenericNew
};
*/

PyObject* make(PyObject *self, PyObject *args) {
  return py("This is a histogram");
}

PyMethodDef methods[] = {
  { "histogram", make, METH_VARARGS | METH_KEYWORDS,
    "initializes a histogram" },
  { NULL, NULL, 0, NULL } // sentinel
};

PyModuleDef module = {
  PyModuleDef_HEAD_INIT,
  "histograms",
  NULL,
  -1,
  methods
};

} // end namespace

PyMODINIT_FUNC PyInit_histograms(void) {
  PyObject* m = PyModule_Create(&histograms::python::module);
  if (!m) return nullptr;

  /*
  Py_INCREF(&CustomType);
  if ( PyModule_AddObject(
      m, "histogram_double", (PyObject*)&py_hist_double ) < 0
  ) {
    Py_DECREF(&py_hist_double);
    Py_DECREF(m);
    return nullptr;
  }
  */

  return m;
}

