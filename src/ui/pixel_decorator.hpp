#pragma once
#include <RmlUi/Core/Vertex.h>

namespace sonnheide::ui {
// Code-native filled whole-raster-pixel silhouettes, without strokes or frames.
struct PixelMesh { Rml::Vector<Rml::Vertex> vertices; Rml::Vector<int> indices; };
PixelMesh pixel_mesh(float width,float height,float step,float shadow,Rml::Colourb fill,Rml::Colourb shade);
void register_pixel_decorator();
}
