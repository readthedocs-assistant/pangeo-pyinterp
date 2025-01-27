#include "pyinterp/geodetic/box.hpp"

#include "pyinterp/geodetic/polygon.hpp"

namespace pyinterp::geodetic {

/// Calculate the area
auto Box::area(const std::optional<System>& wgs) const -> double {
  return static_cast<Polygon>(*this).area(wgs);
}

/// Calculate the distance between two boxes
auto Box::distance(const Box& other) const -> double {
  return static_cast<Polygon>(*this).distance(static_cast<Polygon>(other));
}

/// Calculate the distance between this instance and a point
[[nodiscard]] auto Box::distance(const Point& other) const -> double {
  return static_cast<Polygon>(*this).distance(other);
}

/// Converts this instance into a polygon
Box::operator Polygon() const {
  Polygon result;
  boost::geometry::convert(*this, result);
  return result;
}

}  // namespace pyinterp::geodetic