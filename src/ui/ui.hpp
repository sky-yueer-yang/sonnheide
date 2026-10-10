#pragma once
#include <RmlUi/Core/EventListener.h>
#include <cstdint>
#include <array>
#include <map>
#include <filesystem>
#include <string>
#include <vector>
namespace Rml { class Context; class ElementDocument; class Event; }
namespace sonnheide::ui {
enum class Screen { MainMenu, Creation, World, Load };
enum class Locale { Chinese, English, German };
enum class CreationMode { Blank, Earth };
enum class BlankBase { Ocean, Soil };
enum class BottomSection { Observe, TerrainEcology, Life, Civilization, Construction, Economy, World, Settings };
enum class ActionKind {
    NewWorld, ContinueWorld, OpenLoad, CancelLoad, SelectSave, LoadSelected, SaveWorld, ReturnMenu, Exit,
    SetCreationMode, SetBlankBase, SetCreationSize, SetCreationName, SetCreationTheme,
    SetEarthBounds, MapZoom, MapPan, RequestPreview, CreateWorld, CancelCreation,
    OpenSettings, CloseSettings, OpenHelp, CloseHelp, SetLocale, SetFullscreen, SetReducedMotion, SetAudioVolume,
    SelectBottomSection, CameraHome, SetPaused, SetSpeed,
    OpenWorldInspector, CloseWorldInspector, SetWorldNameDraft, PreviewWorldName, CommitWorldName, CancelWorldName,
    SetAgeLight, SetAgeDarkness, SetAgeAutomatic, DismissError
};
// All application/core mutations leave the UI through this typed intent queue.
// generation carries draft/session identity; token carries the displayed preview.
struct Action {
    ActionKind kind = ActionKind::NewWorld;
    std::uint64_t generation = 0;
    std::uint64_t token = 0;
    std::string text;
    double a = 0, b = 0, c = 0, d = 0;
};
struct SaveEntry { std::string id; std::string name; std::string detail; friend bool operator==(const SaveEntry&,const SaveEntry&)=default; };
struct CreationView {
    std::uint64_t generation = 0;
    CreationMode mode = CreationMode::Blank;
    BlankBase base = BlankBase::Soil;
    std::string name;
    std::string theme = "flower_meadow";
    int width = 512, height = 512, cell_mm = 250;
    double west = -12, south = 35, east = 15, north = 58;
    bool earth_available = false;
    bool can_select_theme = false;
    bool preview_ready = false;
    bool busy = false;
    std::uint64_t preview_token = 0;
    std::string package_status;
    friend bool operator==(const CreationView&,const CreationView&)=default;
};
struct WorldView {
    std::uint64_t session = 0, id = 0, revision = 0;
    std::string name;
    std::uint64_t day = 0, cells = 0, dry_cells = 0;
    bool paused = false;
    int speed = 1;
    bool darkness = false, automatic_age = false;
    std::uint64_t age_ticks_remaining = 0;
    bool dirty = false;
    bool rename_preview_ready = false;
    std::uint64_t rename_preview_token = 0;
    std::string rename_consequences;
    friend bool operator==(const WorldView&,const WorldView&)=default;
};
struct View {
    Screen screen = Screen::MainMenu;
    Locale locale = Locale::Chinese;
    CreationView creation;
    WorldView world;
    std::vector<SaveEntry> saves;
    std::string selected_save;
    bool can_continue = false, busy = false;
    bool fullscreen = false, reduced_motion = false;
    // Authoritative World/creation presentation Age, supplied by the controller.
    bool presentation_darkness = false;
    int audio_volume = 70;
    // Stable semantic error key; translated UI text is selected by locale.
    std::string error_key;
    std::string error_detail;
    std::string status_key;
    friend bool operator==(const View&,const View&)=default;
};
struct Viewport { float x=0,y=0,width=0,height=0; bool earth_map=false; };
class Ui final : public Rml::EventListener {
public:
    explicit Ui(std::filesystem::path resources);
    ~Ui() override;
    Ui(const Ui&)=delete; Ui& operator=(const Ui&)=delete;
    bool initialize(int pixel_width,int pixel_height,float density,std::string& error);
    void resize(int pixel_width,int pixel_height,float density);
    void set_view(const View& view);
    void update();
    void render();
    Rml::Context& context();
    std::vector<Action> take_actions();
    Viewport viewport() const;
    bool captures_pointer(int pixel_x,int pixel_y) const;
    bool text_input_focused() const;
    bool blocks_world_input() const;
    void ProcessEvent(Rml::Event& event) override;
    // Native automation uses the same handler as real pointer and keyboard input.
    void activate(const std::string& id);
private:
    void refresh();
    void layout();
    void emit(ActionKind kind,const std::string& text={},double a=0,double b=0,double c=0,double d=0);
    std::string text(const std::string& key) const;
    std::map<std::string,std::array<std::string,3>> messages_;
    std::string loaded_saves_signature_;
    std::string local_error_;
    std::string pending_focus_;
    bool valid_creation_inputs() const;
    void input_changed(const std::string& id,const std::string& value);
    Rml::Context* context_=nullptr;
    Rml::ElementDocument* document_=nullptr;
    std::filesystem::path resources_;
    View view_;
    std::vector<Action> actions_;
    bool settings_open_=false,world_open_=false,help_open_=false,controls_open_=true;
    bool syncing_controls_=false;
    bool error_details_open_=false;
    BottomSection section_=BottomSection::Observe;
    std::uint64_t creation_generation_=UINT64_MAX,rename_session_=UINT64_MAX;
    std::string creation_name_,rename_name_;
};
const char* locale_code(Locale locale);
}
