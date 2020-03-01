#include <memory>
#include <string_view>
#include <stdexcept>
#include <histograms/histograms.hh>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

// #define STR1(x) #x
// #define STR(x) STR1(x)

// #define ERROR_PREF __FILE__ ":" STR(__LINE__) ": "

namespace histograms::python {

class occured_error { };

class error: public std::runtime_error {
  PyObject* _type;
public:
  template <typename T>
  error(PyObject* type, const T& arg): runtime_error(arg), _type(type) { }
  PyObject* type() const { return _type; }
};

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

auto as_double_check(const py_ptr& p) {
  const auto x = PyFloat_AsDouble(p.get());
  if (x==-1 && PyErr_Occurred()) throw occured_error{};
  return x;
}
auto as_long_check(const py_ptr& p) {
  const auto x = PyLong_AsLong(p.get());
  if (x==-1 && PyErr_Occurred()) throw occured_error{};
  return x;
}

py_ptr next(const py_ptr& iter) { return py_ptr(PyIter_Next(iter.get())); }

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
  try {
    hist::type::axes_type axes;
    union {
      decltype(PyLong_AsLong(std::declval<PyObject*>())) i;
      double d;
    };
    std::vector<double> edges;
    bool is_float;
    unsigned k = 0;
    for (py_ptr iter(PyObject_GetIter(args)), arg; (arg = next(iter)); ++k) {
      py_ptr iter2(PyObject_GetIter(arg.get()));
      if (!iter2) throw occured_error{};

      py_ptr arg2 = next(iter2);
      if (!arg2) throw error(PyExc_TypeError,
        "argument is iterable but empty");
      if (PyLong_Check(arg2.get())) {
        i = as_long_check(arg2);
        is_float = false;
      } else {
        d = as_double_check(arg2);
        is_float = true;
      }

      arg2 = next(iter2);
      if (arg2) {
        py_ptr iter3(PyObject_GetIter(arg2.get()));
        if (iter3) { // must be a uniform axis
          py_ptr arg3;
          for (int j=0; j<2; ++j) {
            arg3 = next(iter3);
            if (!arg3) throw error(PyExc_TypeError,
              "axis range must have exactly 2 values");
            edges.push_back(as_double_check(arg3));
          }

          arg3 = next(iter3);
          if (arg3) throw error(PyExc_TypeError,
            "axis range must have exactly 2 values");

          axes.emplace_back(
            std::make_shared<uniform_axis<double,true>>(
              (is_float ? index_type(d) : index_type(i)), edges[0], edges[1]
          ));
        } else { // must be a container axis
          PyErr_Clear(); // https://stackoverflow.com/q/60471914/2640636
          edges.push_back(is_float ? d : double(i));
          if (arg2) {
            do edges.push_back(as_double_check(arg2));
            while ((arg2 = next(iter2)));
          }

          axes.emplace_back(
            std::make_shared<container_axis<std::vector<double>,true>>(
              std::move(edges)
          ));
        }
      } else {
        const double x = (is_float ? index_type(d) : index_type(i));
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

  } catch (const occured_error& e) {
    return 1;
  } catch (const error& e) {
    PyErr_SetString(e.type(),e.what());
    return 1;
  } catch (const std::exception& e) {
    PyErr_SetString(PyExc_RuntimeError,e.what());
    return 1;
  }
}

PyObject* hist_nbins(hist *self, PyObject *Py_UNUSED(ignored)) {
  return py(self->h.nbins());
}

PyMethodDef hist_methods[] = {
  { "nbins", (PyCFunction) hist_nbins, METH_NOARGS,
    "total number of histogram bins, including overflow" },
  { nullptr }
};

Py_ssize_t hist_lenfunc(hist *h) { return h->h.nbins(); }

PySequenceMethods hist_sequence_methods = {
  .sq_length = (lenfunc) hist_lenfunc,
  // binaryfunc sq_concat;
  // ssizeargfunc sq_repeat;
  // ssizeargfunc sq_item;
  // void *was_sq_slice;
  // ssizeobjargproc sq_ass_item;
  // void *was_sq_ass_slice;
  // objobjproc sq_contains;
  // binaryfunc sq_inplace_concat;
  // ssizeargfunc sq_inplace_repeat;
};

PyTypeObject hist_pytype = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  .tp_name = "histograms.histogram",
  .tp_basicsize = sizeof(hist),
  .tp_itemsize = 0,
  .tp_dealloc = (destructor) hist_dealloc,
  .tp_as_sequence = &hist_sequence_methods,
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

  if (PyType_Ready(&hist_pytype) < 0) return nullptr;

  PyObject* m = PyModule_Create(&module);
  if (!m) return nullptr;

  Py_INCREF(&hist_pytype);
  if ( PyModule_AddObject(
    m, "histogram", reinterpret_cast<PyObject*>(&hist_pytype) ) < 0
  ) {
    Py_DECREF(&hist_pytype);
    Py_DECREF(m);
    return nullptr;
  }

  return m;
}

