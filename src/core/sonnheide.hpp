#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

namespace sonn {
struct Error : std::runtime_error { std::string code; Error(std::string c, std::string detail); };
using Bytes=std::vector<std::uint8_t>;
using Hash=std::array<std::uint8_t,32>;
using Uuid=std::array<std::uint8_t,16>;
struct Json {
 using Array=std::vector<Json>; using Object=std::map<std::string,Json,std::less<>>;
 std::variant<std::nullptr_t,bool,std::int64_t,std::string,Array,Object> value=nullptr;
 Json()=default; Json(std::nullptr_t):value(nullptr){} Json(bool x):value(x){} Json(int x):value(std::int64_t(x)){} Json(std::int64_t x):value(x){} Json(std::string x):value(std::move(x)){} Json(const char* x):value(std::string(x)){} Json(Array x):value(std::move(x)){} Json(Object x):value(std::move(x)){}
 const Json& at(std::string_view k) const; const Array& array() const; const Object& object() const;
 const std::string& string() const; std::int64_t integer() const; bool boolean() const;
 bool operator==(const Json&) const=default;
};
Json parse_json(std::string_view input); // strict UTF-8, int64, no duplicate keys; every string NFC
std::string canonical_json(const Json& value);
Hash sha256(std::span<const std::uint8_t> bytes); Hash sha256(std::string_view bytes);
std::string hex(std::span<const std::uint8_t> bytes); Bytes unhex(std::string_view text);
Hash hash_from_hex(std::string_view text); Uuid uuid_from_hex(std::string_view text);
Bytes read_file(const std::filesystem::path& path,std::uint64_t max_bytes=268435456);
struct Ref { Uuid world_id{}; std::string kind; std::uint64_t stable_id=0; std::uint32_t generation=1; bool operator==(const Ref&) const=default; };
Json ref_json(const Ref&); Ref parse_ref(const Json&); bool registered_kind(std::string_view); std::string canonical_kind(std::string_view);
Bytes rng_preimage(const Hash& seed,std::string_view domain,const Ref&,const Hash& definition,std::uint64_t event,std::uint64_t draw,std::uint64_t attempt);
struct RngDraw { std::uint64_t value,next_draw_index,attempts; };
std::optional<std::uint64_t> reduce_uniform_raw(std::uint64_t raw,std::uint64_t bound); // arithmetic only, not an entropy/event API
RngDraw uniform_rng(const Hash& seed,std::string_view domain,const Ref&,const Hash& definition,std::uint64_t event,std::uint64_t draw,std::uint64_t bound);
struct DefinitionPackage { Hash hash{},asset_hash{}; Json manifest; std::filesystem::path root; std::vector<std::string> files; };
// Manifest {format:"SonnDefinitions1", files:[{path:relative,sha256:64hex}]}; sorted unique paths.
DefinitionPackage admit_definitions(const std::filesystem::path& manifest,const std::filesystem::path& root);
Hash admit_assets(const std::filesystem::path& manifest,const std::filesystem::path& root); // SonnAssets1, identical strict file admission

enum class TerrainKind : std::uint8_t { DeepOcean,CloseOcean,ShallowWater,Sand,Soil,Hill,Mountain,HighPeak };
inline constexpr std::array<std::int32_t,8> terrain_height_mm{-20000,-8000,-2000,1000,2000,16000,48000,96000};
inline constexpr std::int32_t terrain_cell_size_mm=250,legacy_terrain_cell_size_mm=2000;
inline constexpr std::int32_t terrain_guard_cells=8,minimum_core_cells=32,maximum_core_cells=1008,default_core_cells=512;
std::string_view terrain_name(TerrainKind); TerrainKind parse_terrain(std::string_view);
struct Cell { TerrainKind kind=TerrainKind::CloseOcean; std::uint8_t theme=0; std::uint64_t revision=1; bool operator==(const Cell&) const=default; };
struct SurfacePoint { std::int32_t cell_x,cell_z,height_mm; TerrainKind kind; bool editable,wet; };
struct Vec3 { double x=0,y=0,z=0; }; // metres, presentation/query only; integer columns authoritative
struct RayHit { double distance; Vec3 position,normal; std::int32_t cell_x,cell_z; TerrainKind kind; };
struct MeshVertex { float x,y,z,nx,ny,nz,u,v; std::uint8_t material; };
struct Mesh { std::vector<MeshVertex> vertices; std::vector<std::uint32_t> indices; };
struct Terrain {
 std::int32_t width=0,height=0,guard=terrain_guard_cells,cell_mm=terrain_cell_size_mm; std::vector<Cell> cells;
 // Mutation/cache revision, not a geometry schema or cell-size version.
 std::uint64_t geometry_revision=1; std::uint64_t surface_revision() const { return geometry_revision; }
 const Cell& at(std::int32_t x,std::int32_t z) const; bool editable(std::int32_t x,std::int32_t z) const;
 std::optional<SurfacePoint> support(std::int64_t x_mm,std::int64_t z_mm) const;
 std::optional<RayHit> raycast(Vec3 origin,Vec3 direction,double maximum_distance=100000) const;
 Mesh mesh(std::int32_t start_x=0,std::int32_t start_z=0,std::int32_t count_x=-1,std::int32_t count_z=-1) const;
 Hash surface_hash() const; void validate() const;
};
struct GeoRect { std::int32_t west_udeg=-1000000,east_unwrapped_udeg=1000000,south_udeg=-1000000,north_udeg=1000000; };
// Full precision authoritative source. Coarse official files may instantiate another map-only source.
class Geography {
 struct Impl; std::shared_ptr<const Impl> impl_;
public:
 Geography(const std::filesystem::path& binary,const Hash& expected_hash);
 bool land(std::int64_t longitude_udeg,std::int64_t latitude_udeg) const;
 // Row-major sample; global navigation accepts up to360 degrees, [-90,90]; source remains official.
 std::vector<std::uint8_t> sample(const GeoRect&,std::int32_t width,std::int32_t height) const;
 std::vector<std::uint8_t> sample_world_selection(const GeoRect&,std::int32_t core_width,std::int32_t core_height,const std::function<bool()>& canceled={}) const;
 const Hash& source_hash() const; std::size_t polygon_count() const;
};
enum class CreationKind { BlankSea,BlankLand,Earth };
struct CreationDraft { std::uint64_t session_generation=1,draft_generation=1,candidate_generation=1; CreationKind kind=CreationKind::BlankLand; std::string name="Sonnheide"; std::int32_t core_width=default_core_cells,core_height=default_core_cells; std::uint8_t soil_theme=1; GeoRect selection; Uuid world_id{}; Hash seed{}; std::uint64_t budget_bytes=134217728; };
enum class Age { Light,Darkness };
struct AgeState { Age current=Age::Light; bool automatic=false; std::uint64_t remaining_tick=0,light_duration_tick=12000,dark_duration_tick=12000; bool operator==(const AgeState&) const=default; };
struct CommandReceipt { std::string command_id; Hash payload_hash{}; std::uint64_t committed_revision=0; Json effects; };
struct World {
 Uuid id{}; Hash seed{},definition_hash{},asset_hash{}; std::string name; Terrain terrain;
 std::uint64_t tick=0,revision=1,boundary_ordinal=0,input_sequence=0; AgeState age;
 std::map<std::string,bool,std::less<>> rules; Json provenance;
 std::vector<CommandReceipt> receipts;
 // S00/S01 contain no actors, buildings or inventories. Unimplemented domains reject on load.
 std::uint64_t population=0,awakened_animals=0,buildings=0;
 Ref ref() const; Json query() const; Hash deterministic_hash() const; void validate(const Hash& expected_definitions) const;
};
World create_candidate(const CreationDraft&,const DefinitionPackage&,const Geography* geography=nullptr,const std::function<bool()>& canceled={});
struct Command { enum class Kind { RenameWorld,SetAge,SetAgeAutomatic,SetRule }; Kind kind; std::string text; bool boolean=false; Age age=Age::Light; std::uint64_t light_duration_tick=12000,dark_duration_tick=12000; };
struct Envelope { Uuid world_id{}; std::uint64_t session_generation=1,expected_revision=0,input_sequence=0; std::string command_id; };
struct PreparedCommand { Envelope envelope; Command command; Hash consequence_hash{}; Json preview; };
struct TickTrace { std::uint64_t tick,revision; std::array<std::string_view,11> phases; };
class Authority {
 World world_; std::uint64_t session_; std::thread::id writer_;
 void check_writer() const;
public:
 explicit Authority(World world,std::uint64_t session_generation=1);
 const World& world() const { return world_; }
 PreparedCommand prepare(const Envelope&,const Command&) const;
 CommandReceipt commit(const PreparedCommand&);
 TickTrace advance_tick(); // 11 barriers; future domains are explicitly absent in S01
};
Bytes encode_save(const World&); World decode_save(std::span<const std::uint8_t>,const DefinitionPackage&);
enum class SaveFault { None,BeforeWrite,ShortWrite,FileFlush,Readback,RenameCheckpoint,CheckpointDirectoryFlush,PointerWrite,PointerRename,PointerDirectoryFlush,PendingRemove,PendingDirectoryFlush,PendingDirectoryFlushAndRecoveryFlush };
enum class SaveStatus { Committed,Failed,DurabilityUnknown };
struct Checkpoint { SaveStatus status=SaveStatus::Failed; std::string code; std::string filename; Hash file_hash{}; Uuid world_id{}; std::uint64_t revision=0; bool reader_gate_restored=false; std::string reader_gate_error; };
class SaveStore {
 std::filesystem::path root_;
 // A failed durable publication cannot promote its candidate in this process,
 // even if pending was removed and reinstalling its on-disk gate also fails.
 // Keep raw bytes so malformed/incompatible prior pointers remain unchanged.
 std::optional<Bytes> isolated_previous_pointer_;
public:
 explicit SaveStore(std::filesystem::path root);
 Checkpoint save(const World&,const DefinitionPackage&,SaveFault fault=SaveFault::None);
 std::optional<World> continue_world(const DefinitionPackage&) const;
 std::vector<std::string> recovery_candidates() const;
 World recover(std::string_view filename,const DefinitionPackage&) const; // explicit caller recovery choice
 const std::filesystem::path& root() const { return root_; }
};
class WorldSession {
 std::uint64_t generation_=1,draft_generation_=0,candidate_generation_=0; std::thread::id writer_;
 std::unique_ptr<Authority> active_; std::optional<CreationDraft> draft_;
public:
 WorldSession(); std::uint64_t generation() const { return generation_; }
 const Authority* active() const { return active_.get(); } Authority* active() { return active_.get(); }
 CreationDraft begin(CreationDraft draft); void cancel(); bool accepts(const CreationDraft&) const;
 bool publish(const CreationDraft&,World candidate,SaveStore&,const DefinitionPackage&,Checkpoint& result,SaveFault fault=SaveFault::None);
 void load_continue(SaveStore&,const DefinitionPackage&); void load_checkpoint(std::string_view filename,SaveStore&,const DefinitionPackage&); void return_to_menu();
};
} // namespace sonn
