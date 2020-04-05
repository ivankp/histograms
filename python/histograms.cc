// #include <memory>
#include <string_view>
#include <stdexcept>
#include <histograms/histograms.hh>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

// #define STR1(x) #x
// #define STR(x) STR1(x)

// #define ERROR_PREF __FILE__ ":" STR(__LINE__) ": "

#include <iostream>
#define TEST(var) \
  std::cout << "\033[36m" #var "\033[0m = " << var << std::endl;

namespace histograms {
namespace {

class existing_error { };

class error: public std::runtime_error {
  PyObject* _type;
public:
  template <typename T>
  error(PyObject* type, const T& arg): runtime_error(arg), _type(type) { }
  PyObject* type() const { return _type; }
};

[[maybe_unused]] PyObject* py(double x) { return PyFloat_FromDouble(x); }
[[maybe_unused]] PyObject* py(float  x) { return PyFloat_FromDouble(x); }
[[maybe_unused]] PyObject* py(long   x) { return PyLong_FromLong(x); }
[[maybe_unused]] PyObject* py(long long x) { return PyLong_FromLongLong(x); }
[[maybe_unused]] PyObject* py(unsigned long x) { return PyLong_FromUnsignedLong(x); }
[[maybe_unused]] PyObject* py(unsigned long long x) {
  return PyLong_FromUnsignedLongLong(x);
}
// PyObject* py(size_t x) { return PyLong_FromSize_t(x); }
[[maybe_unused]] PyObject* py(std::string_view x) {
  // return PyBytes_FromStringAndSize(x.data(),x.size());
  return PyUnicode_FromStringAndSize(x.data(),x.size());
}

template <typename T>
class my_py_ptr {
  static_assert(alignof(T)==alignof(PyObject));
  // static_assert(
  //   std::disjunction<
  //     std::is_same<T,PyObject>,
  //     std::is_same<decltype(T::ob_base),PyObject>
  //   >::value);
  T* p;
  void incref() { Py_INCREF(reinterpret_cast<PyObject*>(p)); }
  void decref() { Py_DECREF(reinterpret_cast<PyObject*>(p)); }
public:
  my_py_ptr() noexcept: p(nullptr) { }
  explicit my_py_ptr(T* p) noexcept: p(p) {
    // should not increment reference count here
    // because it may have already been incremented
  }
  my_py_ptr(const my_py_ptr& o): p(o.p) { incref(); }
  my_py_ptr& operator=(const my_py_ptr& o) {
    if (p != o.p) {
      if (p) decref();
      p = o.p;
      incref();
    }
    return *this;
  }
  my_py_ptr(my_py_ptr&& o) noexcept: p(o.p) { o.p = nullptr; }
  my_py_ptr& operator=(my_py_ptr&& o) noexcept {
    p = o.p;
    o.p = nullptr;
    return *this;
  }
  ~my_py_ptr() { if (p) decref(); }

  T* get() noexcept { return p; }
  const T* get() const noexcept { return p; }
  T* operator->() noexcept { return p; }
  const T* operator->() const noexcept { return p; }
  T& operator*() noexcept { return *p; }
  const T& operator*() const noexcept { return *p; }
};

using py_ptr = my_py_ptr<PyObject>;

template <typename T>
T unpy(py_ptr p) {
  if constexpr (std::is_same_v<T,double>) {
    return PyFloat_AsDouble(p.get());
  } else
  if constexpr (std::is_same_v<T,long>) {
    return PyLong_AsLong(p.get());
  } else
  if constexpr (std::is_same_v<T,unsigned long>) {
    return PyLong_AsUnsignedLong(p.get());
  } else
  if constexpr (std::is_same_v<T,long long>) {
    return PyLong_AsLongLong(p.get());
  } else
  if constexpr (std::is_same_v<T,unsigned long long>) {
    return PyLong_AsUnsignedLongLong(p.get());
  }
}

template <typename T>
T unpy_check(py_ptr p) {
  const auto x = unpy<T>(p);
  if (x==(decltype(x))-1 && PyErr_Occurred()) throw existing_error{};
  return x;
}

py_ptr next(py_ptr iter) { return py_ptr(PyIter_Next(iter.get())); }

template <typename T>
void destroy(T* x) noexcept { x->~T(); }

template <typename T>
void dealloc(T *self) {
  static_assert(alignof(T)==alignof(PyObject));
  // call destructor
  destroy(self);
  // free memory
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

template <typename T>
int init(T *self, PyObject *args, PyObject *kwargs) {
  try {
    new(self) T(args,kwargs);
  } catch (const existing_error& e) {
    return 1;
  } catch (const error& e) {
    PyErr_SetString(e.type(),e.what());
    return 1;
  } catch (const std::exception& e) {
    PyErr_SetString(PyExc_RuntimeError,e.what());
    return 1;
  }
  return 0;
}

namespace axis {

struct py_axis {
  PyObject_HEAD
  using type = poly_axis_base<double>;
  type* axis = nullptr;

  py_axis(PyObject *args, PyObject *kwargs) {
    TEST(__PRETTY_FUNCTION__)
    // axis = new type();
  }
  ~py_axis() {
    TEST(__PRETTY_FUNCTION__)
    if (axis) delete axis;
  }

  type* operator->() { return axis; }
  const type* operator->() const { return axis; }
  type& operator*() { return *axis; }
  const type& operator*() const { return *axis; }
};

} // end axis namespace

namespace hist {

struct py_hist {
  PyObject_HEAD
  // using type = histogram<
  //   py_ptr,
  //   std::vector< std::shared_ptr<poly_axis_base<double>> >
  // >;
  using type = histogram< py_ptr, std::vector<my_py_ptr<axis::py_axis>> >;
  type h;

  py_hist(PyObject *args, PyObject *kwargs) {
    /*
    py_hist::type::axes_type axes;
    union {
      long long ix;
      double dx;
    };
    std::vector<double> edges;
    bool is_float;
    unsigned k = 0;
    for (py_ptr iter(PyObject_GetIter(args)), arg; (arg = next(iter)); ++k) {
      py_ptr iter2(PyObject_GetIter(arg.get()));
      if (!iter2) throw existing_error{};

      py_ptr arg2 = next(iter2);
      if (!arg2) throw error(PyExc_TypeError,
        "argument is iterable but empty");
      if (PyLong_Check(arg2.get())) {
        ix = unpy_check<decltype(ix)>(arg2);
        is_float = false;
      } else {
        dx = unpy_check<double>(arg2);
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
            edges.push_back(unpy_check<double>(arg3));
          }

          arg3 = next(iter3);
          if (arg3) throw error(PyExc_TypeError,
            "axis range must have exactly 2 values");

          axes.emplace_back(
            std::make_shared<uniform_axis<double,true>>(
              (is_float ? index_type(dx) : index_type(ix)), edges[0], edges[1]
          ));
        } else { // must be a container axis
          PyErr_Clear(); // https://stackoverflow.com/q/60471914/2640636
          edges.push_back(is_float ? dx : double(ix));
          if (arg2) {
            do edges.push_back(unpy_check<double>(arg2));
            while ((arg2 = next(iter2)));
          }

          axes.emplace_back(
            std::make_shared<container_axis<std::vector<double>,true>>(
              std::move(edges)
          ));
        }
      } else {
        const double x = (is_float ? dx : double(ix));
        axes.emplace_back(
          std::make_shared<uniform_axis<double,true>>(
            0, x, x
        ));
      }

      edges.clear();
    }

    auto& h = self->h;
    new(&h) py_hist::type(std::move(axes));
    for (auto& b : h.bins()) {
      b = nullptr;
    }
    */
  }
};

PyObject* nbins(py_hist *self, PyObject *Py_UNUSED(ignored)) {
  return py(self->h.nbins());
}

PyMethodDef methods[] = {
  { "nbins", (PyCFunction) nbins, METH_NOARGS,
    "total number of histogram bins, including overflow" },
  { nullptr }
};

Py_ssize_t lenfunc(py_hist *h) { return h->h.nbins(); }

PyObject* call(py_hist *self, PyObject *args, PyObject *kwargs) {
  printf("%s\n",__PRETTY_FUNCTION__);
  Py_RETURN_NONE;
}

PySequenceMethods sq_methods = {
  .sq_length = (::lenfunc) lenfunc,
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

PyTypeObject py_type = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  .tp_name = "histograms.histogram",
  .tp_basicsize = sizeof(py_hist),
  .tp_itemsize = 0,
  .tp_dealloc = (::destructor) dealloc<py_hist>,
  .tp_as_sequence = &sq_methods,
  .tp_call = (::ternaryfunc) call,
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "histogram object",
  .tp_methods = methods,
  .tp_members = nullptr,
  .tp_init = (::initproc) init<py_hist>,
  .tp_new = PyType_GenericNew,
};

} // end hist namespace

/*
PyMethodDef methods[] = {
  { "histogram", make, METH_VARARGS | METH_KEYWORDS,
    "initializes a histogram" },
  { nullptr, nullptr, 0, nullptr } // sentinel
};
*/

PyModuleDef py_module = {
  PyModuleDef_HEAD_INIT,
  .m_name = "histograms",
  .m_doc = "Python bindings for the histograms library",
  .m_size = -1,
  // methods
};

}
} // end histograms namespace

PyMODINIT_FUNC PyInit_histograms() {
  static constexpr std::array<
    std::tuple<PyTypeObject*,const char*>, 1
  > py_types {{
    { &histograms::hist::py_type, "histogram" }
  }};

  for (const auto& py_type : py_types) {
    if (PyType_Ready(std::get<0>(py_type)) < 0) return nullptr;
  }

  PyObject* m = PyModule_Create(&histograms::py_module);
  if (!m) return nullptr;

  unsigned n = 0;
  for (const auto& py_type : py_types) {
    Py_INCREF(std::get<0>(py_type));
    ++n;
    if ( PyModule_AddObject(
      m, std::get<1>(py_type),
      reinterpret_cast<PyObject*>(std::get<0>(py_type)) ) < 0
    ) {
      do {
        Py_DECREF(std::get<0>(py_types[--n]));
      } while (n);
      Py_DECREF(m);
      return nullptr;
    }
  }

  return m;
}

