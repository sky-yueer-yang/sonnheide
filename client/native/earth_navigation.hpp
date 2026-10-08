#pragma once
#include "sonnheide/earth_world.hpp"
namespace sonnheide::client {
// Global navigation is presentation only; it never changes worldScale or a World.
struct AtlasNavigation {
 double longitude{14.382}, latitude{40.626}, span_longitude{360};
 earth::LonLat at(double x, double y, double width, double height) const;
 void pan(double dx, double dy, double width, double height);
 void zoom(double steps, double x, double y, double width, double height);
};
}
