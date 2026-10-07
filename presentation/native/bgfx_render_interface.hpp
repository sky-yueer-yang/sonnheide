#pragma once
#include <RmlUi/Core.h>
#include <bgfx/bgfx.h>
#include <filesystem>
#include <memory>
namespace sonnheide::native {
class BgfxRenderInterface final : public Rml::RenderInterface {
public:
 explicit BgfxRenderInterface(void* native_window,const std::filesystem::path& resource_root,int pixel_width,int pixel_height);
 ~BgfxRenderInterface() override;
 void begin_frame(int logical_width,int logical_height,int pixel_width,int pixel_height);
 void begin_ui_frame();
 void end_frame();
 bool prepare_image(const std::filesystem::path& path);
 void draw_image(const std::filesystem::path& path,float x,float y,float width,float height,float opacity=1.f);
 void draw_menu_shade(float width,float height);
 void draw_compass(float center_x,float center_y,float instrument_size,float logo_size);
 void RenderGeometry(Rml::Vertex*,int,int*,int,Rml::TextureHandle,const Rml::Vector2f&) override;
 Rml::CompiledGeometryHandle CompileGeometry(Rml::Vertex*,int,int*,int,Rml::TextureHandle) override;
 void RenderCompiledGeometry(Rml::CompiledGeometryHandle,const Rml::Vector2f&) override;
 void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle) override;
 void EnableScissorRegion(bool) override;
 void SetScissorRegion(int,int,int,int) override;
 bool LoadTexture(Rml::TextureHandle&,Rml::Vector2i&,const Rml::String&) override;
 bool GenerateTexture(Rml::TextureHandle&,const Rml::byte*,const Rml::Vector2i&) override;
 void ReleaseTexture(Rml::TextureHandle) override;
 void SetTransform(const Rml::Matrix4f*) override;
 const char* renderer_name() const;
private:
 struct Impl; std::unique_ptr<Impl> impl_;
};
}
