#include <memory>
#include <string_view>
#include <stdexcept>
#include <histograms/histograms.hh>

#define PY_SSIZE_T_CLEAN
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

template <typename T>
void destroy(T* x) { x->~T(); }

struct hist {
  PyObject_HEAD
  using type = histogram<
    PyObject*,
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
  std::vector<double> nums;
  bool is_float;
  unsigned k = 0;
  for (PyObject *iter = PyObject_GetIter(args), *arg;
      (arg = PyIter_Next(iter)); ++k)
  {
    PyObject_Print(arg,stdout,0);
    fprintf(stdout,"\n");
    PyObject *iter2 = PyObject_GetIter(arg);
    if (!iter2) throw std::runtime_error("bad iter2");

    PyObject *arg2 = PyIter_Next(iter2);
    if (PyLong_Check(arg2)) {
      i = PyLong_AsLong(arg2);
      // if (i==-1) PyErr_Occurred();
      is_float = false;
    } else {
      d = PyFloat_AsDouble(arg2);
      // if (d==-1) PyErr_Occurred(); // TODO: need to check
      is_float = true;
    }

    PyObject_Print(arg2,stdout,0);
    fprintf(stdout,"\nis_float = %i\n",is_float);
    if (is_float)
      fprintf(stdout,"x = %f\n",d);
    else
      fprintf(stdout,"x = %li\n",i);

    arg2 = PyIter_Next(iter2);
    // TODO: need a proper check if iterable
    // can't just check if iter3 is null
    PyObject *iter3 = arg2 ? PyObject_GetIter(arg2) : nullptr;
    if (iter3) { // must be a uniform axis
      PyObject *arg3 = PyIter_Next(iter3);
      if (!arg3) throw std::runtime_error("first arg3 missing");
      nums.push_back(PyFloat_AsDouble(arg3));

      arg3 = PyIter_Next(iter3);
      if (!arg3) throw std::runtime_error("second arg3 missing");
      nums.push_back(PyFloat_AsDouble(arg3));

      arg3 = PyIter_Next(iter3);
      if (arg3) throw std::runtime_error("third arg3");

      axes.emplace_back(
        std::make_shared<uniform_axis<double,true>>(
          (is_float ? index_type(d) : index_type(i)), nums[0], nums[1]
      ));
    } else { // must be a container axis
      nums.push_back(is_float ? d : double(i));
      if (arg2) {
        do nums.push_back(PyFloat_AsDouble(arg2));
        while ((arg2 = PyIter_Next(iter2)));
      }

      axes.emplace_back(
        std::make_shared<container_axis<std::vector<double>,true>>(
          std::move(nums)
      ));
    }

    nums.clear();
  }

  auto& h = self->h;
  new(&h) hist::type(std::move(axes));
  for (auto& b : h.bins()) {
    b = nullptr;
  }
  return 0;
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

