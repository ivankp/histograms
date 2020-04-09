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
  // https://docs.python.org/3/c-api/exceptions.html#standard-exceptions
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

template <typename U>
class is_string_like {
  template <typename, typename = void>
  struct impl : std::false_type { };
  template <typename T>
  struct impl<T, decltype((void)(
    std::declval<T&>().data(), std::declval<T&>().size()
  ))> : std::is_convertible<
    decltype(std::declval<T&>().data()), const char*
  > { };
public:
  using type = impl<U>;
  static constexpr bool value = type::value;
};

template <typename T>
[[nodiscard]] PyObject* py(const T& x) noexcept {
  if constexpr (std::is_floating_point_v<T>) {
    return PyFloat_FromDouble(x);
  } else
  if constexpr (std::is_same_v<T,int> || std::is_same_v<T,long>) {
    return PyLong_FromLong(x);
  } else
  if constexpr (std::is_same_v<T,unsigned> || std::is_same_v<T,unsigned long>) {
    return PyLong_FromUnsignedLong(x);
  } else
  if constexpr (std::is_same_v<T,long long>) {
    return PyLong_FromLongLong(x);
  } else
  if constexpr (std::is_same_v<T,unsigned long long>) {
    return PyLong_FromUnsignedLongLong(x);
  } else
  if constexpr (is_string_like<T>::value) {
    return PyUnicode_FromStringAndSize(x.data(),x.size());
  }
}

template <typename T>
T unpy(PyObject* p) noexcept {
  if constexpr (std::is_floating_point_v<T>) {
    return PyFloat_AsDouble(p);
  } else
  if constexpr (std::is_same_v<T,int> || std::is_same_v<T,long>) {
    return PyLong_AsLong(p);
  } else
  if constexpr (std::is_same_v<T,unsigned> || std::is_same_v<T,unsigned long>) {
    return PyLong_AsUnsignedLong(p);
  } else
  if constexpr (std::is_same_v<T,long long>) {
    return PyLong_AsLongLong(p);
  } else
  if constexpr (std::is_same_v<T,unsigned long long>) {
    return PyLong_AsUnsignedLongLong(p);
  } else
  if constexpr (std::is_same_v<T,bool>) {
    return PyObject_IsTrue(p);
  } else
  if constexpr (is_string_like<T>::value) {
    // https://docs.python.org/3/c-api/unicode.html
    const size_t len = PyUnicode_GET_DATA_SIZE(p);
    if (len==0) return { };
    return { PyUnicode_AS_DATA(p), len };
  }
}

template <typename T>
T unpy_check(PyObject* p) {
  if constexpr (std::is_arithmetic_v<T>) {
    if constexpr (std::is_same_v<T,bool>) {
      const auto x = PyObject_IsTrue(p);
      if (x==(decltype(x))-1 && PyErr_Occurred()) throw existing_error{};
      return x;
    } else {
      const auto x = unpy<T>(p);
      if (x==(decltype(x))-1 && PyErr_Occurred()) throw existing_error{};
      return x;
    }
  } else
  if constexpr (is_string_like<T>::value) {
    if (PyUnicode_READY(p)!=0)
      throw error(PyExc_ValueError,"could not read python object as a string");
    return unpy<T>(p);
  }
}

template <typename T>
void unpy(T& x, PyObject* p) noexcept { x = unpy<T>(p); }
template <typename T>
void unpy_check(T& x, PyObject* p) { x = unpy_check<T>(p); }

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

  // T* get() noexcept { return p; }
  // const T* get() const noexcept { return p; }
  // T* operator->() noexcept { return p; }
  // const T* operator->() const noexcept { return p; }
  // T& operator*() noexcept { return *p; }
  // const T& operator*() const noexcept { return *p; }

  operator T*() const noexcept { return p; }
};

using py_ptr = my_py_ptr<PyObject>;

template <typename T>
void destroy(T* x) noexcept { x->~T(); }

template <typename T>
void dealloc(T* self) noexcept {
  static_assert(alignof(T)==alignof(PyObject));
  // call destructor
  destroy(self);
  // free memory
  Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

template <typename T>
int init(T* mem, PyObject* args, PyObject* kwargs) noexcept {
  try {
    new(mem) T(args,kwargs);
  } catch (...) {
    lipp();
    return 1;
  }
  return 0;
}

template <typename T>
PyObject* call(T* self, PyObject* args, PyObject* kwargs) noexcept {
  return (*self)(args,kwargs);
}

py_ptr get_iter(PyObject* obj) { return py_ptr(PyObject_GetIter(obj)); }
py_ptr get_next(PyObject* iter) { return py_ptr(PyIter_Next(iter)); }

namespace axis {

[[noreturn]] void uniform_args_error() {
  throw error(PyExc_ValueError,
    "uniform axis arguments must have the form"
    "[nbins,min,max] or [nbins,min,max,\"log\"]");
}

using edge_type = double;

struct py_axis {
  PyObject_HEAD

  using type = poly_axis_base<edge_type>;
  std::unique_ptr<type> axis;

  py_axis(PyObject* args, PyObject* kwargs) {
    std::vector<edge_type> edges;
    index_type nbins = 0;
    edge_type min, max;

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
        unpy_check(nbins,subarg);

        subarg = get_next(subiter);
        if (!subarg) uniform_args_error();
        unpy_check(min,subarg);

        subarg = get_next(subiter);
        if (!subarg) uniform_args_error();
        unpy_check(max,subarg);

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
        edges.push_back(unpy_check<edge_type>(arg));
      }

      // set up for next iteration
      arg = get_next(iter);
      if (arg) {
        if (nbins) {
          const edge_type d = (max - min)/nbins;
          edges.reserve(edges.size()+nbins+1);
          for (index_type i=0; i<=nbins; ++i)
            edges.push_back(min + i*d);
          nbins = 0;
        }
      } else break;
    }

    // make the axis
    if (edges.empty()) {
      axis = std::make_unique<uniform_axis<edge_type,true>>(
        nbins, min, max
      );
    } else {
      // need to sort and remove repetitions
      axis = std::make_unique<container_axis<std::vector<edge_type>,true>>(
        std::move(edges)
      );
    }
  }

  type* operator->() { return axis.get(); }
  const type* operator->() const { return axis.get(); }
  type& operator*() { return *axis; }
  const type& operator*() const { return *axis; }

  PyObject* operator()(PyObject* args, PyObject* kwargs) noexcept {
    Py_RETURN_NONE;
  }
};

PyMethodDef methods[] = {
  { "nbins", reinterpret_cast<PyCFunction>(
    +[](py_axis* self, PyObject* Py_UNUSED(ignored)) noexcept {
      return py((*self)->nbins());
    }), METH_NOARGS, "number of bins on the axis not counting overflow" },

  { "nedges", reinterpret_cast<PyCFunction>(
    +[](py_axis* self, PyObject* Py_UNUSED(ignored)) noexcept {
      return py((*self)->nedges());
    }), METH_NOARGS, "number of edges on the axis" },

  { "find_bin_index", reinterpret_cast<PyCFunction>(+[](
      py_axis* self, PyObject* const* args, Py_ssize_t nargs
    ) noexcept -> PyObject* {
      if (nargs!=1) {
        PyErr_SetString(PyExc_TypeError,
          "find_bin_index(x) takes exactly one argument");
        return nullptr;
      }
      return py((*self)->find_bin_index(unpy_check<edge_type>(args[0])));
    }), METH_FASTCALL, "" },

  { }
};

PyTypeObject py_type = {
  PyVarObject_HEAD_INIT(nullptr, 0)
  .tp_name = "histograms.axis",
  .tp_basicsize = sizeof(py_axis),
  .tp_itemsize = 0,
  .tp_dealloc = reinterpret_cast<::destructor>( dealloc<py_axis> ),
  .tp_call = reinterpret_cast<::ternaryfunc>( call<py_axis> ),
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "axis object",
  .tp_methods = methods,
  .tp_members = nullptr,
  .tp_init = reinterpret_cast<::initproc>( init<py_axis> ),
  .tp_new = PyType_GenericNew,
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

  py_hist(PyObject* args, PyObject* kwargs) {
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

  PyObject* operator()(PyObject* args, PyObject* kwargs) noexcept {
    Py_RETURN_NONE;
  }
};

PyMethodDef methods[] = {
  { "size", reinterpret_cast<PyCFunction>(
    +[](py_hist* self, PyObject* Py_UNUSED(ignored)) noexcept {
      return py(self->h.size());
    }), METH_NOARGS, "total number of histogram bins, including overflow" },

  { }
};

PySequenceMethods sq_methods = {
  .sq_length = reinterpret_cast<::lenfunc>(
    +[](py_hist* h) noexcept -> Py_ssize_t {
      return h->h.size();
    }),
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
  .tp_dealloc = reinterpret_cast<::destructor>( dealloc<py_hist> ),
  .tp_as_sequence = &sq_methods,
  .tp_call = reinterpret_cast<::ternaryfunc>( call<py_hist> ),
  .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
  .tp_doc = "histogram object",
  .tp_methods = methods,
  .tp_members = nullptr,
  .tp_init = reinterpret_cast<::initproc>( init<py_hist> ),
  .tp_new = PyType_GenericNew,
};

} // end hist namespace

/*
PyMethodDef methods[] = {
  { "histogram", make, METH_VARARGS | METH_KEYWORDS,
    "initializes a histogram" },
  { } // sentinel
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
  struct {
    PyTypeObject* ptr;
    const char* name;
  } static constexpr py_types[] {
    { &histograms::hist::py_type, "histogram" },
    { &histograms::axis::py_type, "axis" }
  };

  for (const auto& py_type : py_types) {
    if (PyType_Ready(py_type.ptr) < 0) return nullptr;
  }

  PyObject* m = PyModule_Create(&histograms::py_module);
  if (!m) return nullptr;

  unsigned n = 0;
  for (const auto& py_type : py_types) {
    Py_INCREF(py_type.ptr);
    ++n;
    if ( PyModule_AddObject(
      m, py_type.name,
      reinterpret_cast<PyObject*>(py_type.ptr) ) < 0
    ) {
      do {
        Py_DECREF(py_types[--n].ptr);
      } while (n);
      Py_DECREF(m);
      return nullptr;
    }
  }

  return m;
}

