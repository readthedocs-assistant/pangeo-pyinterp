// Copyright (c) 2022 CNES
//
// All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.
#pragma once
#include <pybind11/pybind11.h>

#include <boost/geometry/geometries/register/point.hpp>

#include "pyinterp/detail/geometry/point.hpp"
#include "pyinterp/geodetic/algorithm.hpp"
#include "pyinterp/geodetic/system.hpp"

namespace pyinterp::geodetic {

// Handle a point in a equatorial spherical coordinates system in degrees.
class Point : public detail::geometry::GeographicPoint2D<double> {
 public:
  /// Default constructor
  Point() noexcept = default;

  /// Build a new point with the coordinates provided.
  Point(const double lon, const double lat)
      : detail::geometry::GeographicPoint2D<double>(lon, lat) {}

  /// Get longitude value in degrees
  [[nodiscard]] inline auto lon() const -> double { return this->get<0>(); }

  /// Get latitude value in degrees
  [[nodiscard]] inline auto lat() const -> double { return this->get<1>(); }

  /// Set longitude value in degrees
  inline auto lon(double const v) -> void { this->set<0>(v); }

  /// Set latitude value in degrees
  inline auto lat(double const v) -> void { this->set<1>(v); }

  /// Calculate the distance between the two points
  [[nodiscard]] auto distance(const Point& other,
                              const DistanceStrategy strategy,
                              const std::optional<System>& wgs) const
      -> double {
    return geodetic::distance(*this, other, strategy, wgs);
  }

  /// Get a tuple that fully encodes the state of this instance
  [[nodiscard]] auto getstate() const -> pybind11::tuple {
    return pybind11::make_tuple(lon(), lat());
  }

  /// Create a new instance from a registered state of an instance of this
  /// object.
  static auto setstate(const pybind11::tuple& state) -> Point {
    if (state.size() != 2) {
      throw std::runtime_error("invalid state");
    }
    return {state[0].cast<double>(), state[1].cast<double>()};
  }

  /// Converts a Point into a string with the same meaning as that of this
  /// instance.
  [[nodiscard]] auto to_string() const -> std::string {
    std::stringstream ss;
    ss << boost::geometry::dsv(*this);
    return ss.str();
  }

  /// Returns true if the tow points are equal.
  auto operator==(const Point& other) const -> bool {
    return boost::geometry::equals(*this, other);
  }
};

}  // namespace pyinterp::geodetic

// BOOST specialization to accept pyinterp::geodectic::Point as a geometry
// entity
namespace boost::geometry::traits {

namespace pg = pyinterp::geodetic;

/// Coordinate tag
template <>
struct tag<pg::Point> {
  /// Typedef for type
  using type = point_tag;
};

/// Coordinate type
template <>
struct coordinate_type<pg::Point> {
  /// Typedef for type
  using type = double;
};

/// Coordinate system
template <>
struct coordinate_system<pg::Point> {
  /// Typedef for type
  using type = cs::geographic<degree>;
};

template <>
struct dimension<pg::Point> : boost::mpl::int_<2> {};

/// access struct defining with Cartesian
template <std::size_t I>
struct access<pg::Point, I> {
  /// Accessor to pointer.
  static auto get(pg::Point const& p) -> double { return p.template get<I>(); }

  /// Pointer setter.
  static void set(pg::Point& p, double const& v) {  // NOLINT
    p.template set<I>(v);
  }
};

}  // namespace boost::geometry::traits
