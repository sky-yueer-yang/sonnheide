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
// Generic carved faces retain the eight non-stone material silhouettes.
PixelMesh pixel_material_mesh(float width,float height,float texel_scale,float edge,float corner,float depth,Rml::Colourb tint,bool tab=false,float gap_start=0,float gap_width=0);
// A dark stone face has sparse physical edge chips and short fractures rooted
// at its actual upper edge. `lip` adds a continuous six-dp raised top ledge
// followed by a two-dp shadow; broad low tabs join that ledge with shallow
// stepped trapezoid shoulders over the short face. detail_seed
// only chooses small edge details and never consumes simulation randomness.
PixelMesh pixel_stone_mesh(float width,float height,float texel_scale,float edge,float corner,float depth,Rml::Colourb tint,bool tab=false,bool lip=false,unsigned detail_seed=0);
// Only the open tab neck shares the dock's texture coordinates. texture_origin
// is the rounded tab padding origin relative to the rounded dock padding
// origin, in physical pixels. Faces keep their original local coordinates.
PixelMesh pixel_material_neck_mesh(float width,float height,float texel_scale,float neck,Rml::Vector2f texture_origin,Rml::Colourb tint);
void pixel_glyph_dilate(Rml::byte* destination,Rml::Vector2i size,int stride,const Rml::FontGlyph& glyph,int radius);
void pixel_glyph_bolden(Rml::byte* destination,Rml::Vector2i size,int stride,const Rml::FontGlyph& glyph,int width);
void register_pixel_decorator();
}
