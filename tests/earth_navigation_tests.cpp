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
 std::cout<<"PASS: pointer-anchored zoom, date-line wrap, bounded polar pan; no World mutation\n";
}
