#include <memory>
#include <string_view>
#include <stdexcept>
#include <histograms/histograms.hh>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#define STR1(x) #x
#define STR(x) STR1(x)

#define ERROR_PREF __FILE__ ":" STR(__LINE__) ": "

#define ERROR(TYPE,MSG) { \
  PyErr_SetString(TYPE, ERROR_PREF MSG); \
  goto error; \
}

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

void decref(PyObject* x) { Py_DECREF(x); }
using py_ptr = std::unique_ptr<
  PyObject,
  std::integral_constant<std::decay_t<decltype(decref)>,decref>
>;

template <typename T>
void destroy(T* x) { x->~T(); }

struct hist {
  PyObject_HEAD
  using type = histogram<
    py_ptr,
    std::vector< std::shared_ptr<poly_axis_base<double>> >
  >;
  type h;
};

void hist_dealloc(hist *self) {
  static_assert(alignof(hist)==alignof(PyObject));
  destroy(&self->h);
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

int hist_init(hist *self, PyObject *args, PyObject *kwargs) {
  PyObject_Print(args,stdout,0);
  fprintf(stdout,"\n");
  hist::type::axes_type axes;
  union {
    decltype(PyLong_AsLong(std::declval<PyObject*>())) i;
    double d;
  };
  std::vector<double> edges;
  bool is_float;
  unsigned k = 0;
  for (py_ptr iter(PyObject_GetIter(args)), arg;
      (arg.reset(PyIter_Next(iter.get())), arg); ++k)
  {
    PyObject_Print(arg.get(),stdout,0);
    fprintf(stdout,"\n");
    py_ptr iter2(PyObject_GetIter(arg.get()));
    if (!iter2) ERROR(PyExc_TypeError,"argument is not iterable")

    py_ptr arg2(PyIter_Next(iter2.get()));
    if (!arg2) ERROR(PyExc_TypeError,"argument is iterable but empty")
    if (PyLong_Check(arg2.get())) {
      i = PyLong_AsLong(arg2.get());
      if (i==-1 && PyErr_Occurred()) goto error;
      is_float = false;
    } else {
      d = PyFloat_AsDouble(arg2.get());
      if (d==-1 && PyErr_Occurred()) goto error;
      is_float = true;
    }

    PyObject_Print(arg2.get(),stdout,0);
    fprintf(stdout,"\nis_float = %i\n",is_float);
    if (is_float)
      fprintf(stdout,"x = %f\n",d);
    else
      fprintf(stdout,"x = %li\n",i);

    arg2.reset(PyIter_Next(iter2.get()));
    if (arg2) {
      py_ptr iter3(PyObject_GetIter(arg2.get()));
      if (iter3) { // must be a uniform axis
        fprintf(stdout,"uniform_axis\n");

        py_ptr arg3;
        for (int j=0; j<2; ++j) {
          arg3.reset(PyIter_Next(iter3.get()));
          if (!arg3) goto error_uniform;
          double x = PyFloat_AsDouble(arg3.get());
          if (x==-1 && PyErr_Occurred()) goto error;
          edges.push_back(x);
        }

        arg3.reset(PyIter_Next(iter3.get()));
        if (arg3) {
          error_uniform:
          ERROR(PyExc_TypeError,"axis range must have exactly 2 values")
        }

        axes.emplace_back(
          std::make_shared<uniform_axis<double,true>>(
            (is_float ? index_type(d) : index_type(i)), edges[0], edges[1]
        ));
      } else { // must be a container axis
        PyErr_Clear();
        fprintf(stdout,"container_axis\n");
        edges.push_back(is_float ? d : double(i));
        if (arg2) {
          do {
            double x = PyFloat_AsDouble(arg2.get());
            if (x==-1 && PyErr_Occurred()) goto error;
            edges.push_back(x);
          } while ((arg2 = py_ptr(PyIter_Next(iter2.get()))));
        }

        axes.emplace_back(
          std::make_shared<container_axis<std::vector<double>,true>>(
            std::move(edges)
        ));
      }
    } else {
      double x = (is_float ? index_type(d) : index_type(i));
      axes.emplace_back(
        std::make_shared<uniform_axis<double,true>>(
          0, x, x
      ));
    }

    edges.clear();
  }

  new(&self->h) hist::type(std::move(axes));
  for (auto& b : self->h.bins()) {
    b = nullptr;
  }
  return 0;

error:
  return 1;
}

PyObject* hist_nbins(hist *self, PyObject *Py_UNUSED(ignored)) {
  return py(self->h.nbins());
}

PyMethodDef hist_methods[] = {
  { "nbins", (PyCFunction) hist_nbins, METH_NOARGS,
    "total number of histogram bins, including overflow" },
  { nullptr }
};

PyTypeObject hist_type = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  .tp_name = "histograms.histogram",
  .tp_basicsize = sizeof(hist),
  .tp_itemsize = 0,
  .tp_dealloc = (destructor) hist_dealloc,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "histogram object",
  .tp_methods = hist_methods,
  .tp_members = nullptr,
  .tp_init = (initproc) hist_init,
  .tp_new = PyType_GenericNew,
};

/*
PyMethodDef methods[] = {
  { "histogram", make, METH_VARARGS | METH_KEYWORDS,
    "initializes a histogram" },
  { nullptr, nullptr, 0, nullptr } // sentinel
};
*/

PyModuleDef module = {
  PyModuleDef_HEAD_INIT,
  .m_name = "histograms",
  .m_doc = "Python bindings for the histograms library",
  .m_size = -1,
  // methods
};

} // end namespace

PyMODINIT_FUNC PyInit_histograms() {
  using namespace histograms::python;

  if (PyType_Ready(&hist_type) < 0) return nullptr;

  PyObject* m = PyModule_Create(&module);
  if (!m) return nullptr;

  Py_INCREF(&hist_type);
  if ( PyModule_AddObject(
    m, "histogram", reinterpret_cast<PyObject*>(&hist_type) ) < 0
  ) {
    Py_DECREF(&hist_type);
    Py_DECREF(m);
    return nullptr;
  }

  return m;
}

