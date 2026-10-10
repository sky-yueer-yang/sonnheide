#pragma once
#include <RmlUi/Core/Vertex.h>

namespace sonnheide::ui {
// The same filled, axis-aligned mesh is used by the decorator and its geometry
// contract tests. There is no outline, stroke, border strip or gradient.
struct PixelMesh { Rml::Vector<Rml::Vertex> vertices; Rml::Vector<int> indices; };
PixelMesh pixel_mesh(float width,float height,float step,float shadow,Rml::Colourb fill,Rml::Colourb shade);
void register_pixel_decorator();
}
