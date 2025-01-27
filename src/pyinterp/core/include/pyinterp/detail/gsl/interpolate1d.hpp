// Copyright (c) 2022 CNES
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.
#pragma once
#include <gsl/gsl_spline.h>

#include <Eigen/Core>
#include <functional>
#include <memory>

#include "pyinterp/detail/gsl/accelerator.hpp"

namespace pyinterp::detail::gsl {

/// Interpolate a 1-D function
class Interpolate1D {
 public:
  /// Interpolate a 1-D function
  ///
  /// @param size Size of workspace
  /// @param type fitting model
  /// @param acc Accelerator
  Interpolate1D(const size_t size, const gsl_interp_type* type, Accelerator acc)
      : workspace_(
            std::unique_ptr<gsl_spline, std::function<void(gsl_spline*)>>(
                gsl_spline_alloc(type, size),
                [](gsl_spline* ptr) { gsl_spline_free(ptr); })),
        acc_(std::move(acc)) {}

  /// Returns the name of the interpolation type used
  [[nodiscard]] inline auto name() const noexcept -> std::string {
    return gsl_spline_name(workspace_.get());
  }

  /// Return the minimum number of points required by the interpolation
  [[nodiscard]] inline auto min_size() const noexcept -> size_t {
    return gsl_spline_min_size(workspace_.get());
  }

  /// Return the interpolated value of y for a given point x
  [[nodiscard]] inline auto interpolate(const Eigen::VectorXd& xa,
                                        const Eigen::VectorXd& ya,
                                        const double x) -> double {
    init(xa, ya);
    return gsl_spline_eval(workspace_.get(), x, acc_);
  }

  /// Return the derivative d of an interpolated function for a given point x
  [[nodiscard]] inline auto derivative(const Eigen::VectorXd& xa,
                                       const Eigen::VectorXd& ya,
                                       const double x) -> double {
    init(xa, ya);
    return gsl_spline_eval_deriv(workspace_.get(), x, acc_);
  }

  /// Return the second derivative d of an interpolated function for a given
  /// point x
  [[nodiscard]] inline auto second_derivative(const Eigen::VectorXd& xa,
                                              const Eigen::VectorXd& ya,
                                              const double x) -> double {
    init(xa, ya);
    return gsl_spline_eval_deriv2(workspace_.get(), x, acc_);
  }

  /// Return the numerical integral result of an interpolated function over the
  /// range [a, b],
  [[nodiscard]] inline auto integral(const Eigen::VectorXd& xa,
                                     const Eigen::VectorXd& ya, const double a,
                                     const double b) -> double {
    init(xa, ya);
    return gsl_spline_eval_integ(workspace_.get(), a, b, acc_);
  }

 private:
  std::unique_ptr<gsl_spline, std::function<void(gsl_spline*)>> workspace_;
  Accelerator acc_;

  /// Initializes the interpolation object
  inline auto init(const Eigen::VectorXd& xa,
                   const Eigen::VectorXd& ya) noexcept -> void {
    acc_.reset();
    gsl_spline_init(workspace_.get(), xa.data(), ya.data(), xa.size());
  }
};

}  // namespace pyinterp::detail::gsl
