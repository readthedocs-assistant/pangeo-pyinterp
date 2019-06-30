// Copyright (c) 2019 CNES
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.
#pragma once
#include "pyinterp/axis.hpp"
#include "pyinterp/detail/broadcast.hpp"
#include <pybind11/numpy.h>

namespace pyinterp {

/// Cartesian Grid 2D
template <typename T, ssize_t Dimension = 2>
class Grid2D {
 public:
  /// Default constructor
  Grid2D(Axis x, Axis y, pybind11::array_t<T> z)
      : x_(std::move(x)),
        y_(std::move(y)),
        array_(std::move(z)),
        ptr_(array_.template unchecked<Dimension>()) {
    check_shape(0, x_, "x", "z", y_, "y", "z");
  }

  /// Default constructor
  Grid2D() = default;

  /// Default destructor
  virtual ~Grid2D() = default;

  /// Copy constructor
  ///
  /// @param rhs right value
  Grid2D(const Grid2D& rhs) = default;

  /// Move constructor
  ///
  /// @param rhs right value
  Grid2D(Grid2D&& rhs) noexcept = default;

  /// Copy assignment operator
  ///
  /// @param rhs right value
  Grid2D& operator=(const Grid2D& rhs) = default;

  /// Move assignment operator
  ///
  /// @param rhs right value
  Grid2D& operator=(Grid2D&& rhs) noexcept = default;

  /// Gets the X-Axis
  inline const Axis& x() const noexcept { return x_; }

  /// Gets the Y-Axis
  inline const Axis& y() const noexcept { return y_; }

  /// Gets values of the array to interpolate
  inline const pybind11::array_t<T>& array() const noexcept { return array_; }

  /// Pickle support: get state of this instance
  virtual pybind11::tuple getstate() const {
    return pybind11::make_tuple(x_.getstate(), y_.getstate(), array_);
  }

  /// Pickle support: set state of this instance
  static Grid2D setstate(const pybind11::tuple& tuple) {
    if (tuple.size() != 3) {
      throw std::runtime_error("invalid state");
    }
    return Grid2D(Axis::setstate(tuple[0].cast<pybind11::tuple>()),
                  Axis::setstate(tuple[1].cast<pybind11::tuple>()),
                  tuple[2].cast<pybind11::array_t<T>>());
  }

 protected:
  Axis x_;
  Axis y_;
  pybind11::array_t<T> array_;
  pybind11::detail::unchecked_reference<T, Dimension> ptr_;

  /// End of the recursive call of the function "check_shape"
  void check_shape(const size_t idx) {}

  /// Checking the shape of the array for each defined axis.
  template <typename... Args>
  void check_shape(const size_t idx, const Axis& axis, const std::string& x,
                   const std::string& y, Args... args) {
    if (axis.size() != array_.shape(idx)) {
      throw std::invalid_argument(
          x + ", " + y + " could not be broadcast together with shape (" +
          std::to_string(axis.size()) + ", ) " + detail::ndarray_shape(array_));
    }
    check_shape(idx + 1, args...);
  }
};

/// Cartesian Grid 3D
template <typename T>
class Grid3D : public Grid2D<T, 3> {
 public:
  /// Default constructor
  Grid3D(Axis x, Axis y, Axis z, pybind11::array_t<T> u)
      : Grid2D<T, 3>(std::move(x), std::move(y), std::move(u)),
        z_(std::move(z)) {
    this->check_shape(2, z_, "z", "u");
  }

  /// Gets the Y-Axis
  inline const Axis& z() const noexcept { return z_; }

  /// Pickle support: get state of this instance
  pybind11::tuple getstate() const final {
    return pybind11::make_tuple(this->x_.getstate(), this->y_.getstate(),
                                z_.getstate(), this->array_);
  }

  /// Pickle support: set state of this instance
  static Grid3D setstate(const pybind11::tuple& tuple) {
    if (tuple.size() != 4) {
      throw std::runtime_error("invalid state");
    }
    return Grid3D(Axis::setstate(tuple[0].cast<pybind11::tuple>()),
                  Axis::setstate(tuple[1].cast<pybind11::tuple>()),
                  Axis::setstate(tuple[2].cast<pybind11::tuple>()),
                  tuple[3].cast<pybind11::array_t<T>>());
  }

 protected:
  Axis z_;
};

}  // namespace pyinterp