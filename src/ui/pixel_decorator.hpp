#pragma once
#include <RmlUi/Core/Vertex.h>

namespace sonnheide::ui {
// Original code-native whole-raster-pixel silhouettes and translucent frames.
struct PixelMesh { Rml::Vector<Rml::Vertex> vertices; Rml::Vector<int> indices; };
PixelMesh pixel_mesh(float width,float height,float step,float shadow,Rml::Colourb fill,Rml::Colourb shade);
PixelMesh pixel_frame_mesh(float width,float height,float edge,float corner,Rml::Colourb fill,Rml::Colourb light,Rml::Colourb dark);
void register_pixel_decorator();
}
