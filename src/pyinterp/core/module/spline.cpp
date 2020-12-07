// Copyright (c) 2020 CNES
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.
#include "pyinterp/spline.hpp"

#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <cctype>

#include "pyinterp/detail/math/linear.hpp"
#include "pyinterp/detail/math/spline2d.hpp"
#include "pyinterp/detail/thread.hpp"
#include "pyinterp/xarray.hpp"

namespace py = pybind11;

namespace pyinterp {

/// Returns the GSL interp type
static inline auto interp_type(const FittingModel kind)
    -> const gsl_interp_type* {
  switch (kind) {
    case kLinear:
      return gsl_interp_linear;
    case kPolynomial:
      return gsl_interp_polynomial;
    case kCSpline:
      return gsl_interp_cspline;
    case kCSplinePeriodic:
      return gsl_interp_cspline_periodic;
    case kAkima:
      return gsl_interp_akima;
    case kAkimaPeriodic:
      return gsl_interp_akima_periodic;
    case kSteffen:
      return gsl_interp_steffen;
    default:
      throw std::invalid_argument("Invalid interpolation type: " +
                                  std::to_string(kind));
  }
}

/// Evaluate the interpolation.
template <typename DataType>
auto spline(const Grid2D<DataType>& grid, const py::array_t<double>& x,
            const py::array_t<double>& y, size_t nx, size_t ny,
            FittingModel fitting_model, const axis::Boundary boundary,
            const bool bounds_error, size_t num_threads)
    -> py::array_t<double> {
  detail::check_array_ndim("x", 1, x, "y", 1, y);
  detail::check_ndarray_shape("x", x, "y", y);

  auto size = x.size();
  auto result = py::array_t<double>(py::array::ShapeContainer{size});

  auto _x = x.template unchecked<1>();
  auto _y = y.template unchecked<1>();
  auto _result = result.template mutable_unchecked<1>();
  {
    py::gil_scoped_release release;

    // Captures the detected exceptions in the calculation function
    // (only the last exception captured is kept)
    auto except = std::exception_ptr(nullptr);

    // Access to the shared pointer outside the loop to avoid data races
    const auto is_angle = grid.x()->is_angle();

    detail::dispatch(
        [&](const size_t start, const size_t end) {
          try {
            auto frame = detail::math::XArray2D(nx, ny);
            auto interpolator =
                detail::math::Spline2D(frame, interp_type(fitting_model));

            for (size_t ix = start; ix < end; ++ix) {
              auto xi = _x(ix);
              auto yi = _y(ix);
              _result(ix) =
                  // The grid instance is accessed as a constant reference, no
                  // data race problem here.
                  load_frame(grid, xi, yi, boundary, bounds_error, frame)
                      ? interpolator.interpolate(
                            is_angle ? frame.normalize_angle(xi) : xi, yi,
                            frame)
                      : std::numeric_limits<double>::quiet_NaN();
            }
          } catch (...) {
            except = std::current_exception();
          }
        },
        size, num_threads);

    if (except != nullptr) {
      std::rethrow_exception(except);
    }
  }
  return result;
}

/// Evaluate the interpolation.
template <typename DataType, typename AxisType>
auto spline_3d(const Grid3D<DataType, AxisType>& grid,
               const py::array_t<double>& x, const py::array_t<double>& y,
               const py::array_t<AxisType>& z, size_t nx, size_t ny,
               FittingModel fitting_model, const axis::Boundary boundary,
               const bool bounds_error, size_t num_threads)
    -> py::array_t<double> {
  detail::check_array_ndim("x", 1, x, "y", 1, y, "z", 1, z);
  detail::check_ndarray_shape("x", x, "y", y, "z", z);

  auto size = x.size();
  auto result = py::array_t<double>(py::array::ShapeContainer{size});

  auto _x = x.template unchecked<1>();
  auto _y = y.template unchecked<1>();
  auto _z = z.template unchecked<1>();
  auto _result = result.template mutable_unchecked<1>();
  {
    py::gil_scoped_release release;

    // Captures the detected exceptions in the calculation function
    // (only the last exception captured is kept)
    auto except = std::exception_ptr(nullptr);

    // Access to the shared pointer outside the loop to avoid data races
    const auto is_angle = grid.x()->is_angle();

    detail::dispatch(
        [&](const size_t start, const size_t end) {
          try {
            auto frame = detail::math::XArray3D<AxisType>(nx, ny, 1);
            auto interpolator = detail::math::Spline2D(
                detail::math::XArray2D(nx, ny), interp_type(fitting_model));

            for (size_t ix = start; ix < end; ++ix) {
              auto xi = _x(ix);
              auto yi = _y(ix);
              auto zi = _z(ix);

              if (load_frame<DataType, AxisType>(grid, xi, yi, zi, boundary,
                                                 bounds_error, frame)) {
                xi = is_angle ? frame.normalize_angle(xi) : xi;
                auto z0 = interpolator.interpolate(xi, yi, frame.xarray_2d(0));
                auto z1 = interpolator.interpolate(xi, yi, frame.xarray_2d(1));
                _result(ix) = detail::math::linear<AxisType, double>(
                    zi, frame.z(0), frame.z(1), z0, z1);
              } else {
                _result(ix) = std::numeric_limits<double>::quiet_NaN();
              }
            }
          } catch (...) {
            except = std::current_exception();
          }
        },
        size, num_threads);

    if (except != nullptr) {
      std::rethrow_exception(except);
    }
  }
  return result;
}

/// Evaluate the interpolation.
template <typename DataType, typename AxisType>
auto spline_4d(const Grid4D<DataType, AxisType>& grid,
               const py::array_t<double>& x, const py::array_t<double>& y,
               const py::array_t<AxisType>& z, const py::array_t<double>& u,
               size_t nx, size_t ny, FittingModel fitting_model,
               const axis::Boundary boundary, const bool bounds_error,
               size_t num_threads) -> py::array_t<double> {
  detail::check_array_ndim("x", 1, x, "y", 1, y, "z", 1, z, "u", 1, u);
  detail::check_ndarray_shape("x", x, "y", y, "z", z, "u", u);

  auto size = x.size();
  auto result = py::array_t<double>(py::array::ShapeContainer{size});

  auto _x = x.template unchecked<1>();
  auto _y = y.template unchecked<1>();
  auto _z = z.template unchecked<1>();
  auto _u = u.template unchecked<1>();
  auto _result = result.template mutable_unchecked<1>();
  {
    py::gil_scoped_release release;

    // Captures the detected exceptions in the calculation function
    // (only the last exception captured is kept)
    auto except = std::exception_ptr(nullptr);

    // Access to the shared pointer outside the loop to avoid data races
    const auto is_angle = grid.x()->is_angle();

    detail::dispatch(
        [&](const size_t start, const size_t end) {
          try {
            auto frame = detail::math::XArray4D<AxisType>(nx, ny, 1, 1);
            auto interpolator = detail::math::Spline2D(
                detail::math::XArray2D(nx, ny), interp_type(fitting_model));

            for (size_t ix = start; ix < end; ++ix) {
              auto xi = _x(ix);
              auto yi = _y(ix);
              auto zi = _z(ix);
              auto ui = _u(ix);

              if (load_frame<DataType, AxisType>(grid, xi, yi, zi, ui, boundary,
                                                 bounds_error, frame)) {
                xi = is_angle ? frame.normalize_angle(xi) : xi;
                auto z00 =
                    interpolator.interpolate(xi, yi, frame.xarray_2d(0, 0));
                auto z10 =
                    interpolator.interpolate(xi, yi, frame.xarray_2d(1, 0));
                auto z01 =
                    interpolator.interpolate(xi, yi, frame.xarray_2d(0, 1));
                auto z11 =
                    interpolator.interpolate(xi, yi, frame.xarray_2d(1, 1));
                _result(ix) = detail::math::linear<double>(
                    ui, frame.u(0), frame.u(1),
                    detail::math::linear<AxisType, double>(
                        zi, frame.z(0), frame.z(1), z00, z10),
                    detail::math::linear<AxisType, double>(
                        zi, frame.z(0), frame.z(1), z01, z11));
              } else {
                _result(ix) = std::numeric_limits<double>::quiet_NaN();
              }
            }
          } catch (...) {
            except = std::current_exception();
          }
        },
        size, num_threads);

    if (except != nullptr) {
      std::rethrow_exception(except);
    }
  }
  return result;
}

}  // namespace pyinterp

template <typename DataType>
void implement_spline(py::module& m, const std::string& suffix) {
  auto function_suffix = suffix;
  function_suffix[0] = std::tolower(function_suffix[0]);

  m.def(("spline_" + function_suffix).c_str(), &pyinterp::spline<DataType>,
        py::arg("grid"), py::arg("x"), py::arg("y"), py::arg("nx") = 3,
        py::arg("ny") = 3,
        py::arg("fitting_model") = pyinterp::FittingModel::kCSpline,
        py::arg("boundary") = pyinterp::axis::kUndef,
        py::arg("bounds_error") = false, py::arg("num_threads") = 0,
        (R"__doc__(
Spline gridded 2D interpolation.

Args:
    grid (pyinterp.core.Grid2D)__doc__" +
         suffix +
         R"__doc__(): Grid containing the values to be interpolated.
    x (numpy.ndarray): X-values
    y (numpy.ndarray): Y-values
    nx (int, optional): The number of X coordinate values required to perform
        the interpolation. Defaults to ``3``.
    ny (int, optional): The number of Y coordinate values required to perform
        the interpolation. Defaults to ``3``.
    fitting_model (pyinterp.core.FittingModel, optional): Type of interpolation
        to be performed. Defaults to
        :py:data:`pyinterp.core.FittingModel.CSpline`
    boundary (pyinterp.core.AxisBoundary, optional): Type of axis boundary
        management. Defaults to
        :py:data:`pyinterp.core.AxisBoundary.Undef`
    bounds_error (bool, optional): If True, when interpolated values are
        requested outside of the domain of the input axes (x,y), a ValueError
        is raised. If False, then value is set to NaN.
    num_threads (int, optional): The number of threads to use for the
        computation. If 0 all CPUs are used. If 1 is given, no parallel
        computing code is used at all, which is useful for debugging.
        Defaults to ``0``.
Return:
    numpy.ndarray: Values interpolated
  )__doc__")
            .c_str());
}

template <typename DataType, typename AxisType>
void implement_spline_3d(py::module& m, const std::string& prefix,
                         const std::string& suffix) {
  auto function_suffix = suffix;
  function_suffix[0] = std::tolower(function_suffix[0]);
  m.def(("spline_" + function_suffix).c_str(),
        &pyinterp::spline_3d<DataType, AxisType>, py::arg("grid"), py::arg("x"),
        py::arg("y"), py::arg("z"), py::arg("nx") = 3, py::arg("ny") = 3,
        py::arg("fitting_model") = pyinterp::FittingModel::kCSpline,
        py::arg("boundary") = pyinterp::axis::kUndef,
        py::arg("bounds_error") = false, py::arg("num_threads") = 0,
        (R"__doc__(
Spline gridded 3D interpolation.

A spline 2D interpolation is performed along the X and Y axes of the 3D grid,
and linearly along the Z axis between the two values obtained by the spatial
spline 2D interpolation.

Args:
    grid (pyinterp.core.)__doc__" +
         prefix + "Grid3D" + suffix +
         R"__doc__(): Grid containing the values to be interpolated.
    x (numpy.ndarray): X-values
    y (numpy.ndarray): Y-values
    z (numpy.ndarray): Z-values
    nx (int, optional): The number of X coordinate values required to perform
        the interpolation. Defaults to ``3``.
    ny (int, optional): The number of Y coordinate values required to perform
        the interpolation. Defaults to ``3``.
    fitting_model (pyinterp.core.FittingModel, optional): Type of interpolation
        to be performed. Defaults to
        :py:data:`pyinterp.core.FittingModel.CSpline`
    boundary (pyinterp.core.AxisBoundary, optional): Type of axis boundary
        management. Defaults to
        :py:data:`pyinterp.core.AxisBoundary.Undef`
    bounds_error (bool, optional): If True, when interpolated values are
        requested outside of the domain of the input axes (x,y), a ValueError
        is raised. If False, then value is set to NaN.
    num_threads (int, optional): The number of threads to use for the
        computation. If 0 all CPUs are used. If 1 is given, no parallel
        computing code is used at all, which is useful for debugging.
        Defaults to ``0``.
Return:
    numpy.ndarray: Values interpolated
  )__doc__")
            .c_str());
}

template <typename DataType, typename AxisType>
void implement_spline_4d(py::module& m, const std::string& prefix,
                         const std::string& suffix) {
  auto function_suffix = suffix;
  function_suffix[0] = std::tolower(function_suffix[0]);
  m.def(("spline_" + function_suffix).c_str(),
        &pyinterp::spline_4d<DataType, AxisType>, py::arg("grid"), py::arg("x"),
        py::arg("y"), py::arg("z"), py::arg("u"), py::arg("nx") = 3,
        py::arg("ny") = 3,
        py::arg("fitting_model") = pyinterp::FittingModel::kCSpline,
        py::arg("boundary") = pyinterp::axis::kUndef,
        py::arg("bounds_error") = false, py::arg("num_threads") = 0,
        (R"__doc__(
Spline gridded 4D interpolation

A spline 2D interpolation is performed along the X and Y axes of the 4D grid,
and linearly along the Z and U axes between the four values obtained by the
spatial spline 2D interpolation.

Args:
    grid (pyinterp.core.)__doc__" +
         prefix + "Grid4D" + suffix +
         R"__doc__(): Grid containing the values to be interpolated.
    x (numpy.ndarray): X-values
    y (numpy.ndarray): Y-values
    z (numpy.ndarray): Z-values
    u (numpy.ndarray): U-values
    nx (int, optional): The number of X coordinate values required to perform
        the interpolation. Defaults to ``3``.
    ny (int, optional): The number of Y coordinate values required to perform
        the interpolation. Defaults to ``3``.
    fitting_model (pyinterp.core.FittingModel, optional): Type of interpolation
        to be performed. Defaults to
        :py:data:`pyinterp.core.FittingModel.CSpline`
    boundary (pyinterp.core.AxisBoundary, optional): Type of axis boundary
        management. Defaults to
        :py:data:`pyinterp.core.AxisBoundary.Undef`
    bounds_error (bool, optional): If True, when interpolated values are
        requested outside of the domain of the input axes (x,y), a ValueError
        is raised. If False, then value is set to NaN.
    num_threads (int, optional): The number of threads to use for the
        computation. If 0 all CPUs are used. If 1 is given, no parallel
        computing code is used at all, which is useful for debugging.
        Defaults to ``0``.
Return:
    numpy.ndarray: Values interpolated
  )__doc__")
            .c_str());
}

void init_spline(py::module& m) {
  py::enum_<pyinterp::FittingModel>(m, "FittingModel", R"__doc__(
Spline fitting model
)__doc__")
      .value("Linear", pyinterp::FittingModel::kLinear,
             "*Linear interpolation*.")
      .value("Polynomial", pyinterp::FittingModel::kPolynomial,
             "*Polynomial interpolation*.")
      .value("CSpline", pyinterp::FittingModel::kCSpline,
             "*Cubic spline with natural boundary conditions*.")
      .value("CSplinePeriodic", pyinterp::FittingModel::kCSplinePeriodic,
             "*Cubic spline with periodic boundary conditions*.")
      .value("Akima", pyinterp::FittingModel::kAkima,
             "*Non-rounded Akima spline with natural boundary conditions*.")
      .value("AkimaPeriodic", pyinterp::FittingModel::kAkimaPeriodic,
             "*Non-rounded Akima spline with periodic boundary conditions*.")
      .value(
          "Steffen", pyinterp::FittingModel::kSteffen,
          "*Steffen’s method guarantees the monotonicity of data points. the "
          "interpolating function between the given*.");

  implement_spline<double>(m, "Float64");
  implement_spline<float>(m, "Float32");
  implement_spline_3d<double, double>(m, "", "Float64");
  implement_spline_3d<double, int64_t>(m, "Temporal", "Float64");
  implement_spline_3d<float, double>(m, "", "Float32");
  implement_spline_3d<float, int64_t>(m, "Temporal", "Float32");
  implement_spline_4d<double, double>(m, "", "Float64");
  implement_spline_4d<double, int64_t>(m, "Temporal", "Float64");
  implement_spline_4d<float, double>(m, "", "Float32");
  implement_spline_4d<float, int64_t>(m, "Temporal", "Float32");
}