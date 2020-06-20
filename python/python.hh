#ifndef IVANP_PYTHON_CPP_HH
#define IVANP_PYTHON_CPP_HH

#include <stdexcept>
#include <type_traits>
#include <ivanp/concepts.hh>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

namespace ivanp::python {

class existing_error { };

class error: public std::runtime_error {
  PyObject* _type;
public:
  template <typename T>
  error(PyObject* type, const T& arg): runtime_error(arg), _type(type) { }
  PyObject* type() const { return _type; }
};

void lipp() noexcept { // Lippincott function
  // https://youtu.be/-amJL3AyADI
  // https://docs.python.org/3/c-api/exceptions.html#standard-exceptions
  try {
    throw;
  } catch (const existing_error&) {
  } catch (const error& e) {
    PyErr_SetString(e.type(),e.what());
  } catch (const std::exception& e) {
    PyErr_SetString(PyExc_RuntimeError,e.what());
  } catch (...) {
    PyErr_SetString(PyExc_RuntimeError,"");
  }
}

template <typename T>
concept stringlike = requires (T& x) {
  { x.data() } -> convertible_to<const char*>;
  { x.size() } -> convertible_to<size_t>;
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
  if constexpr (stringlike<const T>) {
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
  if constexpr (stringlike<const T>) {
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
  if constexpr (stringlike<T>) {
    if (PyUnicode_READY(p)!=0)
      throw error(PyExc_ValueError,"could not read python object as a string");
    return unpy<T>(p);
  }
}

template <typename T>
void unpy(T& x, PyObject* p) noexcept { x = unpy<T>(p); }
template <typename T>
void unpy_check(T& x, PyObject* p) { x = unpy_check<T>(p); }

class py_ptr {
protected:
  PyObject* p = nullptr;
public:
  py_ptr() noexcept = default;
  explicit py_ptr(PyObject* p) noexcept: p(p) {
    // should not increment reference count here
    // because it is usually already incremented by python
  }
  py_ptr(const py_ptr& o): p(o.p) {
    Py_XINCREF(p);
  }
  py_ptr& operator=(const py_ptr& o) {
    Py_XINCREF(o.p);
    Py_XDECREF(p);
    p = o.p;
    return *this;
  }
  py_ptr(py_ptr&& o) noexcept {
    std::swap(p,o.p);
  }
  py_ptr& operator=(py_ptr&& o) noexcept {
    std::swap(p,o.p);
    return *this;
  }
  ~py_ptr() { Py_XDECREF(p); }

  operator PyObject*() const noexcept { return p; }

  PyObject* operator->() noexcept { return p; }
  const PyObject* operator->() const noexcept { return p; }
  PyObject& operator*() noexcept { return *p; }
  const PyObject& operator*() const noexcept { return *p; }
};

py_ptr get_iter(PyObject* obj) { return py_ptr(PyObject_GetIter(obj)); }
py_ptr get_next(PyObject* iter) { return py_ptr(PyIter_Next(iter)); }

// Type methods =====================================================

template <typename T>
void tp_dealloc(T* self) noexcept {
  self->~T();
  Py_TYPE(self)->tp_free(self);
}

template <typename T>
PyObject* tp_new(PyTypeObject* subtype, PyObject* args, PyObject* kwargs)
noexcept {
  PyObject* obj = subtype->tp_alloc(subtype,0);
  if constexpr (std::is_nothrow_constructible_v<T,PyObject*,PyObject*>) {
    new(reinterpret_cast<T*>(obj)) T(args,kwargs);
    return obj;
  } else {
    try {
      new(reinterpret_cast<T*>(obj)) T(args,kwargs);
      return obj;
    } catch (...) { lipp(); return nullptr; }
  }
}

template <typename T>
PyObject* tp_call(T* self, PyObject* args, PyObject* kwargs) noexcept {
  if constexpr (noexcept((*self)(args,kwargs))) {
    return (*self)(args,kwargs);
  } else {
    try {
      return (*self)(args,kwargs);
    } catch (...) { lipp(); return nullptr; }
  }
}

template <typename T>
PyObject* tp_str(T* self) noexcept {
  if constexpr (noexcept(self->str())) {
    return self->str();
  } else {
    try {
      return self->str();
    } catch (...) { lipp(); return nullptr; }
  }
}

template <typename T>
PyObject* sq_item(T* self, Py_ssize_t i) noexcept {
  if constexpr (noexcept((*self)[i])) {
    return (*self)[i];
  } else {
    try {
      return (*self)[i];
    } catch (...) { lipp(); return nullptr; }
  }
}

template <typename T>
PyObject* mp_subscript(T* self, PyObject* args) noexcept {
  if constexpr (noexcept((*self)[args])) {
    return (*self)[args];
  } else {
    try {
      return (*self)[args];
    } catch (...) { lipp(); return nullptr; }
  }
}

} // end namespace

#endif
