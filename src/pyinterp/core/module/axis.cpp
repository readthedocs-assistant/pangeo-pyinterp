// Copyright (c) 2022 CNES
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.
#include "pyinterp/axis.hpp"

#include <pybind11/eigen.h>
#include <pybind11/stl.h>

#include "pyinterp/detail/broadcast.hpp"
#include "pyinterp/temporal_axis.hpp"

namespace py = pybind11;

template <class Axis, class Coordinates>
auto implement_axis(py::class_<Axis, std::shared_ptr<Axis>>& axis,
                    const std::string& name) {
  axis.def(
          "__repr__",
          [](const Axis& self) -> std::string {
            return static_cast<std::string>(self);
          },
          "Called by the ``repr()`` built-in function to compute the string "
          "representation of an Axis.")
      .def(
          "__getitem__",
          [](const Axis& self, size_t index) {
            return self.coordinate_value(index);
          },
          py::arg("index"))
      .def("__getitem__", &Axis::coordinate_values, py::arg("indices"))
      .def("__len__", [](const Axis& self) { return self.size(); })
      .def(
          "is_regular",
          [](const Axis& self) -> bool { return self.is_regular(); },
          R"__doc__(
Check if this axis values are spaced regularly.

Returns:
  bool: True if this axis values are spaced regularly.
)__doc__")
      .def(
          "flip",
          [](std::shared_ptr<Axis>& self,
             const bool inplace) -> std::shared_ptr<Axis> {
            if (inplace) {
              self->flip();
              return self;
            }
            auto result =
                std::make_shared<Axis>(Axis::setstate(self->getstate()));
            result->flip();
            return result;
          },
          py::arg("inplace") = false,
          (R"__doc__(
Reverse the order of elements in this axis.

Args:
    inplace (bool, optional): If true, this instance will be modified,
        otherwise the modification will be made on a copy. Default to
        ``False``.

Returns:
    )__doc__" +
           name + R"__doc__(: The flipped axis.
)__doc__")
              .c_str())
      .def(
          "find_index",
          [](const Axis& self, Coordinates& coordinates,
             const bool bounded) -> py::array_t<int64_t> {
            return self.find_index(coordinates, bounded);
          },
          py::arg("coordinates"), py::arg("bounded") = false, R"__doc__(
Given coordinate positions, find what grid elements contains them, or is
closest to them.

Args:
    coordinates (numpy.ndarray): Positions in this coordinate system.
    bounded (bool, optional): True if you want to obtain the closest value to
        a coordinate outside the axis definition range.
Returns:
    numpy.ndarray: index of the grid points containing them or -1 if the
    ``bounded`` parameter is set to false and if one of the searched indexes
    is out of the definition range of the axis, otherwise the index of the
    closest value of the coordinate is returned.
)__doc__")
      .def(
          "find_indexes",
          [](const Axis& self,
             Coordinates& coordinates) -> py::array_t<int64_t> {
            return self.find_indexes(coordinates);
          },
          py::arg("coordinates"), R"__doc__(
For all coordinate positions, search for the axis elements around them. This
means that for n coordinate ``ix`` of the provided array, the method searches
the indexes ``i0`` and ``i1`` as fallow:

.. code::

  self[i0] <= coordinates[ix] <= self[i1]

The provided coordinates located outside the axis definition range are set to
``-1``.

Args:
    coordinates (numpy.ndarray): Positions in this coordinate system.
Returns:
    numpy.ndarray: A matrix of shape ``(n, 2)``. The first column of the
    matrix contains the indexes ``i0`` and the second column the indexes
    ``i1`` found.
)__doc__")
      .def("is_ascending", &Axis::is_ascending, R"__doc__(
Test if the data is sorted in ascending order.

Returns:
    bool: True if the data is sorted in ascending order.
)__doc__")
      .def(
          "__eq__",
          [](const Axis& self, const Axis& rhs) -> bool { return self == rhs; },
          py::arg("other"),
          "Overrides the default behavior of the ``==`` operator.")
      .def(
          "__ne__",
          [](const Axis& self, const Axis& rhs) -> bool { return self != rhs; },
          py::arg("other"),
          "Overrides the default behavior of the ``!=`` operator.")
      .def(py::pickle(
          [](const Axis& self) { return self.getstate(); },
          [](const py::tuple& state) { return Axis::setstate(state); }));
}

static void init_core_axis(py::module& m) {
  using Axis = pyinterp::Axis<double>;

  auto axis = py::class_<Axis, std::shared_ptr<Axis>>(m, "Axis", R"__doc__(
A coordinate axis is a Variable that specifies one of the coordinates
of a variable's values.
)__doc__");

  axis.def(py::init<>([](py::array_t<double, py::array::c_style>& values,
                         const double epsilon, const bool is_circle) {
             return std::make_shared<Axis>(values, epsilon, is_circle);
           }),
           py::arg("values"), py::arg("epsilon") = 1e-6,
           py::arg("is_circle") = false,
           R"__doc__(
Create a coordinate axis from values.

Args:
    values (numpy.ndarray): Axis values.
    epsilon (float, optional): Maximum allowed difference between two real
        numbers in order to consider them equal. Defaults to ``1e-6``.
    is_circle (bool, optional): True, if the axis can represent a
        circle. Defaults to ``false``.
)__doc__")
      .def_property_readonly(
          "is_circle",
          [](const Axis& self) -> bool { return self.is_circle(); },
          R"__doc__(
Test if this axis represents a circle.

Returns:
    bool: True if this axis represents a circle.
)__doc__")
      .def("front", &Axis::front, R"__doc__(
Get the first value of this axis.

Returns:
    float: The first value.
)__doc__")
      .def("back", &Axis::back, R"__doc__(
Get the last value of this axis.

Returns:
    float: The last value.
)__doc__")
      .def("increment", &Axis::increment, R"__doc__(
Get increment value if is_regular().

Raises:
    RuntimeError: if this instance does not represent a regular axis.
Returns:
    float: Increment value.
)__doc__")
      .def("min_value", &Axis::min_value, R"__doc__(
Get the minimum coordinate value.

Returns:
    float: The minimum coordinate value.
)__doc__")
      .def("max_value", &Axis::max_value, R"__doc__(
Get the maximum coordinate value.

Returns:
    float: The maximum coordinate value.
)__doc__");
  implement_axis<Axis, const py::array_t<double>>(axis, "pyinterp.core.Axis");
}

void init_temporal_axis(py::module& m) {
  // Required to declare the relationship between C++ classes
  // pyinterp::TemporalAxis & pyinterp::Axis<int64_t>. pyinterp::Axis<int64_t>
  // is used by the time grids.
  auto base_class =
      py::class_<pyinterp::Axis<int64_t>,
                 std::shared_ptr<pyinterp::Axis<int64_t>>>(m, "AxisInt64");

  auto axis = py::class_<pyinterp::TemporalAxis,
                         std::shared_ptr<pyinterp::TemporalAxis>>(
      m, "TemporalAxis", base_class, "Time axis");

  axis.def(py::init<>([](const py::array& values) {
             return std::make_shared<pyinterp::TemporalAxis>(values);
           }),
           py::arg("values"),
           R"__doc__(
Create a coordinate axis from values.

Args:
    values (numpy.ndarray): Items representing the datetimes or
        timedeltas of the axis.

Raises:
    TypeError: if the array data type is not a datetime64 or timedelta64
        subtype.

Examples:

    >>> import datetime
    >>> import numpy as np
    >>> import pyinterp
    >>> start = datetime.datetime(2000, 1, 1)
    >>> values = np.array([
    ...     start + datetime.timedelta(hours=index)
    ...     for index in range(86400)
    ... ],
    ...                   dtype="datetime64[us]")
    >>> axis = pyinterp.TemporalAxis(values)
    >>> axis
    <pyinterp.core.TemporalAxis>
      min_value: 2000-01-01T00:00:00.000000
      max_value: 2009-11-08T23:00:00.000000
      step     : 3600000000 microseconds
    >>> values = np.array([
    ...     datetime.timedelta(hours=index)
    ...     for index in range(86400)
    ... ],
    ...                   dtype="timedelta64[us]")
    >>> axis = pyinterp.TemporalAxis(values)
    >>> axis
    <pyinterp.core.TemporalAxis>
      min_value: 0 microseconds
      max_value: 311036400000000 microseconds
      step     : 3600000000 microseconds
)__doc__")
      .def("dtype", &pyinterp::TemporalAxis::dtype, R"__doc__(
Data-type of the axis's elements.

Returns:
    numpy.dtype: numpy dtype object.
)__doc__")
      .def("front", &pyinterp::TemporalAxis::front, R"__doc__(
Get the first value of this axis.

Returns:
    numpy.array: The first value.
)__doc__")
      .def("back", &pyinterp::TemporalAxis::back, R"__doc__(
Get the last value of this axis.

Returns:
    numpy.array: The last value.
)__doc__")
      .def("increment", &pyinterp::TemporalAxis::increment, R"__doc__(
Get increment value if is_regular().

Raises:
    RuntimeError: if this instance does not represent a regular axis.
Returns:
    numpy.array: Increment value.
)__doc__")
      .def("min_value", &pyinterp::TemporalAxis::min_value, R"__doc__(
Get the minimum coordinate value.

Returns:
    numpy.array: The minimum coordinate value.
)__doc__")
      .def("max_value", &pyinterp::TemporalAxis::max_value, R"__doc__(
Get the maximum coordinate value.

Returns:
    numpy.array: The maximum coordinate value.
)__doc__")
      .def(
          "safe_cast",
          [](const pyinterp::TemporalAxis& self, const pybind11::array& array)
              -> py::array { return self.safe_cast("values", array); },
          py::arg("values"),
          R"__doc__(
Convert the values of the vector in the same unit as the time axis
handled by this instance.

Args:
    values (numpy.ndarray): Values to convert.

Returns:
    numpy.ndarray: values converted.

Raises:
    UserWarning: If the implicit conversion of the supplied values to the
        resolution of the axis truncates the values (e.g. converting
        microseconds to seconds).
)__doc__");

  implement_axis<pyinterp::TemporalAxis, py::array>(
      axis, "pyinterp.core.TemporalAxis");
}

void init_axis(py::module& m) {
  py::enum_<pyinterp::axis::Boundary>(m, "AxisBoundary", R"__doc__(
Type of boundary handling.
)__doc__")
      .value("Expand", pyinterp::axis::kExpand,
             "*Expand the boundary as a constant*.")
      .value("Wrap", pyinterp::axis::kWrap, "*Circular boundary conditions*.")
      .value("Sym", pyinterp::axis::kSym, "*Symmetrical boundary conditions*.")
      .value("Undef", pyinterp::axis::kUndef,
             "*Boundary violation is not defined*.");

  init_core_axis(m);
  init_temporal_axis(m);
}
