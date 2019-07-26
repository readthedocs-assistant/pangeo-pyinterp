// Copyright (c) 2019 CNES
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.
#pragma once
#include "pyinterp/detail/thread.hpp"
#include "pyinterp/detail/math.hpp"
#include "pyinterp/grid.hpp"
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <Eigen/Core>
#include <pybind11/eigen.h>
#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace pyinterp {
namespace detail {

/// Calculate the zonal average in x direction
///
/// @param grid The grid to be processed.
/// @param mask Matrix describing the undefined pixels of the grid providedNaN
/// NumberReplaces all missing (_FillValue) values in a grid with values derived
/// from solving Poisson's equation via relaxation. of threads used for the
/// calculation
///
/// @param grid
template <typename Type>
void set_zonal_average(
    pybind11::EigenDRef<Eigen::Matrix<Type, Eigen::Dynamic, Eigen::Dynamic>>&
        grid,
    Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic>& mask,
    const size_t num_threads) {
  // Captures the detected exceptions in the calculation function
  // (only the last exception captured is kept)
  auto except = std::exception_ptr(nullptr);

  detail::dispatch(
      [&](size_t y_start, size_t y_end) {
        try {
          // Calculation of longitude band means.
          for (size_t iy = y_start; iy < y_end; ++iy) {
            auto acc = boost::accumulators::accumulator_set<
                Type,
                boost::accumulators::stats<boost::accumulators::tag::count,
                                           boost::accumulators::tag::mean>>();
            for (size_t ix = 0; ix < grid.rows(); ++ix) {
              if (!mask(ix, iy)) {
                acc(grid(ix, iy));
              }
            }

            // The masked value is replaced by the average of the longitude band
            // if it is defined; otherwise it is replaced by zero.
            auto first_guess = boost::accumulators::count(acc)
                                   ? boost::accumulators::mean(acc)
                                   : Type(0);
            for (size_t ix = 0; ix < grid.rows(); ++ix) {
              if (mask(ix, iy)) {
                grid(ix, iy) = first_guess;
              }
            }
          }
        } catch (...) {
          except = std::current_exception();
        }
      },
      grid.cols(), num_threads);

  if (except != nullptr) {
    std::rethrow_exception(except);
  }
}

/// Replaces all missing values in a grid with values derived from solving
/// Poisson's equation.
///
/// @param grid The grid to be processed
/// @param is_circle True if the X axis of the grid defines a circle.
/// @param relaxation Relaxation constant
/// @return maximum residual value
template <typename Type>
Type poisson_grid_fill(
    pybind11::EigenDRef<Eigen::Matrix<Type, Eigen::Dynamic, Eigen::Dynamic>>&
        grid,
    Eigen::Matrix<bool, -1, -1>& mask, const bool is_circle,
    const Type relaxation, const size_t num_threads) {
  // Maximum residual values for each thread.
  std::vector<Type> max_residuals(num_threads);

  // Shape of the grid
  auto x_size = grid.rows();
  auto y_size = grid.cols();

  // Captures the detected exceptions in the calculation function
  // (only the last exception captured is kept)
  auto except = std::exception_ptr(nullptr);

  // Thread worker responsible for processing several strips along the y-axis of
  // the grid.
  auto worker = [&](int64_t y_start, int64_t y_end,
                    Type* max_residual) -> void {
    // Modifies the value of a masked pixel.
    auto cell_fill = [&grid, &relaxation, &max_residual](
                         int64_t ix0, int64_t ix, int64_t ix1, int64_t iy0,
                         int64_t iy, int64_t iy1) {
      auto residual = (Type(0.25) * (grid(ix0, iy) + grid(ix1, iy) +
                                     grid(ix, iy0) + grid(ix, iy1)) -
                       grid(ix, iy)) *
                      relaxation;
      grid(ix, iy) += residual;
      *max_residual = std::max(*max_residual, std::fabs(residual));
    };

    // Initialization of the maximum value of the residuals of the treated
    // strips.
    *max_residual = Type(0);

    try {
      for (auto iy = y_start; iy < y_end; ++iy) {
        // Previous (iy0) and next (iy1) pixel indices on the Y axis. On the
        // edge we consider the mirror value.
        auto iy0 = iy == 0 ? 1 : iy - 1;
        auto iy1 = iy == y_size - 1 ? y_size - 2 : iy + 1;

        // Processing of the strips along the X axis, ignoring the edges.
        for (auto ix = 1; ix < x_size - 1; ++ix) {
          if (mask(ix, iy)) {
            cell_fill(ix - 1, ix, ix + 1, iy0, iy, iy1);
          }
        }

        // Edge handling along the X axis of the grid. We consider the mirror
        // value if the X axis does not define a circle, otherwise the previous
        // or next value of the circle.
        if (mask(0, iy)) {
          cell_fill(is_circle ? x_size - 1 : 1, 0, 1, iy0, iy, iy1);
        }
        if (mask(x_size - 1, iy)) {
          cell_fill(x_size - 2, x_size - 1, is_circle ? 0 : x_size - 2, iy0, iy,
                    iy1);
        }
      }
    } catch (...) {
      except = std::current_exception();
    }
  };

  if (num_threads == 1) {
    worker(0, y_size, &max_residuals[0]);
  } else {
    std::vector<std::thread> threads(num_threads);
    int64_t start = 0, shift = y_size / num_threads, index = 0;

    // Launch and join threads
    for (auto it = std::begin(threads); it != std::end(threads) - 1; ++it) {
      *it = std::thread(worker, start, start + shift, &max_residuals[index++]);
      start += shift;
    }
    threads.back() =
        std::thread(worker, start, y_size - 1, &max_residuals[index]);
    for (auto&& item : threads) {
      item.join();
    }
  }
  if (except != nullptr) {
    std::rethrow_exception(except);
  }
  return *std::max_element(max_residuals.begin(), max_residuals.end());
}

}  // namespace detail

namespace fill {

/// Type of first guess grid.
enum FirstGuess {
  kZero,          //!< Use 0.0 as an initial guess
  kZonalAverage,  //!< Use zonal average in x direction
};

/// Replaces all NaN values in a grid with values derived from solving Poisson's
/// equation via relaxation.
///
/// @param grid The grid to be processed
/// @param is_circle True if the X axis of the grid defines a circle.
/// @param max_iterations Maximum number of iterations to be used by relaxation.
/// @param epsilon Tolerance for ending relaxation before the maximum number of
/// iterations limit.
/// @param relaxation Relaxation constant
/// @param num_threads The number of threads to use for the computation. If 0
/// all CPUs are used. If 1 is given, no parallel computing code is used at all,
/// which is useful for debugging.
/// @return A tuple containing the number of iterations performed and the
/// maximum residual value.
template <typename Type>
std::tuple<size_t, Type> poisson(
    pybind11::EigenDRef<Eigen::Matrix<Type, Eigen::Dynamic, Eigen::Dynamic>>&
        grid,
    const FirstGuess first_guess, const bool is_circle,
    const size_t max_iterations, const double epsilon, const double relaxation,
    size_t num_threads) {
  /// If the grid doesn't have an undefined value, this routine has nothing more
  /// to do.
  if (!grid.hasNaN()) {
    return std::make_tuple(0, Type(0));
  }

  /// Calculation of the maximum number of threads if the user chooses.
  if (num_threads == 0) {
    num_threads = std::thread::hardware_concurrency();
  }

  /// Calculation of the position of the undefined values on the grid.
  auto mask =
      Eigen::Matrix<bool, Eigen::Dynamic, Eigen::Dynamic>(grid.array().isNaN());

  /// Calculation of the first guess with the chosen method
  switch (first_guess) {
    case FirstGuess::kZero:
      grid = (mask.array()).select(0, grid);
      break;
    case FirstGuess::kZonalAverage:
      detail::set_zonal_average(grid, mask, num_threads);
      break;
    default:
      throw std::invalid_argument("Invalid guess type: " +
                                  std::to_string(first_guess));
  }

  // Initialization of the function results.
  size_t iteration = 0;
  Type max_residual = 0;

  for (size_t it = 0; it < max_iterations; ++it) {
    ++iteration;
    max_residual = detail::poisson_grid_fill<Type>(grid, mask, is_circle,
                                                   relaxation, num_threads);
    if (max_residual < epsilon) {
      break;
    }
  }
  return std::make_tuple(iteration, max_residual);
}

/// Fills undefined values using a locally weighted regression function or
/// LOESS. The weight function used for LOESS is the tri-cube weight function,
/// w(x)=(1-|d|^{3})^{3}
///
/// @param grid Grid Function on a uniform 2-dimensional grid to be filled.
/// @param nx Number of points of the half-window to be taken into account along
/// the longitude axis.
/// @param nx Number of points of the half-window to be taken into account along
/// the latitude axis.
/// @param num_threads The number of threads to use for the computation. If 0
/// all CPUs are used. If 1 is given, no parallel computing code is used at all,
/// which is useful for debugging.
/// @return The grid will have all the NaN filled with extrapolated values.
template <typename Type>
pybind11::array_t<Type> loess(const Grid2D<Type>& grid, const size_t nx,
                              const size_t ny, const size_t num_threads) {
  auto result = pybind11::array_t<Type>(
      pybind11::array::ShapeContainer{grid.x()->size(), grid.y()->size()});
  auto _result = result.template mutable_unchecked<2>();

  // Captures the detected exceptions in the calculation function
  // (only the last exception captured is kept)
  auto except = std::exception_ptr(nullptr);

  auto worker = [&](const size_t start, const size_t end) {
    try {
      for (size_t ix = start; ix < end; ++ix) {
        auto x = (*grid.x())(ix);

        for (size_t iy = 0; iy < grid.y()->size(); ++iy) {
          auto z = grid.value(ix, iy);

          // If the current value is masked.
          if (std::isnan(z)) {
            auto y = (*grid.y())(iy);

            // Reading the coordinates of the window around the masked point.
            auto x_frame = grid.x()->find_indexes(x, nx, Axis::kSym);
            auto y_frame = grid.y()->find_indexes(y, ny, Axis::kSym);

            // Initialization of values to calculate the extrapolated value.
            auto value = Type(0);
            auto weight = Type(0);

            // For all the coordinates of the frame.
            for (auto wx : x_frame) {
              for (auto wy : y_frame) {
                auto zi = grid.value(wx, wy);

                // If the value is not masked, its weight is calculated from the
                // tri-cube weight function
                if (!std::isnan(zi)) {
                  auto d =
                      std::sqrt(detail::math::sqr((((*grid.x())(wx)-x)) / nx) +
                                detail::math::sqr((((*grid.y())(wy)-y)) / ny));
                  auto wi =
                      d <= 1 ? std::pow((1.0 - std::pow(d, 3.0)), 3.0) : 0.0;
                  value += wi * zi;
                  weight += wi;
                }
              }
            }

            // Finally, we calculate the extrapolated value if possible,
            // otherwise we will recopy the masked original value.
            if (weight != 0) {
              z = value / weight;
            }
          }
          _result(ix, iy) = z;
        }
      }
    } catch (...) {
      except = std::current_exception();
    }
  };

  {
    pybind11::gil_scoped_release release;
    detail::dispatch(worker, grid.x()->size(), num_threads);
  }
  return result;
}

}  // namespace fill
}  // namespace pyinterp