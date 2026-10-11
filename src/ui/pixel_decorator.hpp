#pragma once
#include <RmlUi/Core/Vertex.h>
#include <RmlUi/Core/FontGlyph.h>
#include <array>

namespace sonnheide::ui {
// Original code-native whole-raster-pixel silhouettes and translucent frames.
struct PixelMesh { Rml::Vector<Rml::Vertex> vertices; Rml::Vector<int> indices; };
PixelMesh pixel_mesh(float width,float height,float step,float shadow,Rml::Colourb fill,Rml::Colourb shade);
// Actual glyph effects use the loaded Regular face: square white silhouettes
// and wordmark weight, directional CJK pigment that preserves narrow counters.
PixelMesh pixel_rim_mesh(float width,float height,float edge,float corner,float depth,Rml::Colourb fill,Rml::Colourb outline,Rml::Colourb shade,float gap_start=0,float gap_width=0);
PixelMesh pixel_tab_mesh(float width,float height,float edge,float shoulder,float neck,Rml::Colourb fill,Rml::Colourb outline,Rml::Colourb join);
// The same original material registry drives native texture loading and UI
// verification. Material faces retain one logical texel per dp at every size;
// UVs are split at tile boundaries instead of stretching a tiny flat picture.
struct PhysicalMaterialDefinition { const char* name; const char* texture_source; int width,height; };
const std::array<PhysicalMaterialDefinition,9>& physical_material_definitions();
// For tabs `depth` is the open marble neck height. Dock gaps remove the full
// six-dp neck, independently of their three-dp dark rim, so paint occurs once.
PixelMesh pixel_material_mesh(float width,float height,float texel_scale,float edge,float corner,float depth,Rml::Colourb tint,bool tab=false,float gap_start=0,float gap_width=0);
void pixel_glyph_dilate(Rml::byte* destination,Rml::Vector2i size,int stride,const Rml::FontGlyph& glyph,int radius);
void pixel_glyph_bolden(Rml::byte* destination,Rml::Vector2i size,int stride,const Rml::FontGlyph& glyph,int width);
void register_pixel_decorator();
}
