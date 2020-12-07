// Copyright (c) 2020 CNES
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.
#pragma once
#include "pyinterp/detail/axis.hpp"
#include "pyinterp/grid.hpp"

namespace pyinterp {

/// Fitting model
enum FittingModel {
  kLinear,           //!< Linear interpolation
  kPolynomial,       //!< Polynomial interpolation
  kCSpline,          //!< Cubic spline with natural boundary conditions.
  kCSplinePeriodic,  //!< Cubic spline with periodic boundary conditions.
  kAkima,            //!< Non-rounded Akima spline with natural boundary
                     //!< conditions
  kAkimaPeriodic,    //!< Non-rounded Akima spline with periodic boundary
                     //!< conditions
  kSteffen           //!< Steffen’s method guarantees the monotonicity of
                     //!< the interpolating function between the given
                     //!< data points.
};

/// Spline gridded 2D interpolation
///
/// @tparam Type The type of data used by the numerical grid.
template <typename Type>
auto spline(const Grid2D<Type>& grid, const pybind11::array_t<double>& x,
            const pybind11::array_t<double>& y, size_t nx, size_t ny,
            FittingModel fitting_model, axis::Boundary boundary,
            bool bounds_error, size_t num_threads) -> pybind11::array_t<double>;
}  // namespace pyinterp