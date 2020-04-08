#include <string_view>
#include <stdexcept>
#include <memory>
#include <variant>
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

void lipp() noexcept { // https://youtu.be/-amJL3AyADI
  // https://docs.python.org/3/c-api/exceptions.html
  try {
    throw;
  } catch (const existing_error&) {
  } catch (const error& e) {
    PyErr_SetString(e.type(),e.what());
  } catch (const std::exception& e) {
    PyErr_SetString(PyExc_RuntimeError,e.what());
  } catch (...) {
    PyErr_SetString(PyExc_RuntimeError,"unknown error");
  }
}

[[maybe_unused]] PyObject* py(double x) { return PyFloat_FromDouble(x); }
[[maybe_unused]] PyObject* py(long   x) { return PyLong_FromLong(x); }
[[maybe_unused]] PyObject* py(unsigned long x) { return PyLong_FromUnsignedLong(x); }
[[maybe_unused]] PyObject* py(long long x) { return PyLong_FromLongLong(x); }
[[maybe_unused]] PyObject* py(unsigned long long x) {
  return PyLong_FromUnsignedLongLong(x);
}
[[maybe_unused]] PyObject* py(std::string_view x) {
  // return PyBytes_FromStringAndSize(x.data(),x.size());
  return PyUnicode_FromStringAndSize(x.data(),x.size());
}
[[maybe_unused]] PyObject* py(int x) { return py((long)x); }
[[maybe_unused]] PyObject* py(unsigned x) { return py((unsigned long)x); }

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

  bool operator!() const noexcept { return !p; }
  operator bool() const noexcept { return p; }
};

using py_ptr = my_py_ptr<PyObject>;

template <typename T>
T unpy(py_ptr p) noexcept {
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
  } else
  if constexpr (std::is_same_v<T,std::string_view>) {
    // https://docs.python.org/3/c-api/unicode.html
    const size_t len = PyUnicode_GET_DATA_SIZE(p.get());
    if (len==0) return { };
    return { PyUnicode_AS_DATA(p.get()), len };
  } else
  if constexpr (std::is_same_v<T,int>) {
    return unpy<long>(p);
  } else
  if constexpr (std::is_same_v<T,unsigned>) {
    return unpy<unsigned long>(p);
  } else
  if constexpr (std::is_same_v<T,float>) {
    return unpy<double>(p);
  }
}

template <typename T>
T unpy_check(py_ptr p) {
  const auto x = unpy<T>(p);
  if (x==(decltype(x))-1 && PyErr_Occurred()) throw existing_error{};
  return x;
}

template <>
std::string_view unpy_check<std::string_view>(py_ptr p) {
  if (PyUnicode_READY(p.get())!=0)
    throw error(PyExc_ValueError,"could not read python object as a string");
  return unpy<std::string_view>(p);
}

template <typename T>
T& unpy_to(T& x, py_ptr p) { return x = unpy<T>(p); }
template <typename T>
T& unpy_to_check(T& x, py_ptr p) { return x = unpy_check<T>(p); }

template <typename T>
void destroy(T* x) noexcept { x->~T(); }

template <typename T>
void dealloc(T *self) noexcept {
  static_assert(alignof(T)==alignof(PyObject));
  // call destructor
  destroy(self);
  // free memory
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

template <typename T>
int init(T *mem, PyObject *args, PyObject *kwargs) noexcept {
  try {
    new(mem) T(args,kwargs);
  } catch (...) {
    lipp();
    return 1;
  }
  return 0;
}

py_ptr get_iter(PyObject *obj) { return py_ptr(PyObject_GetIter(obj)); }
py_ptr get_iter(py_ptr obj) { return get_iter(obj.get()); }

py_ptr get_next(PyObject *iter) { return py_ptr(PyIter_Next(iter)); }
py_ptr get_next(py_ptr iter) { return get_next(iter.get()); }

namespace axis {

[[noreturn]] void uniform_args_error() {
  throw error(PyExc_ValueError,
    "uniform axis arguments must have the form"
    "[nbins,min,max] or [nbins,min,max,\"log\"]");
}

struct py_axis {
  PyObject_HEAD

  using type = poly_axis_base<double>;
  std::unique_ptr<type> axis;

  py_axis(PyObject *args, PyObject *kwargs) {
    std::vector<double> edges;
    index_type nbins = 0;
    double min, max;

    auto iter = get_iter(args);
    if (!iter) throw existing_error{};

    auto arg = get_next(iter);
    if (!arg) throw error(PyExc_ValueError,
      "empty list of axis arguments");
    for (;;) { // loop over arguments
      auto subiter = get_iter(arg);
      if (subiter) { // uniform chunk

        auto subarg = get_next(subiter);
        if (!subarg) uniform_args_error();
        unpy_to_check(nbins,subarg);

        subarg = get_next(subiter);
        if (!subarg) uniform_args_error();
        unpy_to_check(min,subarg);

        subarg = get_next(subiter);
        if (!subarg) uniform_args_error();
        unpy_to_check(max,subarg);

        subarg = get_next(subiter);
        if (subarg) { // flags
          const auto flag = unpy_check<std::string_view>(subarg);
          if (flag == "log") {
            // TODO: logarithmic spacing
          } else uniform_args_error();

          if (get_next(subiter)) // too many elements
            uniform_args_error();
        }

      } else { // single edge
        PyErr_Clear(); // https://stackoverflow.com/q/60471914/2640636
        edges.push_back(unpy_check<double>(arg));
      }

      // set up for next iteration
      arg = get_next(iter);
      if (arg) {
        if (nbins) {
          const double d = (max - min)/nbins;
          edges.reserve(edges.size()+nbins+1);
          for (index_type i=0; i<=nbins; ++i)
            edges.push_back(min + i*d);
          nbins = 0;
        }
      } else break;
    }

    // make the axis
    if (edges.empty()) {
      axis = std::make_unique<uniform_axis<double,true>>(
        nbins, min, max
      );
    } else {
      // need to sort and remove repetitions
      axis = std::make_unique<container_axis<std::vector<double>,true>>(
        std::move(edges)
      );
    }
  }

  type* operator->() { return axis.get(); }
  const type* operator->() const { return axis.get(); }
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
    // py_hist::type::axes_type axes;
    // union {
    //   long long ix;
    //   double dx;
    // };
    // std::vector<double> edges;
    // bool is_float;
    // unsigned k = 0;
    // for (py_ptr iter(PyObject_GetIter(args)), arg; (arg = next(iter)); ++k) {
    // }
    //
    // auto& h = self->h;
    // new(&h) py_hist::type(std::move(axes));
    // for (auto& b : h.bins()) {
    //   b = nullptr;
    // }
  }
};

PyObject* size(py_hist *self, PyObject *Py_UNUSED(ignored)) {
  return py(self->h.size());
}

PyMethodDef methods[] = {
  { "size", (PyCFunction) size, METH_NOARGS,
    "total number of histogram bins, including overflow" },
  { nullptr }
};

Py_ssize_t lenfunc(py_hist *h) { return h->h.size(); }

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

