#include <sonnheide/native_platform.hpp>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <iostream>
namespace sonnheide::native {
Rml::FileHandle UnicodeFileInterface::Open(const Rml::String& name) {
 try {
  auto path=std::filesystem::path(std::u8string(name.begin(),name.end()));
#ifdef _WIN32
  auto* file=_wfopen(path.c_str(),L"rb");
#else
  auto* file=std::fopen(path.c_str(),"rb");
#endif
  return reinterpret_cast<Rml::FileHandle>(file);
 }catch(...){return 0;}
}
void UnicodeFileInterface::Close(Rml::FileHandle h){if(h)std::fclose(reinterpret_cast<std::FILE*>(h));}
std::size_t UnicodeFileInterface::Read(void* data,std::size_t size,Rml::FileHandle h){return h?std::fread(data,1,size,reinterpret_cast<std::FILE*>(h)):0;}
bool UnicodeFileInterface::Seek(Rml::FileHandle h,long offset,int origin){return h&&std::fseek(reinterpret_cast<std::FILE*>(h),offset,origin)==0;}
std::size_t UnicodeFileInterface::Tell(Rml::FileHandle h){if(!h)return 0;auto n=std::ftell(reinterpret_cast<std::FILE*>(h));return n>=0?std::size_t(n):0;}
PlatformWindow::PlatformWindow(int width,int height) {
 if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
 window_=SDL_CreateWindow("SONNHEIDE",width,height,SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
 if(!window_){SDL_Quit();throw std::runtime_error(SDL_GetError());}
 SDL_StartTextInput(window_);
}
PlatformWindow::~PlatformWindow(){if(window_){SDL_StopTextInput(window_);SDL_DestroyWindow(window_);}SDL_Quit();}
void* PlatformWindow::native_handle() const {
#ifdef _WIN32
 return SDL_GetPointerProperty(SDL_GetWindowProperties(window_),SDL_PROP_WINDOW_WIN32_HWND_POINTER,nullptr);
#elif defined(__APPLE__)
 return SDL_GetPointerProperty(SDL_GetWindowProperties(window_),SDL_PROP_WINDOW_COCOA_WINDOW_POINTER,nullptr);
#else
 return nullptr;
#endif
}
void PlatformWindow::dimensions(int& w,int& h,int& pw,int& ph)const {SDL_GetWindowSize(window_,&w,&h);SDL_GetWindowSizeInPixels(window_,&pw,&ph);}
void PlatformWindow::fullscreen(bool enabled){if(!SDL_SetWindowFullscreen(window_,enabled))throw std::runtime_error(SDL_GetError());}
bool PlatformWindow::is_fullscreen() const{return (SDL_GetWindowFlags(window_)&SDL_WINDOW_FULLSCREEN)!=0;}
bool PlatformWindow::visible()const{return (SDL_GetWindowFlags(window_)&(SDL_WINDOW_HIDDEN|SDL_WINDOW_MINIMIZED))==0;}
double SdlSystemInterface::GetElapsedTime(){return double(SDL_GetTicksNS())/1e9;}
bool SdlSystemInterface::LogMessage(Rml::Log::Type,const Rml::String& message){std::cerr<<"RmlUi: "<<message<<'\n';return true;}
void SdlSystemInterface::SetClipboardText(const Rml::String& text){SDL_SetClipboardText(text.c_str());}
void SdlSystemInterface::GetClipboardText(Rml::String& text){char* value=SDL_GetClipboardText();if(value){text=value;SDL_free(value);}}
namespace {
int modifiers(){auto m=SDL_GetModState();return ((m&SDL_KMOD_CTRL)?Rml::Input::KM_CTRL:0)|((m&SDL_KMOD_SHIFT)?Rml::Input::KM_SHIFT:0)|((m&SDL_KMOD_ALT)?Rml::Input::KM_ALT:0)|((m&SDL_KMOD_GUI)?Rml::Input::KM_META:0);}
Rml::Input::KeyIdentifier key(SDL_Keycode k){using namespace Rml::Input;
 if(k>=SDLK_A && k<=SDLK_Z)return KeyIdentifier(KI_A+int(k-SDLK_A));
 if(k>=SDLK_0 && k<=SDLK_9)return KeyIdentifier(KI_0+int(k-SDLK_0));
 switch(k){case SDLK_RETURN:return KI_RETURN;case SDLK_ESCAPE:return KI_ESCAPE;case SDLK_BACKSPACE:return KI_BACK;case SDLK_TAB:return KI_TAB;case SDLK_SPACE:return KI_SPACE;case SDLK_DELETE:return KI_DELETE;case SDLK_LEFT:return KI_LEFT;case SDLK_RIGHT:return KI_RIGHT;case SDLK_UP:return KI_UP;case SDLK_DOWN:return KI_DOWN;case SDLK_HOME:return KI_HOME;case SDLK_END:return KI_END;default:return KI_UNKNOWN;}}
}
void process_event(const SDL_Event& e,Rml::Context& c){auto m=modifiers();switch(e.type){
 case SDL_EVENT_MOUSE_MOTION:c.ProcessMouseMove(int(std::lround(e.motion.x*c.GetDensityIndependentPixelRatio())),int(std::lround(e.motion.y*c.GetDensityIndependentPixelRatio())),m);break;
 case SDL_EVENT_MOUSE_BUTTON_DOWN:c.ProcessMouseButtonDown(e.button.button==SDL_BUTTON_RIGHT?1:e.button.button==SDL_BUTTON_MIDDLE?2:0,m);break;
 case SDL_EVENT_MOUSE_BUTTON_UP:c.ProcessMouseButtonUp(e.button.button==SDL_BUTTON_RIGHT?1:e.button.button==SDL_BUTTON_MIDDLE?2:0,m);break;
 case SDL_EVENT_MOUSE_WHEEL:c.ProcessMouseWheel(-e.wheel.y,m);break;
 case SDL_EVENT_KEY_DOWN:c.ProcessKeyDown(key(e.key.key),m);break;
 case SDL_EVENT_KEY_UP:c.ProcessKeyUp(key(e.key.key),m);break;
 case SDL_EVENT_TEXT_INPUT:c.ProcessTextInput(e.text.text);break;
 case SDL_EVENT_WINDOW_MOUSE_LEAVE:c.ProcessMouseLeave();break;
 default:break;
}}
}
