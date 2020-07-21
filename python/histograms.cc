#include <string_view>
#include <sstream>

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

#include <ivanp/hist/histograms.hh>
#include <ivanp/hist/bins.hh>
#include <python.hh>

using namespace std::string_literals;
using namespace ivanp::map::operators;

// TODO: copy, deepcopy
// https://stackoverflow.com/q/62976099/2640636

namespace ivanp::hist {
namespace {
using namespace ivanp::python;

using edge_type = double;

static PyObject *sentinel_one, *double_zero;

struct py_axis {
  PyObject_HEAD // must not be overwritten in the constructor

  using base_type = poly_axis_base<edge_type>;
  using uniform_axis_type = uniform_axis<edge_type,true>;
  using list_axis_type = list_axis<std::vector<edge_type>,true>;

  std::aligned_union_t<0,uniform_axis_type,list_axis_type> buff;

  base_type& operator*() noexcept {
    return *reinterpret_cast<base_type*>(&buff);
  }
  const base_type& operator*() const noexcept {
    return *reinterpret_cast<const base_type*>(&buff);
  }
  base_type* operator->() noexcept { return &**this; }
  const base_type* operator->() const noexcept { return &**this; }

  [[noreturn]] static void uniform_args_error() {
    throw error(PyExc_ValueError,
      "uniform axis arguments must be of the form"
      " [nbins,min,max] or [nbins,min,max,\"flags\"]");
  }

  // TODO: allow axes as chuncks in constructor
  py_axis(PyObject* args, PyObject* kwargs) {
    std::vector<edge_type> edges;
    index_type nbins = 0;
    edge_type min, max;

    auto iter = get_iter(args); // guaranteed tuple
    auto arg = get_next(iter);
    if (!arg) throw error(PyExc_TypeError,
      "empty list of axis arguments");
    for (;;) { // loop over arguments
      if (auto subiter = get_iter(arg)) { // uniform chunk
        // strings are also iterable and will enter here

        auto subarg = get_next(subiter);
        if (!subarg) uniform_args_error();
        unpy_check_to(nbins,subarg);

        subarg = get_next(subiter);
        if (!subarg) uniform_args_error();
        unpy_check_to(min,subarg);

        subarg = get_next(subiter);
        if (!subarg) uniform_args_error();
        unpy_check_to(max,subarg);

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
      new (&**this) uniform_axis_type(nbins, min, max);
    } else {
      std::sort( begin(edges), end(edges) );
      edges.erase( std::unique( begin(edges), end(edges) ), end(edges) );
      new (&**this) list_axis_type(std::move(edges));
    }
  }

  ~py_axis() { (*this)->~base_type(); }

  PyObject* operator()(PyObject* args, PyObject* kwargs) const {
    auto iter = get_iter(args);
    if (!iter) throw existing_error{};

    auto arg = get_next(iter);
    if (!arg || get_next(iter)) throw error(PyExc_TypeError,
      "axis call expression takes exactly one argument");

    return py((*this)->find_bin_index(unpy_check<edge_type>(arg)));
  }

  PyObject* str() const noexcept {
    std::stringstream ss;
    if (auto* axis = dynamic_cast< const uniform_axis_type* >(&**this)) {
      ss << "axis: { nbins: " << axis->nbins()
                 << ", min: " << axis->min()
                 << ", max: " << axis->max()
                 << " }";
    } else
    if (auto* axis = dynamic_cast< const list_axis_type* >(&**this)) {
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

struct py_axis_iter {
  PyObject_HEAD // must not be overwritten in the constructor

  using base_type = typename py_axis::base_type;
  const base_type& axis;
  index_type i;

  py_axis_iter(PyObject* args, PyObject* kwargs) noexcept
  : py_axis_iter( **py_cast<py_axis>( tuple_items(args)[0] ) )
  { }

  py_axis_iter(base_type& axis) noexcept: axis(axis), i(0) { }
};

struct axis_iter_py_type: PyTypeObject {
  axis_iter_py_type(): PyTypeObject{ PyVarObject_HEAD_INIT(nullptr, 0) } {
    tp_name = "histograms.axis_iterator";
    tp_basicsize = sizeof(py_axis_iter);
    tp_new = (::newfunc) ivanp::python::tp_new<py_axis_iter>;
    tp_dealloc = (::destructor) ivanp::python::tp_dealloc<py_axis_iter>;
    tp_iternext = (::iternextfunc) +[](py_axis_iter* self) noexcept
      -> PyObject* {
        const auto& axis = self->axis;
        auto& i = self->i;
        if (i == axis.nedges()) return nullptr;
        return py(axis.edge(i++));
      };
  }
} axis_iter_py_type;

PyMethodDef axis_methods[] {
  { "nbins", (PyCFunction) +[](py_axis* self) noexcept {
      return py((*self)->nbins());
    }, METH_NOARGS, "number of axis bins not counting overflow" },
  { "nedges", (PyCFunction) +[](py_axis* self) noexcept {
      return py((*self)->nedges());
    }, METH_NOARGS, "number of axis edges" },
  { "find_bin_index", (PyCFunction) +[](py_axis* self, PyObject* arg) noexcept
    -> PyObject* {
      try {
        return py((*self)->find_bin_index(unpy_check<edge_type>(arg)));
      } catch (...) { lipp(); return nullptr; }
    }, METH_O, "find bin by coordinate" },
  { "edge", (PyCFunction) +[](py_axis* self, PyObject* arg) noexcept
    -> PyObject* {
      try {
        return py((*self)->edge(unpy_check<index_type>(arg)));
      } catch (...) { lipp(); return nullptr; }
    }, METH_O, "edge at index" },
  { "min", (PyCFunction) +[](py_axis* self) noexcept
    -> PyObject* {
      return py((*self)->min());
    }, METH_NOARGS, "axis minimum" },
  { "max", (PyCFunction) +[](py_axis* self) noexcept
    -> PyObject* {
      return py((*self)->max());
    }, METH_NOARGS, "axis maximum" },
  { "lower", (PyCFunction) +[](py_axis* self, PyObject* arg) noexcept
    -> PyObject* {
      try {
        return py((*self)->lower(unpy_check<index_type>(arg)));
      } catch (...) { lipp(); return nullptr; }
    }, METH_O, "lower bin edge" },
  { "upper", (PyCFunction) +[](py_axis* self, PyObject* arg) noexcept
    -> PyObject* {
      try {
        return py((*self)->upper(unpy_check<index_type>(arg)));
      } catch (...) { lipp(); return nullptr; }
    }, METH_O, "upper bin edge" },
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
    tp_iter = (::getiterfunc) +[](PyObject* self) noexcept {
      return PyObject_CallObject(
        py_cast<PyObject>(&axis_iter_py_type),
        +static_py_tuple(self)
      );
    };
  }
} axis_py_type;

// end axis defs ====================================================

struct py_bin_filler {
  static PyObject* fill(py_ptr& bin, PyObject* arg) {
    PyObject* sum = PyNumber_InPlaceAdd(bin,arg);
    if (!sum) [[unlikely]] throw existing_error{};
    if (sum != bin) bin = py_ptr(sum);
    return bin;
  }
};

extern struct hist_py_type hist_py_type;

struct py_hist {
  PyObject_HEAD

  struct axis_ptr: py_ptr {
    using py_ptr::py_ptr;
    const py_axis::base_type& operator*() const noexcept {
      return **py_cast<py_axis>(p);
    }
    const py_axis::base_type* operator->() const noexcept {
      return &**this;
    }
  };

  using hist = histogram<
    py_ptr,
    std::vector<axis_ptr>,
    filler_spec<py_bin_filler>
  >;
  hist h;

  py_hist(PyObject* args, PyObject* kwargs)
  : h([](PyObject* args_tuple) { // guaranteed tuple
      const auto* arg = tuple_items(args_tuple);
      const auto nargs = tuple_size(args_tuple);
      if (nargs==0) throw error(PyExc_TypeError,
        "empty list of histogram arguments");
      if (const auto* h_arg = py_dynamic_cast<py_hist>(*arg,&hist_py_type)) {
        auto& axes = h_arg->h.axes();
        for (auto& axis : axes)
          Py_INCREF(axis);
        return axes;
      } else {
        hist::axes_type axes;
        axes.reserve(nargs);
        for (auto* const end=arg+nargs; arg!=end; ++arg) {
          PyObject* ax = call_with_iterable(
            py_cast<PyObject>(&axis_py_type), *arg);
          if (!ax) throw existing_error{};
          TEST(as_str(ax))
          axes.emplace_back(ax);
        }
        return axes;
      }
    }(args))
  {
    PyObject* bintype = nullptr;
    if (kwargs) {
      bintype = PyDict_GetItemString(kwargs,"bintype");
      if (bintype && !PyType_Check(bintype)) throw error(PyExc_TypeError,
        "histogram bintype argument must be a type");
    }
    if (bintype) {
      for (auto& bin : h) {
        bin = py_ptr(PyObject_CallObject(bintype,nullptr));
        if (!bin) throw existing_error{};
      }
    } else {
      if (const auto arg = py_dynamic_cast<py_hist>(
        tuple_items(args)[0], &hist_py_type
      )) {
        map::map<map::flags::no_size_check>([](auto& to, auto& from){
          auto* const ob_type = from->ob_type;
          if (ob_type == &PyFloat_Type || ob_type == &PyLong_Type)
            Py_INCREF((to = from));
          else
            to = py_ptr(PyObject_CallObject(
              py_cast<PyObject>(ob_type), +static_py_tuple(+from) ));
        },h,arg->h);
      } else { // default to float-valued bins
        for (auto& bin : h)
          Py_INCREF((bin = py_ptr(double_zero)));
      }
    }
  }

  PyObject* operator()(PyObject* args, PyObject* kwargs) {
    auto iter = get_iter(args); // guaranteed tuple
    auto arg = get_next(iter);
    if (!arg) throw error(PyExc_ValueError, // no arguments
      "no arguments passed to histogram.fill()");

    PyObject* fill_arg;
    bool need_decref = false;
    if (auto iter2 = get_iter(arg)) { // first arg is iterable
      iter = iter2;
      arg = get_next(iter);
      const unsigned nargs = tuple_size(args) - 1;
      if (nargs==0) {
        fill_arg = sentinel_one;
      } else {
        auto arr1 = tuple_items(args);
        if (nargs==1) {
          fill_arg = arr1[1];
        } else {
          fill_arg = PyTuple_New(nargs);
          std::copy(arr1+1, arr1+1+nargs, tuple_items(fill_arg));
          for (PyObject** p=arr1+nargs; p!=arr1; --p)
            Py_INCREF(*p);
          need_decref = true;
        }
      }
    } else { // first arg not iterable
      PyErr_Clear();
      fill_arg = sentinel_one;
    }

    PyObject* bin = h([&]{
        const unsigned n = h.naxes();
        std::vector<edge_type> coords(n);
        unsigned i = 0;
        for (; arg; ++i) {
          if (i==n) throw error(PyExc_ValueError,
            "too many coordinates passed to histogram.fill()");
          coords[i] = unpy_check<edge_type>(arg);
          arg = get_next(iter);
        }
        if (i!=n) throw error(PyExc_ValueError,
          "too few coordinates passed to histogram.fill()");
        return coords;
      }(), fill_arg);
    Py_INCREF(bin);
    if (need_decref) Py_DECREF(fill_arg);
    return bin;
  }

  PyObject* operator[](PyObject* args) {
    PyObject* bin;
    if (auto iter = get_iter(args)) { // multiple args
      const unsigned n = h.naxes();
      unsigned i = 0;
      std::vector<index_type> ii(n);
      auto arg = get_next(iter);
      while (arg) {
        if (i==n) throw error(PyExc_ValueError,
          "too many argument indices");
        ii[i] = unpy_check<index_type>(arg);
        arg = get_next(iter);
        ++i;
      }
      if (i!=n) throw error(PyExc_ValueError,
        "too few argument indices");
      bin = h[ii];
    } else { // single arg
      PyErr_Clear();
      bin = h[unpy_check<index_type>(args)];
    }

    Py_INCREF(bin);
    return bin;
  }
}; // end py_hist ---------------------------------------------------

struct py_hist_iter {
  PyObject_HEAD // must not be overwritten in the constructor

  decltype(std::declval<py_hist::hist&>().begin()) it;
  decltype(std::declval<py_hist::hist&>().end  ()) end;

  py_hist_iter(PyObject* args, PyObject* kwargs) noexcept
  : py_hist_iter( py_cast<py_hist>( tuple_items(args)[0] )->h )
  { }

  py_hist_iter(py_hist::hist& h) noexcept: it(h.begin()), end(h.end()) { }
};

struct hist_iter_py_type: PyTypeObject {
  hist_iter_py_type(): PyTypeObject{ PyVarObject_HEAD_INIT(nullptr, 0) } {
    tp_name = "histograms.histogram_iterator";
    tp_basicsize = sizeof(py_hist_iter);
    tp_new = (::newfunc) ivanp::python::tp_new<py_hist_iter>;
    tp_dealloc = (::destructor) ivanp::python::tp_dealloc<py_hist_iter>;
    tp_iternext = (::iternextfunc) +[](py_hist_iter* self) noexcept
      -> PyObject* {
        auto& it = self->it;
        if (it == self->end) return nullptr;
        PyObject* bin = *it;
        ++it;
        Py_INCREF(bin);
        return bin;
      };
  }
} hist_iter_py_type;

PyMethodDef hist_methods[] {
  { "fill", (PyCFunction) ivanp::python::tp_call<py_hist>,
    METH_VARARGS, "fill histogram bin at given coordinates" },
  { "axes", (PyCFunction) +[](py_hist* self) noexcept {
      const auto& axes = self->h.axes();
      PyObject* tup = PyTuple_New(axes.size());
      std::copy(axes.begin(), axes.end(), tuple_items(tup));
      for (PyObject* x : axes) Py_INCREF(x);
      return tup;
    }, METH_NOARGS, "tuple of axes" },
  { "axis", (PyCFunction) +[](py_hist* self, PyObject* arg) noexcept
    -> PyObject* {
      try {
        const auto i = unpy_check<int>(arg);
        const auto& axes = self->h.axes();
        PyObject* axis = axes.at(i<0 ? axes.size()+i : i);
        Py_INCREF(axis);
        return axis;
      } catch(...) { lipp(); return nullptr; }
    }, METH_O, "axis at given index" },
  { "naxes", (PyCFunction) +[](py_hist* self) noexcept {
      return py(self->h.naxes());
    }, METH_NOARGS, "number of axes" },
  { "size", (PyCFunction) +[](py_hist* self) noexcept {
      return py(self->h.size());
    }, METH_NOARGS, "total number of bins, including overflow" },
  { "join_index", (PyCFunction) +[](py_hist* self, PyObject* args) noexcept
    -> PyObject* {
      try {
        return py( self->h.join_index( map_py_args(
          args, unpy_check<index_type>, self->h.naxes()
        )));
      } catch(...) { lipp(); return nullptr; }
    }, METH_VARARGS, "compute global bin index" },
  { "bin_at", (PyCFunction) +[](py_hist* self, PyObject* args) noexcept
    -> PyObject* {
      try {
        return self->h.bin_at( map_py_args(
          args, unpy_check<index_type>, self->h.naxes()
        ));
      } catch(...) { lipp(); return nullptr; }
    }, METH_VARARGS, "bin at given indices" },
  { "find_bin_index", (PyCFunction) +[](py_hist* self, PyObject* args) noexcept
    -> PyObject* {
      try {
        return py( self->h.find_bin_index( map_py_args(
          args, unpy_check<edge_type>, self->h.naxes()
        )));
      } catch(...) { lipp(); return nullptr; }
    }, METH_VARARGS, "bin index from coordinates" },
  { "find_bin", (PyCFunction) +[](py_hist* self, PyObject* args) noexcept
    -> PyObject* {
      try {
        return self->h.find_bin( map_py_args(
          args, unpy_check<edge_type>, self->h.naxes()
        ));
      } catch(...) { lipp(); return nullptr; }
    }, METH_VARARGS, "bin at given coordinates" },
  { "fill_at", (PyCFunction) +[](py_hist* self, PyObject* args) noexcept
    -> PyObject* {
      try {
        auto iter = get_iter(args); // guaranteed tuple
        auto arg = get_next(iter);
        if (!arg) throw error(PyExc_ValueError, // no arguments
          "no arguments passed to histogram.fill_at()");

        PyObject* fill_arg;
        bool need_decref = false;
        if (auto iter2 = get_iter(arg)) { // first arg is iterable
          iter = iter2;
          arg = get_next(iter);
          const unsigned nargs = tuple_size(args) - 1;
          if (nargs==0) {
            fill_arg = sentinel_one;
          } else {
            auto arr1 = tuple_items(args);
            if (nargs==1) {
              fill_arg = arr1[1];
            } else {
              fill_arg = PyTuple_New(nargs);
              std::copy(arr1+1, arr1+1+nargs, tuple_items(fill_arg));
              for (PyObject** p=arr1+nargs; p!=arr1; --p)
                Py_INCREF(*p);
              need_decref = true;
            }
          }
        } else { // first arg not iterable
          PyErr_Clear();
          fill_arg = sentinel_one;
        }

        PyObject* bin = self->h.fill_at([&]{
          const unsigned n = self->h.naxes();
          std::vector<index_type> ii(n);
          unsigned i = 0;
          for (; arg; ++i) {
            if (i==n) throw error(PyExc_ValueError,
              "too many indices passed to histogram.fill_at()");
            ii[i] = unpy_check<index_type>(arg);
            arg = get_next(iter);
          }
          if (i!=n) throw error(PyExc_ValueError,
            "too few indices passed to histogram.fill_at()");
          return ii;
        }(), fill_arg);
        Py_INCREF(bin);
        if (need_decref) Py_DECREF(fill_arg);
        return bin;
      } catch(...) { lipp(); return nullptr; }
    }, METH_VARARGS, "fill bin at given index" },
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
    tp_iter = (::getiterfunc) +[](PyObject* self) noexcept {
      return PyObject_CallObject(
        py_cast<PyObject>(&hist_iter_py_type),
        +static_py_tuple(self)
      );
    };
  }
} hist_py_type;

// end hist defs ====================================================

struct py_mc_bin {
  PyObject_HEAD // must not be overwritten in the constructor

  mc_bin bin;

  py_mc_bin(PyObject* args, PyObject* kwargs) noexcept { }

  PyObject* str() const noexcept {
    std::stringstream ss;
    ss << "{ w: " << bin.w
       << ", w2: " << bin.w2
       << ", n: " << bin.n
       << " }";
    return py(std::move(ss).str());
  }
}; // end py_mc_bin -------------------------------------------------

struct mc_bin_nb_methods: PyNumberMethods {
  mc_bin_nb_methods(): PyNumberMethods{ } {
    nb_inplace_add = (::binaryfunc) +[](py_mc_bin* self, PyObject* arg)
    noexcept {
      if (arg == sentinel_one) ++self->bin;
      else self->bin += unpy_check<double>(arg);
      Py_INCREF(self);
      return self;
    };
  }
} mc_bin_nb_methods;

// TODO: add member access
// TODO: allow static weight to be set

struct mc_bin_py_type: PyTypeObject {
  mc_bin_py_type(): PyTypeObject{ PyVarObject_HEAD_INIT(nullptr, 0) } {
    tp_name = "histograms.mc_bin";
    tp_basicsize = sizeof(py_mc_bin);
    tp_dealloc = (::destructor) ivanp::python::tp_dealloc<py_mc_bin>;
    tp_str = (::reprfunc) ivanp::python::tp_str<py_mc_bin>;
    tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE;
    tp_doc = "MC bin object";
    tp_new = (::newfunc) ivanp::python::tp_new<py_mc_bin>;
    tp_as_number = &mc_bin_nb_methods;
  }
} mc_bin_py_type;

// end bin defs =====================================================

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
} // end ivanp::hist namespace

PyMODINIT_FUNC PyInit_histograms() {
  using namespace ivanp::python;

  ivanp::hist::sentinel_one = PyLong_FromLong(1); // never leaves the module
  ivanp::hist::double_zero = py<double>(0.);

  struct {
    PyTypeObject* type;
    const char* name;
  } static constexpr py_types[] {
    { &ivanp::hist::axis_py_type, "axis" },
    { &ivanp::hist::axis_iter_py_type, "axis_iterator" },
    { &ivanp::hist::hist_py_type, "histogram" },
    { &ivanp::hist::hist_iter_py_type, "histogram_iterator" },
    { &ivanp::hist::mc_bin_py_type, "mc_bin" }
  };

  for (auto [type, name] : py_types)
    if (PyType_Ready(type) < 0) return nullptr;

  // PyDict_SetItemString(
  //   ivanp::hist::mc_bin_py_type.tp_dict,
  //   "weight",
  //   &mc_bin::bin_type::weight
  // );

  PyObject* m = PyModule_Create(&ivanp::hist::py_module);
  if (!m) return nullptr;

  unsigned n = 0;
  for (auto [type, name] : py_types) {
    Py_INCREF(type);
    ++n;
    if (PyModule_AddObject(m, name, py_cast<PyObject>(type)) < 0) {
      do {
        Py_DECREF(py_types[--n].type);
      } while (n);
      Py_DECREF(m);
      return nullptr;
    }
  }

  return m;
}
