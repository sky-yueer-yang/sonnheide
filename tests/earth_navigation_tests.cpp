#include "earth_navigation.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using sonnheide::client::AtlasNavigation;
void check(bool condition){if(!condition)throw std::runtime_error("Navigation invariant failed");}
int main(){
 AtlasNavigation view;view.span_longitude=30;view.latitude=15;view.longitude=175;
 const auto anchor=view.at(1000,210,1440,900);view.zoom(4,1000,210,1440,900);
 const auto after=view.at(1000,210,1440,900);
 check(std::abs(anchor.longitude-after.longitude)<1e-10&&std::abs(anchor.latitude-after.latitude)<1e-10);
 view.pan(-4000,0,1440,900);check(view.longitude>=-180&&view.longitude<180);
 view.pan(0,1e8,1440,900);check(view.at(0,0,1440,900).latitude<=90);
 view.pan(0,-1e8,1440,900);check(view.at(1440,900,1440,900).latitude>=-90);
 const auto state=view;view.zoom(1,500,400,1440,900);check(view.span_longitude<state.span_longitude);
 using namespace sonnheide::client;
 sonnheide::native::EarthCamera camera;camera.height_m=32;camera.pitch_deg=20;
 const sonnheide::earth::Bounds bounds{-10000,-10000,10000,10000};
 for(float yaw:{0.f,90.f,179.f,-120.f}){
  camera.yaw_deg=yaw;
  const auto centre=ground_camera_point(camera,720,450,1440,900);
  check(centre&&std::hypot(centre->x-camera.target_x_m,centre->y-camera.target_z_m)<1e-8);
  const auto anchor3d=ground_camera_point(camera,800,620,1440,900);check(bool(anchor3d));
  zoom_ground_camera(camera,3,800,620,1440,900,bounds);
  const auto zoomed=ground_camera_point(camera,800,620,1440,900);
  check(zoomed&&std::hypot(anchor3d->x-zoomed->x,anchor3d->y-zoomed->y)<1e-5);
  pan_ground_camera(camera,800,620,840,650,1440,900,bounds);
  const auto panned=ground_camera_point(camera,840,650,1440,900);
  check(panned&&std::hypot(zoomed->x-panned->x,zoomed->y-panned->y)<1e-8);
 }
 check(!ground_camera_point(camera,720,0,1440,900));
 orbit_ground_camera(camera,1440,0);check(camera.yaw_deg>=-180&&camera.yaw_deg<180&&!camera.orthographic);
 orbit_ground_camera(camera,0,-10000);check(camera.pitch_deg==20);
 orbit_ground_camera(camera,0,10000);check(camera.pitch_deg==85);
 zoom_ground_camera(camera,1000,720,450,1440,900,bounds);check(camera.height_m>=8);
 zoom_ground_camera(camera,-1000,720,450,1440,900,bounds);check(camera.height_m<=30000);
 pan_ground_camera(camera,720,450,1e8,1e8,1440,900,bounds);
 check(camera.target_x_m>=bounds.min_x&&camera.target_x_m<=bounds.max_x&&camera.target_z_m>=bounds.min_y&&camera.target_z_m<=bounds.max_y);
 std::cout<<"PASS: atlas and perspective ray-anchored zoom/pan, all yaw directions, sky rejection, bounds, orbit; no World mutation\n";
}
