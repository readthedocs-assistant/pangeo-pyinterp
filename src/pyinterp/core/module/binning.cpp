// Copyright (c) 2022 CNES
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.
#include "pyinterp/binning.hpp"

#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;

template <typename Type>
void implement_binning_2d(py::module& m, const std::string& suffix) {
  PYBIND11_NUMPY_DTYPE(pyinterp::detail::math::Accumulators<Type>, count,
                       sum_of_weights, mean, min, max, sum, mom2, mom3, mom4);

  py::class_<pyinterp::Binning2D<Type>>(m, ("Binning2D" + suffix).c_str(),
                                        R"__doc__(
Group a number of more or less continuous values into a smaller number of
"bins" located on a grid.
)__doc__")
      .def(py::init<std::shared_ptr<pyinterp::Axis<double>>,
                    std::shared_ptr<pyinterp::Axis<double>>,
                    std::optional<pyinterp::geodetic::System>>(),
           py::arg("x"), py::arg("y"),
           py::arg("wgs") = std::optional<pyinterp::geodetic::System>(),
           R"__doc__(
Default constructor

Args:
    x (pyinterp.core.Axis): Definition of the bin centers for the X axis of
        the grid.
    y (pyinterp.core.Axis): Definition of the bin centers for the Y axis of
        the grid.
    wgs (pyinterp.geodetic.System, optional): WGS of the coordinate system
        used to manipulate geographic coordinates. If this parameter is not
        set, the handled coordinates will be considered as Cartesian
        coordinates. Otherwise, ``x`` and ``y`` are considered to represents
        the longitudes and latitudes.
)__doc__")
      .def_property_readonly(
          "x", [](const pyinterp::Binning2D<Type>& self) { return self.x(); },
          R"__doc__(
Gets the bin centers for the X Axis of the grid.

Returns:
    pyinterp.core.Axis: X-Axis.
)__doc__")
      .def_property_readonly(
          "y", [](const pyinterp::Binning2D<Type>& self) { return self.y(); },
          R"__doc__(
Gets the bin centers for the Y Axis of the grid.

Returns:
    pyinterp.core.Axis: Y-Axis.
)__doc__")
      .def_property_readonly(
          "wgs",
          [](const pyinterp::Binning2D<Type>& self) { return self.wgs(); },
          R"__doc__(
Gets the WGS system handled by this instance.

Returns:
    pyinterp.core.geodetic.System: Geodetic system.
)__doc__")
      .def("clear", &pyinterp::Binning2D<Type>::clear, "Reset the statistics")
      .def("count", &pyinterp::Binning2D<Type>::count,
           R"__doc__(
Compute the count of points within each bin.

Returns:
    numpy.ndarray: count of points within each bin.
)__doc__")
      .def("kurtosis", &pyinterp::Binning2D<Type>::kurtosis,
           R"__doc__(
Compute the kurtosis of values for points within each bin.

Returns:
    numpy.ndarray: kurtosis of values for points within each bin.
)__doc__")
      .def("max", &pyinterp::Binning2D<Type>::max,
           R"__doc__(
Compute the maximum of values for points within each bin.

Returns:
    numpy.ndarray: maximum of values for points within each bin.
)__doc__")
      .def("mean", &pyinterp::Binning2D<Type>::mean,
           R"__doc__(
Compute the mean of values for points within each bin.

Returns:
    numpy.ndarray: mean of values for points within each bin.
)__doc__")
      .def("min", &pyinterp::Binning2D<Type>::min,
           R"__doc__(
Compute the minimum of values for points within each bin.

Returns:
    numpy.ndarray: minimum of values for points within each bin.
)__doc__")
      .def("push", &pyinterp::Binning2D<Type>::push, py::arg("x"), py::arg("y"),
           py::arg("z"), py::arg("simple") = true, R"__doc__(
Push new samples into the defined bins.

Args:
    x (numpy.ndarray): X coordinates of the values to push.
    y (numpy.ndarray): Y coordinates of the values to push.
    z (numpy.ndarray): New samples to push.
    simple (bool, optional):  If true, a simple binning 2D is used
    otherwise a linear binning 2d is applied.
)__doc__")
      .def("sum", &pyinterp::Binning2D<Type>::sum,
           R"__doc__(
Compute the sum of values for points within each bin.

Returns:
    numpy.ndarray: sum of values for points within each bin.
)__doc__")
      .def("sum_of_weights", &pyinterp::Binning2D<Type>::sum_of_weights,
           R"__doc__(
Compute the sum of weights for points within each bin.

Returns:
    numpy.ndarray: sum of weights for points within each bin.
)__doc__")
      .def("skewness", &pyinterp::Binning2D<Type>::skewness,
           R"__doc__(
Compute the skewness of values for points within each bin.

Returns:
    numpy.ndarray: skewness of values for points within each bin.
)__doc__")
      .def("variance", &pyinterp::Binning2D<Type>::variance,
           py::arg("ddof") = 0,
           R"__doc__(
Compute the variance of values for points within each bin.

Args:
    ddof (int, optional): Means Delta Degrees of Freedom. The divisor used in
        calculations is N - ddof, where N represents the number of elements.
        By default ddof is zero.

Returns:
    numpy.ndarray: variance of values for points within each bin.
)__doc__")
      .def("__iadd__", &pyinterp::Binning2D<Type>::operator+=,
           py::call_guard<py::gil_scoped_release>())
      .def(py::pickle(
          [](const pyinterp::Binning2D<Type>& self) { return self.getstate(); },
          [](const py::tuple& state) {
            return pyinterp::Binning2D<Type>::setstate(state);
          }));
}

void init_binning(py::module& m) {
  implement_binning_2d<double>(m, "Float64");
  implement_binning_2d<float>(m, "Float32");
}
