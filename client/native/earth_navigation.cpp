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
}
