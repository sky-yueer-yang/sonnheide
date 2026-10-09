#include "earth_navigation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace sonnheide::client {
namespace {
double wrap(double x) { return x-360*std::floor((x+180)/360); }
double lat_span(const AtlasNavigation& view,double width,double height) {
 return std::min(180.,view.span_longitude*height/width);
}
void size(double w,double h) { if(!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0) throw std::invalid_argument("Invalid map viewport"); }
}
earth::LonLat AtlasNavigation::at(double x,double y,double w,double h) const {
 size(w,h); return {wrap(longitude+(x/w-.5)*span_longitude),latitude+(.5-y/h)*lat_span(*this,w,h)};
}
void AtlasNavigation::pan(double dx,double dy,double w,double h) {
 size(w,h); longitude=wrap(longitude-dx*span_longitude/w);
 const double span=lat_span(*this,w,h);
 latitude=std::clamp(latitude+dy*span/h,-90+span*.5,90-span*.5);
}
void AtlasNavigation::zoom(double steps,double x,double y,double w,double h) {
 size(w,h); if(!std::isfinite(steps)||!std::isfinite(x)||!std::isfinite(y))return;
 const auto anchor=at(x,y,w,h);
 span_longitude=std::clamp(span_longitude*std::exp(-std::clamp(steps,-20.,20.)*.18),.002,360.);
 longitude=wrap(anchor.longitude-(x/w-.5)*span_longitude);
 const auto span=lat_span(*this,w,h);
 latitude=std::clamp(anchor.latitude-(.5-y/h)*span,-90+span*.5,90-span*.5);
}
namespace {
constexpr double radians=3.14159265358979323846/180;
void bound_camera(native::EarthCamera& camera,earth::Bounds b) {
 camera.target_x_m=std::clamp(camera.target_x_m,b.min_x,b.max_x);
 camera.target_z_m=std::clamp(camera.target_z_m,b.min_y,b.max_y);
}
}
std::optional<earth::Point> ground_camera_point(const native::EarthCamera& camera,double x,double y,double w,double h) {
 size(w,h);
 if(!std::isfinite(x)||!std::isfinite(y))return {};
 const double pitch=std::clamp(double(camera.pitch_deg),20.,89.)*radians,yaw=camera.yaw_deg*radians;
 const double sp=std::sin(pitch),cp=std::cos(pitch),sy=std::sin(yaw),cy=std::cos(yaw);
 const double horizontal=camera.height_m/std::tan(pitch);
 double ox=camera.target_x_m+horizontal*sy,oy=camera.height_m,oz=camera.target_z_m-horizontal*cy;
 double rx=-sy*cp,ry=-sp,rz=cy*cp;
 const double px=2*x/w-1,py=1-2*y/h;
 if(camera.orthographic){
  const double ux=px*camera.height_m*w/h,uy=py*camera.height_m;
  ox+=ux*cy-uy*sy*sp;oy+=uy*cp;oz+=ux*sy+uy*cy*sp;
 }else{
  const double sx=px*(w/h)*std::tan(25*radians),sz=py*std::tan(25*radians);
  rx+=sx*cy-sz*sy*sp;ry+=sz*cp;rz+=sx*sy+sz*cy*sp;
 }
 if(ry>=-1e-8)return {}; // Sky has no ground anchor; never invent one at infinity.
 const double t=-oy/ry;
 if(t<0||!std::isfinite(t))return {};
 return earth::Point{ox+t*rx,oz+t*rz};
}
native::EarthCamera initial_ground_camera(const earth::EarthWorldDefinition& world) {
 native::EarthCamera camera;camera.height_m=32;camera.pitch_deg=20;
 const auto bounds=world.domain_bounds(),selection=world.selection_bounds();const auto& cfg=world.config();
 double closest_land=1e100,closest_shore=1e100;earth::Point land{},shore{};
 const double margin=cfg.closure_band_m+cfg.coast_width_m*2;
 for(std::uint32_t z=0;z<world.rows();++z)for(std::uint32_t x=0;x<world.columns();++x){
  earth::Point p{bounds.min_x+(x+.5)*world.cell_width_m(),bounds.min_y+(z+.5)*world.cell_height_m()};
  if(p.x<selection.min_x+margin||p.x>selection.max_x-margin||p.y<selection.min_y+margin||p.y>selection.max_y-margin)continue;
  const auto s=world.sample(p);if(!s.source_land||!s.land)continue;
  const double distance=std::hypot(p.x,p.y);
  if(distance<closest_land){closest_land=distance;land=p;}
  if(s.height_m>=cfg.land_height_m*.75&&s.coast_distance_m<cfg.coast_width_m*2&&distance<closest_shore){closest_shore=distance;shore=p;}
 }
 const auto target=closest_shore<1e100?shore:land;camera.target_x_m=target.x;camera.target_z_m=target.y;
 // Place the eye inland and look towards genuine water when a source shore exists.
 if(closest_shore<1e100){
  double nearest=1e100;
  for(int i=0;i<16;++i){double a=i*3.14159265358979323846/8;const double dx=std::sin(a)*cfg.coast_width_m*3,dz=std::cos(a)*cfg.coast_width_m*3;
   const auto s=world.sample({target.x+dx,target.y+dz});if(s.land)continue;
   if(s.coast_distance_m<nearest){nearest=s.coast_distance_m;camera.yaw_deg=float(std::atan2(-dx,dz)/radians);}
  }
 }
 return camera;
}
native::EarthCamera overview_ground_camera(const earth::EarthWorldDefinition& world) {
 native::EarthCamera camera;camera.height_m=float(std::max(world.config().width_m,world.config().height_m)*1.6);camera.pitch_deg=65;
 return camera;
}
void pan_ground_camera(native::EarthCamera& camera,double ax,double ay,double bx,double by,double w,double h,earth::Bounds bounds) {
 const auto before=ground_camera_point(camera,ax,ay,w,h),after=ground_camera_point(camera,bx,by,w,h);
 if(before&&after){camera.target_x_m+=before->x-after->x;camera.target_z_m+=before->y-after->y;bound_camera(camera,bounds);}
}
void zoom_ground_camera(native::EarthCamera& camera,double steps,double x,double y,double w,double h,earth::Bounds bounds) {
 if(!std::isfinite(steps))return;
 const auto before=ground_camera_point(camera,x,y,w,h);
 camera.height_m=float(std::clamp(camera.height_m*std::exp(-std::clamp(steps,-20.,20.)*.15),8.,30000.));
 const auto after=ground_camera_point(camera,x,y,w,h);
 if(before&&after){camera.target_x_m+=before->x-after->x;camera.target_z_m+=before->y-after->y;bound_camera(camera,bounds);}
}
void orbit_ground_camera(native::EarthCamera& camera,double dx,double dy) {
 if(!std::isfinite(dx)||!std::isfinite(dy))return;
 camera.orthographic=false;
 const double yaw=camera.yaw_deg+dx*.25;camera.yaw_deg=float(yaw-360*std::floor((yaw+180)/360));
 camera.pitch_deg=float(std::clamp(camera.pitch_deg+dy*.2,20.,85.));
}
}
