#include <memory>
#include <string_view>
#include <sstream>

#include <ivanp/hist/histograms.hh>
#include <python.hh>

#define STR1(x) #x
#define STR(x) STR1(x)

#ifndef NDEBUG
#include <iostream>
#define TEST(var) std::cout << \
  "\033[33m" STR(__LINE__) ": " \
  "\033[36m" #var ":\033[0m " << (var) << std::endl;
#else
#define TEST(var)
#endif

using namespace std::string_literals;

namespace ivanp::hist {
namespace {
using namespace ivanp::python;

PyObject* unpack_call(PyObject* callable, PyObject* args) noexcept {
  const bool convert = args && !PyTuple_Check(args);
  if (convert) {
    auto iter = get_iter(args);
    if (!iter) return NULL;

    Py_ssize_t pos = 0, size = 4;

    // https://docs.python.org/3/c-api/tuple.html
    args = PyTuple_New(size);

    for (; ; ++pos) {
      PyObject* arg = PyIter_Next(iter);
      if (!arg) break;

      if (pos==size) _PyTuple_Resize(&args, size*=2);

      PyTuple_SET_ITEM(args,pos,arg);
    }
    if (pos!=size) _PyTuple_Resize(&args, pos);
  }
  // https://docs.python.org/3/c-api/object.html#c.PyObject_CallObject
  PyObject* obj = PyObject_CallObject(callable,args);
  if (convert) Py_DECREF(args);
  return obj;
}

using edge_type = double;

struct py_axis {
  PyObject_HEAD

  using base_type = poly_axis_base<edge_type>;
  using uniform_axis_type = uniform_axis<edge_type,true>;
  using list_axis_type = list_axis<std::vector<edge_type>,true>;

  std::unique_ptr<base_type> axis;

  [[noreturn]] static void uniform_args_error() {
    throw error(PyExc_ValueError,
      "uniform axis arguments must be of the form"
      " [nbins,min,max] or [nbins,min,max,\"log\"]");
  }

  py_axis(PyObject* args, PyObject* kwargs) {
    std::vector<edge_type> edges;
    index_type nbins = 0;
    edge_type min, max;

    auto iter = get_iter(args);
    if (!iter) throw existing_error{};

    auto arg = get_next(iter);
    if (!arg) throw error(PyExc_TypeError,
      "empty list of axis arguments");
    for (;;) { // loop over arguments
      if (auto subiter = get_iter(arg)) { // uniform chunk
        // strings are also iterable and will enter here

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
      if (nbins && (arg || !edges.empty())) {
        const edge_type d = (max - min)/nbins;
        edges.reserve(edges.size()+nbins+1);
        for (index_type i=0; i<=nbins; ++i)
          edges.push_back(min + i*d);
        nbins = 0;
      }
      if (!arg) break;
    }

    // make the axis
    if (edges.empty()) {
      axis = std::make_unique<uniform_axis_type>(nbins, min, max);
    } else {
      std::sort( begin(edges), end(edges) );
      edges.erase( std::unique( begin(edges), end(edges) ), end(edges) );
      axis = std::make_unique<list_axis_type>(std::move(edges));
    }
  }

  base_type* operator->() { return axis.get(); }
  const base_type* operator->() const { return axis.get(); }
  base_type& operator*() { return *axis; }
  const base_type& operator*() const { return *axis; }

  PyObject* operator()(PyObject* args, PyObject* kwargs) {
    auto iter = get_iter(args);
    if (!iter) throw existing_error{};

    auto arg = get_next(iter);
    if (!arg || get_next(iter)) throw error(PyExc_TypeError,
      "axis call expression takes exactly one argument");

    return py(axis->find_bin_index(unpy_check<edge_type>(arg)));
  }

  PyObject* str() noexcept {
    std::stringstream ss;
    const auto* ptr = &**this;
    if (const auto* axis = dynamic_cast< const uniform_axis_type* >(ptr)) {
      ss << "axis: { nbins: " << axis->nbins()
                 << ", min: " << axis->min()
                 << ", max: " << axis->max()
                 << " }";
    } else
    if (const auto* axis = dynamic_cast< const list_axis_type* >(ptr)) {
      ss << "axis: [ ";
      bool first = true;
      for (auto x : axis->edges()) {
        if (first) first = false;
        else ss << ", ";
        ss << x;
      }
      ss << " ]";
    }
    return py(ss.str());
  }
}; // end py_axis ---------------------------------------------------

PyMethodDef axis_methods[] {
  { "nbins", (PyCFunction) +[](py_axis* self, PyObject*) noexcept {
      return py((*self)->nbins());
    }, METH_NOARGS, "number of bins on the axis not counting overflow" },

  { "nedges", (PyCFunction) +[](py_axis* self, PyObject*) noexcept {
      return py((*self)->nedges());
    }, METH_NOARGS, "number of edges on the axis" },

  { "find_bin_index", (PyCFunction) +[](
      py_axis* self, PyObject* const* args, Py_ssize_t nargs
    ) noexcept -> PyObject* {
      try {
        if (nargs!=1) throw error(PyExc_TypeError,
          "axis.find_bin_index(x) takes exactly one argument");
        return py((*self)->find_bin_index(unpy_check<edge_type>(args[0])));
      } catch (...) {
        lipp();
        return nullptr;
      }
    }, METH_FASTCALL, "" },
  { }
};

struct axis_py_type: PyTypeObject {
  axis_py_type(): PyTypeObject{ PyVarObject_HEAD_INIT(nullptr, 0) } {
    tp_name = "histograms.axis";
    tp_basicsize = sizeof(py_axis);
    tp_dealloc = (::destructor) ivanp::python::tp_dealloc<py_axis>;
    tp_call = (::ternaryfunc) ivanp::python::tp_call<py_axis>;
    tp_str = (::reprfunc) ivanp::python::tp_str<py_axis>;
    tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    tp_doc = "axis object";
    tp_methods = axis_methods;
    tp_new = (::newfunc) ivanp::python::tp_new<py_axis>;
  }
} axis_py_type;

// end axis defs ====================================================

struct py_bin_filler {
  static PyObject* fill(py_ptr& bin, PyObject* args) {
    const auto tp = Py_TYPE(bin);
    if (tp == &PyFloat_Type) {
      reinterpret_cast<PyFloatObject*>(&*bin)->ob_fval
        += unpy_check<double>(args);
      return bin;
    }
    if (const auto nb = tp->tp_as_number) {
      if (auto iadd = nb->nb_inplace_add)
        return iadd(bin,args), bin;
      if (auto add = nb->nb_add)
        return bin = py_ptr(add(bin,args));
    }
    if (const auto sq = tp->tp_as_sequence) {
      if (auto iadd = sq->sq_inplace_concat)
        return iadd(bin,args), bin;
      if (auto add = sq->sq_concat)
        return bin = py_ptr(add(bin,args));
    }
    throw error(PyExc_TypeError,
      "cannot add "s+(Py_TYPE(args)->tp_name)+" to "+(tp->tp_name));
  }
  static PyObject* fill(py_ptr& bin) {
    return fill(bin,py<int>(1));
  }
};

struct py_hist {
  PyObject_HEAD

  struct axis_ptr: py_ptr {
    using py_ptr::py_ptr;
    const py_axis::base_type& operator*() const noexcept {
      return **reinterpret_cast<py_axis*>(p);
    }
    const py_axis::base_type* operator->() const noexcept {
      return &**this;
    }
  };
  // TODO: reduce indirection
  // maybe construct py_hist subtypes for some inbuilt bin types

  using hist = histogram<
    py_ptr,
    std::vector<axis_ptr>,
    filler_spec<py_bin_filler>
  >;
  hist h;

  py_hist(PyObject* args, PyObject* kwargs)
  : h([](PyObject* args) {
      hist::axes_type axes;

      auto iter = get_iter(args);
      if (!iter) throw existing_error{};

      auto arg = get_next(iter);
      if (!arg) throw error(PyExc_TypeError,
        "empty list of histogram arguments");
      for (;;) { // loop over arguments
        TEST(unpy<std::string_view>(Py_TYPE(arg)->tp_str(arg)))
        PyObject* ax = unpack_call(
          reinterpret_cast<PyObject*>(&axis_py_type), arg);
        if (!ax) throw existing_error{};
        axes.emplace_back(ax);

        arg = get_next(iter);
        if (!arg) break;
      }

      return axes;
    }(args))
  {
    PyObject* bintype = nullptr;
    if (kwargs) {
      bintype = PyDict_GetItemString(kwargs,"bintype");
      TEST(bintype->ob_type->tp_name)
      TEST(reinterpret_cast<PyTypeObject*>(bintype)->tp_name)
      TEST(PyType_Check(bintype))
      if (bintype && !PyType_Check(bintype)) throw error(PyExc_TypeError,
        "histogram bintype argument must be a type");
    }
    if (bintype) {
      for (auto& bin : h.bins()) {
        bin = py_ptr(PyObject_CallObject(bintype,nullptr));
        if (!bin) throw existing_error{};
      }
    } else {
      for (auto& bin : h.bins())
        bin = py_ptr(py<double>(0.));
    }
  }

  PyObject* operator()(PyObject* args, PyObject* kwargs) {
    // auto iter = get_iter(args);
    // auto arg1 = get_next(iter);
    // if (!arg1)
    PyObject* bin = h.fill_at(0);

    Py_INCREF(bin);
    return bin;

    // auto arg2 = get_next(iter);
    // if (!arg1) return h(arg2);
    //
    // } else { // single edge
    //   PyErr_Clear(); // https://stackoverflow.com/q/60471914/2640636
    //   h();
    // }
    // Py_RETURN_NONE;
  }

  PyObject* operator[](PyObject* args) {
    PyObject* bin;
    if (auto iter = get_iter(args)) {
      auto arg = get_next(iter);
      bin = h[unpy_check<index_type>(arg)];
    } else { // single edge
      PyErr_Clear(); // https://stackoverflow.com/q/60471914/2640636
      bin = h[unpy_check<index_type>(args)];
    }
    // const index_type ui = (i < 0) ? (h.size()+i) : i;

    // TODO: wrap python iterables as c++ containers to use map()

    // TODO: multiple indices
    Py_INCREF(bin);
    return bin;
  }
}; // end py_hist ---------------------------------------------------

PyMethodDef hist_methods[] {
  { "size", (PyCFunction) +[](py_hist* self, PyObject*) noexcept {
      return py(self->h.size());
    }, METH_NOARGS, "total number of histogram bins, including overflow" },
  { "bintype", (PyCFunction) +[](py_hist* self, PyObject*) noexcept {
      return self->h.bins().front()->ob_type;
    }, METH_NOARGS, "type use for bin values" },
  { }
};

struct hist_mp_methods: PyMappingMethods {
  hist_mp_methods(): PyMappingMethods{ } {
    mp_length = (::lenfunc) +[](py_hist* self) noexcept -> Py_ssize_t {
      return self->h.size();
    };
    mp_subscript = (::binaryfunc) ivanp::python::mp_subscript<py_hist>;
  }
} hist_mp_methods;

struct hist_py_type: PyTypeObject {
  hist_py_type(): PyTypeObject{ PyVarObject_HEAD_INIT(nullptr, 0) } {
    tp_name = "histograms.histogram";
    tp_basicsize = sizeof(py_hist);
    tp_dealloc = (::destructor) ivanp::python::tp_dealloc<py_hist>;
    tp_as_mapping = &hist_mp_methods;
    tp_call = (::ternaryfunc) ivanp::python::tp_call<py_hist>;
    // tp_str = (::reprfunc) ivanp::python::tp_str<py_hist>;
    tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    tp_doc = "histogram object";
    tp_methods = hist_methods;
    tp_new = (::newfunc) ivanp::python::tp_new<py_hist>;
  }
} hist_py_type;

// end hist defs ====================================================

/*
PyMethodDef methods[] {
  { "histogram", make, METH_VARARGS | METH_KEYWORDS,
    "initializes a histogram" },
  { } // sentinel
};
*/

struct py_module: PyModuleDef {
  py_module(): PyModuleDef{ PyModuleDef_HEAD_INIT } {
    m_name = "histograms";
    m_doc = "Python bindings for the histograms library";
    m_size = -1;
  }
} py_module;

// TODO: constructor calls for static instances
// https://docs.python.org/3/extending/extending.html#thin-ice
// https://stackoverflow.com/q/62464225/2640636

} // end anonymous namespace
} // end histograms namespace

PyMODINIT_FUNC PyInit_histograms() {
  struct {
    PyTypeObject* type;
    const char* name;
  } static constexpr py_types[] {
    { &ivanp::hist::hist_py_type, "histogram" },
    { &ivanp::hist::axis_py_type, "axis" }
  };

  for (auto [type, name] : py_types)
    if (PyType_Ready(type) < 0) return nullptr;

  PyObject* m = PyModule_Create(&ivanp::hist::py_module);
  if (!m) return nullptr;

  unsigned n = 0;
  for (auto [type, name] : py_types) {
    Py_INCREF(type);
    ++n;
    if (PyModule_AddObject(m, name, reinterpret_cast<PyObject*>(type)) < 0) {
      do {
        Py_DECREF(py_types[--n].type);
      } while (n);
      Py_DECREF(m);
      return nullptr;
    }
  }

  return m;
}

