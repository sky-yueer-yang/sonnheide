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
// Constant-memory SHA256; finish snapshots the stream without consuming it.
class Sha256Stream {
 std::array<std::uint32_t,8> state_{0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 std::array<std::uint8_t,64> buffered_{};
 std::uint64_t total_bytes_=0; std::size_t buffered_bytes_=0;
public:
 void update(std::span<const std::uint8_t> bytes); Hash finish()const;
};
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
inline constexpr std::array<std::int32_t,8> terrain_height_mm{-20000,-8000,-2000,0,0,16000,48000,96000};
inline constexpr std::array<std::int32_t,8> legacy_terrain_height_mm{-20000,-8000,-2000,1000,2000,16000,48000,96000};
inline constexpr std::int32_t terrain_cell_size_mm=2000,legacy_terrain_cell_size_mm=2000,legacy_fine_cell_size_mm=250;
inline constexpr std::int32_t terrain_micro_divisions=64,terrain_micro_size_um=31250;
inline constexpr std::int32_t terrain_guard_cells=8,minimum_core_cells=32,maximum_core_cells=4096,default_core_cells=1024;
inline constexpr std::array<std::int32_t,5> terrain_size_presets{256,512,1024,2048,4096};
inline constexpr std::uint64_t terrain_geometry_budget_bytes=134217728;
bool terrain_is_water(TerrainKind);
std::string_view terrain_name(TerrainKind); TerrainKind parse_terrain(std::string_view);
struct Cell { TerrainKind kind=TerrainKind::CloseOcean; std::uint8_t theme=0; std::uint64_t revision=1; bool operator==(const Cell&) const=default; };
struct SurfacePoint { std::int32_t cell_x,cell_z,height_mm; TerrainKind kind; bool editable,wet; std::int32_t micro_x=0,micro_z=0; std::uint8_t theme=0; std::int32_t water_depth_mm=0; };
struct Vec3 { double x=0,y=0,z=0; }; // metres, presentation/query only; integer columns authoritative
struct RayHit { double distance; Vec3 position,normal; std::int32_t cell_x,cell_z; TerrainKind kind; std::int32_t micro_x=0,micro_z=0; std::uint8_t theme=0; bool wet=false; };
struct MeshVertex { float x,y,z,nx,ny,nz,u,v; std::uint8_t material,theme=0,wet=0; };
struct Mesh { std::vector<MeshVertex> vertices; std::vector<std::uint32_t> indices; };
struct CellRun { std::int32_t end_x=0; Cell value; bool operator==(const CellRun&)const=default; };
// Palette is sorted by (kind,theme,revision). Indices are little-endian packed
// 1/2/4/8/12 bits; binary patches use lossless row/span dictionaries
// when smaller than512 bytes. Semantic hashes always use packedLSB bytes.
struct FinePatch {
 std::vector<Cell> palette; Bytes indices; std::uint8_t index_bits=1;
 //0=packedLSB;1=64 fixed single spans;2=65u16 row offsets+spans;
 //3=sorted span dictionary;4=sorted64-bit row dictionary, with packed selectors. All are lossless.
 std::uint8_t encoding=0;
 const Cell& at(std::size_t index)const; void validate()const;
 std::uint64_t row_bits(std::int32_t row)const; // binary patches only
 Bytes canonical_indices()const; // semantic packedLSB representation for hashes
 std::array<std::uint16_t,64> coverage_counts()const; // kind*8+theme, sum4096
 void compress_binary();
 static FinePatch pack(std::span<const Cell> cells);
 bool operator==(const FinePatch&)const=default;
};
struct Terrain {
 std::int32_t width=0,height=0,guard=terrain_guard_cells,cell_mm=terrain_cell_size_mm;
 std::uint32_t profile=2; //1=dense archival250/2000mm;2=2m management + real31.25mm microcolumns
 std::vector<Cell> cells; // profile1 only; never expanded for profile2
 std::vector<std::vector<CellRun>> rows;
 std::map<std::uint64_t,FinePatch> fine_patches; // z*width+x, not building/legal plot identities
 std::uint64_t storage_budget_bytes=terrain_geometry_budget_bytes;
 // Mutation/cache revision, not a geometry schema or cell-size version.
 std::uint64_t geometry_revision=1; std::uint64_t surface_revision() const { return geometry_revision; }
 const Cell& at(std::int32_t x,std::int32_t z) const; bool editable(std::int32_t x,std::int32_t z) const;
 const Cell& sample_cell(std::int32_t x,std::int32_t z,std::int32_t micro_x,std::int32_t micro_z)const;
 bool has_fine_patch(std::int32_t x,std::int32_t z)const;
 std::vector<Cell> fine_cells(std::int32_t x,std::int32_t z)const;
 std::array<std::uint16_t,64> coverage_counts(std::int32_t x,std::int32_t z)const;
 std::int32_t height_for(TerrainKind)const;
 void initialize_uniform(std::int32_t core_width,std::int32_t core_height,Cell interior);
 void set_cell(std::int32_t x,std::int32_t z,Cell value);
 void set_fine_cells(std::int32_t x,std::int32_t z,std::span<const Cell> values);
 std::uint64_t storage_bytes()const;
 std::int32_t micro_divisions()const { return profile==2?terrain_micro_divisions:1; }
 std::uint64_t dry_micro_count()const;
 // Atomic replacement: only wet microcolumns change. Existing dry Cell values,
 // including kind/theme/revision, are retained exactly. Guard is immutable.
 std::uint64_t fill_water_only(std::int32_t x,std::int32_t z,TerrainKind land_kind,std::uint8_t theme,std::uint64_t revision);
 std::optional<SurfacePoint> support(std::int64_t x_mm,std::int64_t z_mm) const;
 std::optional<SurfacePoint> surface_at_mm(std::int64_t x_mm,std::int64_t z_mm)const { return support(x_mm,z_mm); }
 std::optional<SurfacePoint> support_um(std::int64_t x_um,std::int64_t z_um)const;
 std::optional<RayHit> raycast(Vec3 origin,Vec3 direction,double maximum_distance=100000) const;
 Mesh mesh(std::int32_t start_x=0,std::int32_t start_z=0,std::int32_t count_x=-1,std::int32_t count_z=-1) const;
 Mesh mesh_coarse(std::int32_t start_x=0,std::int32_t start_z=0,std::int32_t count_x=-1,std::int32_t count_z=-1)const;
 Hash surface_hash() const; void validate() const;
};
struct GeoRect { std::int32_t west_udeg=-1000000,east_unwrapped_udeg=1000000,south_udeg=-1000000,north_udeg=1000000; };
// Diagnostics only: allocated vector payload capacities (not OS RSS or proof
// of platform allocator overhead). Excluded from World/save/hash authority.
struct CreationMetrics { std::uint64_t source_storage_bytes=0,geometry_bytes=0,scratch_peak_bytes=0,source_edge_references=0,intersection_work=0; };
// Full precision authoritative source. Coarse official files may instantiate another map-only source.
class Geography {
 struct Impl; std::shared_ptr<const Impl> impl_;
public:
 Geography(const std::filesystem::path& binary,const Hash& expected_hash);
 bool land(std::int64_t longitude_udeg,std::int64_t latitude_udeg) const;
 // Row-major sample; global navigation accepts up to360 degrees, [-90,90]; source remains official.
 std::vector<std::uint8_t> sample(const GeoRect&,std::int32_t width,std::int32_t height) const;
 std::vector<std::uint8_t> sample_world_selection(const GeoRect&,std::int32_t core_width,std::int32_t core_height,const std::function<bool()>& canceled={}) const;
 // Exact source scanlines at every microcolumn center. Streaming64 micro rows
 // keeps scratch bounded; no dense global microgrid is allocated.
 void populate_world(Terrain&,const GeoRect&,std::int32_t core_width,std::int32_t core_height,std::uint8_t theme,const std::function<bool()>& canceled={},CreationMetrics* metrics=nullptr)const;
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
World create_candidate(const CreationDraft&,const DefinitionPackage&,const Geography* geography=nullptr,const std::function<bool()>& canceled={},CreationMetrics* metrics=nullptr);
struct Command { enum class Kind { RenameWorld,SetAge,SetAgeAutomatic,SetRule,FillWaterOnly }; Kind kind; std::string text; bool boolean=false; Age age=Age::Light; std::uint64_t light_duration_tick=12000,dark_duration_tick=12000; std::int32_t cell_x=0,cell_z=0; TerrainKind land_kind=TerrainKind::Soil; std::uint8_t soil_theme=1; };
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
