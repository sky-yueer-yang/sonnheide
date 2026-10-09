#pragma once
#include "sonnheide/earth_world.hpp"
#include "earth_camera.hpp"
#include <optional>
namespace sonnheide::client {
// Global navigation is presentation only; it never changes worldScale or a World.
struct AtlasNavigation {
 double longitude{14.382}, latitude{40.626}, span_longitude{360};
 earth::LonLat at(double x, double y, double width, double height) const;
 void pan(double dx, double dy, double width, double height);
 void zoom(double steps, double x, double y, double width, double height);
};
// Same 50-degree perspective and yaw/pitch basis as EarthRenderer. Coordinates
// are viewport-local logical pixels, so Retina density does not change motion.
std::optional<earth::Point> ground_camera_point(const native::EarthCamera&,double x,double y,double width,double height);
native::EarthCamera initial_ground_camera(const earth::EarthWorldDefinition&);
native::EarthCamera overview_ground_camera(const earth::EarthWorldDefinition&);
void pan_ground_camera(native::EarthCamera&,double from_x,double from_y,double to_x,double to_y,double width,double height,earth::Bounds);
void zoom_ground_camera(native::EarthCamera&,double steps,double x,double y,double width,double height,earth::Bounds);
void orbit_ground_camera(native::EarthCamera&,double dx,double dy);
}
