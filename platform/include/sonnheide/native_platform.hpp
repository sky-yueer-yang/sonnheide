#pragma once
#include <SDL3/SDL.h>
#include <RmlUi/Core.h>
#include <filesystem>
#include <vector>
#include <cstdint>
namespace sonnheide::native {
struct ImagePixels { int width=0,height=0; std::vector<std::uint8_t> rgba; };
ImagePixels decode_bimg_image(const std::filesystem::path& path);
ImagePixels load_image(const std::filesystem::path& path);
class PlatformWindow {
public:
 PlatformWindow(int width=1280,int height=800); ~PlatformWindow();
 PlatformWindow(const PlatformWindow&)=delete; PlatformWindow& operator=(const PlatformWindow&)=delete;
 SDL_Window* window() const {return window_;}
 void* native_handle() const;
 void dimensions(int& logical_width,int& logical_height,int& pixel_width,int& pixel_height) const;
 void fullscreen(bool enabled); bool is_fullscreen() const;
 bool visible() const;
private: SDL_Window* window_=nullptr;
};
class UnicodeFileInterface final : public Rml::FileInterface {
public:
 Rml::FileHandle Open(const Rml::String& path) override;
 void Close(Rml::FileHandle file) override;
 std::size_t Read(void* buffer,std::size_t size,Rml::FileHandle file) override;
 bool Seek(Rml::FileHandle file,long offset,int origin) override;
 std::size_t Tell(Rml::FileHandle file) override;
};
class SdlSystemInterface final : public Rml::SystemInterface {
public:
 double GetElapsedTime() override;
 bool LogMessage(Rml::Log::Type type,const Rml::String& message) override;
 void SetClipboardText(const Rml::String& text) override;
 void GetClipboardText(Rml::String& text) override;
};
void process_event(const SDL_Event& event,Rml::Context& context);
}
