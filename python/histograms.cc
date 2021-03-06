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
using namespace ivanp::cont::ops::map;

namespace ivanp::hist {
namespace {
using namespace ivanp::python;

// TODO: fix __reduce__ expanding uniform axis

using edge_type = double;

static PyObject *sentinel_one, *double_zero;

struct axis_py_type: PyTypeObject {
  axis_py_type();
} axis_py_type;

struct hist_py_type: PyTypeObject {
  hist_py_type();
} hist_py_type;

struct py_axis {
  PyObject_HEAD // must not be overwritten in the constructor

  using axis_type = variant_axis<
    uniform_axis<edge_type>,
    cont_axis<std::vector<edge_type>>
  >;
  axis_type axis;

  axis_type& operator*() noexcept { return axis; }
  const axis_type& operator*() const noexcept { return axis; }
  axis_type* operator->() noexcept { return &axis; }
  const axis_type* operator->() const noexcept { return &axis; }

  [[noreturn]] static void uniform_args_error() {
    throw error(PyExc_ValueError,
      "uniform axis arguments must be of the form"
      " [ndiv,min,max] or [ndiv,min,max,\"flags\"]");
  }

  py_axis(PyObject* args, PyObject* kwargs) {
    std::vector<edge_type> edges;
    index_type ndiv = 0;
    edge_type min, max;

    auto iter = get_iter(args); // guaranteed tuple
    auto arg = get_next(iter);
    if (!arg) throw error(PyExc_TypeError,
      "empty list of axis arguments");
    for (;;) { // loop over arguments
      TEST(arg->ob_type->tp_name)
      TEST(unpy<std::string_view>(PyObject_Str(arg)))
      if (arg->ob_type == &axis_py_type) { // argument is axis
        auto* ptr = &***py_cast<py_axis>(+arg);
        if (auto* ax = std::get_if<0>(ptr)) {
          ndiv = ax->ndiv();
          min = ax->min();
          max = ax->max();
        } else if (auto* ax = std::get_if<1>(ptr)) {
          auto& src = ax->edges();
          edges.insert(edges.end(), src.begin(), src.end());
        }
      } else if (auto subiter = get_iter(arg)) { // uniform chunk
        // Note: strings are also iterable and will enter here

        auto subarg = get_next(subiter);
        if (!subarg) uniform_args_error();
        unpy_check_to(ndiv,subarg);

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
      if (ndiv && (arg || !edges.empty())) {
        const edge_type d = (max - min)/ndiv;
        edges.reserve(edges.size()+ndiv+1);
        for (index_type i=0; i<=ndiv; ++i)
          edges.push_back(min + i*d);
        ndiv = 0;
      }
      if (!arg) break;
    }

    // make the axis
    if (edges.empty()) {
      (*axis).emplace<0>(ndiv, min, max);
    } else {
      std::sort( begin(edges), end(edges) );
      edges.erase( std::unique( begin(edges), end(edges) ), end(edges) );
      (*axis).emplace<1>(std::move(edges));
    }
  }

  PyObject* operator()(PyObject* args, PyObject* kwargs) const {
    auto iter = get_iter(args);
    if (!iter) throw existing_error{};

    auto arg = get_next(iter);
    if (!arg || get_next(iter)) throw error(PyExc_TypeError,
      "axis call expression takes exactly one argument");

    return py(axis.find_bin_index(unpy_check<edge_type>(arg)));
  }

  PyObject* str() const noexcept {
    std::stringstream ss;
    if (auto* ax = std::get_if<0>(&*axis)) {
      ss << "axis: { ndiv: " << ax->ndiv()
         << ", min: " << ax->min()
         << ", max: " << ax->max()
         << " }";
    } else if (auto* ax = std::get_if<1>(&*axis)) {
      ss << "axis: [ ";
      bool first = true;
      for (auto x : ax->edges()) {
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

  using axis_type = typename py_axis::axis_type;
  const axis_type& axis;
  index_type i;

  py_axis_iter(PyObject* args, PyObject* kwargs) noexcept
  : py_axis_iter( **py_cast<py_axis>( tuple_items(args)[0] ) )
  { }

  py_axis_iter(axis_type& axis) noexcept: axis(axis), i(0) { }
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
  { "ndiv", (PyCFunction) +[](py_axis* self) noexcept {
      return py((*self)->ndiv());
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
  { "__reduce__", (PyCFunction) +[](py_axis* self) noexcept
    -> PyObject* {
      PyObject* const tup = PyTuple_New(2);
      auto* const t = tuple_items(tup);
      t[0] = py(reinterpret_cast<PyTypeObject*>(&axis_py_type));
      if (auto* axis = std::get_if<0>(&***self)) {
        auto* x = tuple_items((
          tuple_items(( t[1] = PyTuple_New(1) ))[0] = PyTuple_New(3) ));
        x[0] = py(axis->ndiv());
        x[1] = py(axis->min());
        x[2] = py(axis->max());
      } else if (auto* axis = std::get_if<1>(&***self)) {
        using namespace ivanp::cont;
        map<map_flags::no_size_check>([](auto& to, edge_type from){
          to = py(from);
        }, tuple_span(PyTuple_New(axis->nedges())), axis->edges());
      }
      TEST(unpy<std::string_view>(PyObject_Str(py(self))))
      TEST(unpy<std::string_view>(PyObject_Str(tup)))
      return tup;
    }, METH_NOARGS, "" },
  { }
};

axis_py_type::axis_py_type()
: PyTypeObject{ PyVarObject_HEAD_INIT(nullptr, 0) } {
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
      py(&axis_iter_py_type),
      +static_py_tuple(self)
    );
  };
}

// end axis defs ====================================================

struct py_bin_filler {
  static PyObject* fill(py_ptr& bin, PyObject* arg) {
    PyObject* sum = PyNumber_InPlaceAdd(bin,arg);
    if (!sum) [[unlikely]] throw existing_error{};
    if (sum != bin) bin = py_ptr(sum);
    return bin;
  }
};

struct py_hist {
  PyObject_HEAD

  struct axis_ptr: py_ptr {
    using py_ptr::py_ptr;
    const py_axis::axis_type& operator*() const noexcept {
      return **py_cast<py_axis>(p);
    }
    const py_axis::axis_type* operator->() const noexcept {
      return &**this;
    }
  };

  using hist = histogram<
    py_ptr,
    axes_spec<std::vector<axis_ptr>>,
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
          PyObject* ax = call_with_iterable(py(&axis_py_type), *arg);
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
        using namespace ivanp::cont;
        map<map_flags::no_size_check>([](auto& to, auto& from){
          auto* const ob_type = from->ob_type;
          if (ob_type == &PyFloat_Type || ob_type == &PyLong_Type)
            Py_INCREF((to = from));
          else
            to = py_ptr(PyObject_CallObject(
              py(ob_type), +static_py_tuple(+from) ));
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
        const unsigned n = h.ndim();
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
      const unsigned n = h.ndim();
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
  { "ndim", (PyCFunction) +[](py_hist* self) noexcept {
      return py(self->h.ndim());
    }, METH_NOARGS, "number of axes" },
  { "size", (PyCFunction) +[](py_hist* self) noexcept {
      return py(self->h.size());
    }, METH_NOARGS, "total number of bins, including overflow" },
  { "join_index", (PyCFunction) +[](py_hist* self, PyObject* args) noexcept
    -> PyObject* {
      try {
        return py( self->h.join_index( map_py_args(
          args, unpy_check<index_type>, self->h.ndim()
        )));
      } catch(...) { lipp(); return nullptr; }
    }, METH_VARARGS, "compute global bin index" },
  { "bin_at", (PyCFunction) +[](py_hist* self, PyObject* args) noexcept
    -> PyObject* {
      try {
        return self->h.bin_at( map_py_args(
          args, unpy_check<index_type>, self->h.ndim()
        ));
      } catch(...) { lipp(); return nullptr; }
    }, METH_VARARGS, "bin at given indices" },
  { "find_bin_index", (PyCFunction) +[](py_hist* self, PyObject* args) noexcept
    -> PyObject* {
      try {
        return py( self->h.find_bin_index( map_py_args(
          args, unpy_check<edge_type>, self->h.ndim()
        )));
      } catch(...) { lipp(); return nullptr; }
    }, METH_VARARGS, "bin index from coordinates" },
  { "find_bin", (PyCFunction) +[](py_hist* self, PyObject* args) noexcept
    -> PyObject* {
      try {
        return self->h.find_bin( map_py_args(
          args, unpy_check<edge_type>, self->h.ndim()
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
          const unsigned n = self->h.ndim();
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
  // https://www.python.org/dev/peps/pep-0307/#extended-reduce-api
  { "__reduce__", (PyCFunction) +[](py_hist* self) noexcept
    -> PyObject* {
      auto& h = self->h;
      PyObject* const tup = PyTuple_New(3);
      auto* const t = tuple_items(tup);
      t[0] = py(reinterpret_cast<PyTypeObject*>(&hist_py_type));

      std::copy(h.axes().begin(),h.axes().end(),
        tuple_items(( t[1] = PyTuple_New(h.ndim()) )) );
      h.axes() | [](PyObject* p) noexcept { Py_INCREF(p); };

      std::copy(h.begin(),h.end(),
        tuple_items(( t[2] = PyTuple_New(h.size()) )) );
      h | [](PyObject* p) noexcept { Py_INCREF(p); };

      return tup;
    }, METH_NOARGS, "" },
  { "__setstate__", (PyCFunction) +[](py_hist* self, PyObject* arg) noexcept
    -> PyObject* {
      try {
        cont::map([](auto& to, PyObject* from) noexcept {
          Py_INCREF(( to = py_ptr(from) ));
        }, self->h, tuple_span(arg));
      } catch (...) { lipp(); return nullptr; }
      Py_RETURN_NONE;
    }, METH_O, "" },
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

hist_py_type::hist_py_type()
: PyTypeObject{ PyVarObject_HEAD_INIT(nullptr, 0) } {
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
      py(&hist_iter_py_type),
      +static_py_tuple(self)
    );
  };
}

// end hist defs ====================================================

struct py_mc_bin {
  PyObject_HEAD // must not be overwritten in the constructor

  mc_bin<> bin;

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

PyMethodDef methods[] {
  { "restore_histogram", (PyCFunction) +[](py_hist* self, PyObject* args)
    noexcept -> PyObject* {
      try {
        // auto args = tuple_span(args);
        // auto* h = PyObject_CallObject(
        //   py(&axis_iter_py_type),
        //   +static_py_tuple(self)
        // );
        return PyObject_Str(args);
      } catch(...) { lipp(); return nullptr; }
    }, METH_VARARGS, "restore a histogram from tuple returned by __reduce__" },
  { } // sentinel
};

struct py_module: PyModuleDef {
  py_module(): PyModuleDef{ PyModuleDef_HEAD_INIT } {
    m_name = "histograms";
    m_doc = "Python bindings for the histograms library";
    m_size = -1;
    m_methods = methods;
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
    if (PyModule_AddObject(m, name, py(type)) < 0) {
      do {
        Py_DECREF(py_types[--n].type);
      } while (n);
      Py_DECREF(m);
      return nullptr;
    }
  }

  return m;
}
