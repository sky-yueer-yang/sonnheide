#include "sonnheide/world_creation.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <future>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace sonnheide::creation {
namespace {
constexpr std::size_t max_checkpoint_bytes = 16384;
void require(bool condition, const char* why) { if (!condition) throw std::runtime_error(why); }
bool hex_hash(const std::string& value) {
 return value.size() == 64 && value.find_first_not_of("0123456789abcdef") == std::string::npos;
}
bool identifier(const std::string& id) {
 return id.size() == 32 && id.find_first_not_of("0123456789abcdef") == std::string::npos;
}
bool valid_name(const std::string& name) {
 if (name.empty() || name.size() > 384) return false;
 std::size_t count = 0;
 for (std::size_t i=0; i<name.size();) {
  const auto ch = static_cast<unsigned char>(name[i]);
  std::uint32_t code{}; std::size_t length{};
  if (ch < 0x80) { code = ch; length = 1; }
  else if ((ch & 0xe0) == 0xc0) { code = ch & 0x1f; length = 2; }
  else if ((ch & 0xf0) == 0xe0) { code = ch & 0x0f; length = 3; }
  else if ((ch & 0xf8) == 0xf0) { code = ch & 7; length = 4; }
  else return false;
  if (i + length > name.size()) return false;
  for (std::size_t n=1; n<length; ++n) {
   const auto continuation = static_cast<unsigned char>(name[i+n]);
   if ((continuation & 0xc0) != 0x80) return false;
   code = (code << 6) | (continuation & 0x3f);
  }
  if ((length == 2 && code < 0x80) || (length == 3 && code < 0x800) || (length == 4 && code < 0x10000)
      || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff) || code < 0x20 || code == 0x7f) return false;
  i += length; if (++count > 96) return false;
 }
 return true;
}
std::uint64_t checksum(const std::string& value) {
 std::uint64_t result = 14695981039346656037ULL;
 for (unsigned char ch : value) { result ^= ch; result *= 1099511628211ULL; }
 return result;
}
std::string read_file(const std::filesystem::path& path) {
 std::ifstream in(path, std::ios::binary);
 require(static_cast<bool>(in), "cannot read checkpoint or continue pointer");
 std::string value;
 std::array<char, 4096> buffer{};
 while (in) {
  in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  const auto count = static_cast<std::size_t>(in.gcount());
  require(value.size() + count <= max_checkpoint_bytes, "checkpoint exceeds bounded envelope");
  value.append(buffer.data(), count);
 }
 require(in.eof(), "checkpoint read failed");
 return value;
}
std::string new_id() {
 std::random_device random;
 std::ostringstream out; out.exceptions(std::ios::badbit | std::ios::failbit);
 for (int n=0; n<4; ++n) out << std::hex << std::setw(8) << std::setfill('0') << static_cast<std::uint32_t>(random());
 return out.str();
}
void sync_directory(const std::filesystem::path& directory) {
#ifdef _WIN32
 // MoveFileExW WRITE_THROUGH supplies the Windows durable rename boundary.
 (void)directory;
#else
 const int fd = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY);
 require(fd >= 0, "cannot open save directory for fsync");
 const int result = ::fsync(fd);
 const int error = errno;
 ::close(fd);
 if (result != 0) throw std::system_error(error, std::generic_category(), "save directory fsync");
#endif
}
void rename_atomic(const std::filesystem::path& from, const std::filesystem::path& to) {
#ifdef _WIN32
 require(::MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0,
         "durable checkpoint rename failed");
#else
 require(::rename(from.c_str(), to.c_str()) == 0, "atomic checkpoint rename failed");
#endif
}
class DirectoryLock {
public:
 explicit DirectoryLock(const std::filesystem::path& directory) {
#ifdef _WIN32
  handle_ = ::CreateFileW((directory / ".creation.lock").c_str(), GENERIC_READ | GENERIC_WRITE,
                       0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  require(handle_ != INVALID_HANDLE_VALUE, "save directory is in use");
#else
  fd_ = ::open((directory / ".creation.lock").c_str(), O_RDWR | O_CREAT, 0600);
  require(fd_ >= 0, "cannot open save directory lock");
  if (::flock(fd_, LOCK_EX | LOCK_NB) != 0) { ::close(fd_); fd_=-1; throw std::runtime_error("save directory is in use"); }
#endif
 }
 ~DirectoryLock() {
#ifdef _WIN32
  if (handle_ != INVALID_HANDLE_VALUE) ::CloseHandle(handle_);
#else
  if (fd_ >= 0) { ::flock(fd_, LOCK_UN); ::close(fd_); }
#endif
 }
 DirectoryLock(const DirectoryLock&) = delete;
 DirectoryLock& operator=(const DirectoryLock&) = delete;
private:
#ifdef _WIN32
 HANDLE handle_{INVALID_HANDLE_VALUE};
#else
 int fd_{-1};
#endif
};
struct Steps { PersistenceStep open, write, flush, sync, rename, directory; };
constexpr Steps checkpoint_steps{PersistenceStep::CheckpointOpen, PersistenceStep::CheckpointWrite,
 PersistenceStep::CheckpointFlush, PersistenceStep::CheckpointFileSync,
 PersistenceStep::CheckpointRename, PersistenceStep::CheckpointDirectorySync};
constexpr Steps pointer_steps{PersistenceStep::PointerOpen, PersistenceStep::PointerWrite,
 PersistenceStep::PointerFlush, PersistenceStep::PointerFileSync,
 PersistenceStep::PointerRename, PersistenceStep::PointerDirectorySync};
void durable_write(const std::filesystem::path& destination, const std::string& bytes,
                   const std::function<void(PersistenceStep)>& hook, const Steps& steps,
                   const std::string& nonce) {
 const auto temporary = destination.parent_path() / (destination.filename().string() + "." + nonce + ".tmp");
 std::FILE* file = nullptr;
 const auto invoke = [&](PersistenceStep step) { if (hook) hook(step); };
 try {
  invoke(steps.open);
#ifdef _WIN32
  require(::_wfopen_s(&file, temporary.c_str(), L"wbx") == 0, "cannot create exclusive checkpoint temporary");
#else
  file = std::fopen(temporary.c_str(), "wbx");
#endif
  require(file != nullptr, "cannot create exclusive checkpoint temporary");
  invoke(steps.write);
  require(std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size(), "checkpoint write failed");
  invoke(steps.flush);
  require(std::fflush(file) == 0, "checkpoint flush failed");
  invoke(steps.sync);
#ifdef _WIN32
  require(::_commit(::_fileno(file)) == 0, "checkpoint file sync failed");
#else
  require(::fsync(::fileno(file)) == 0, "checkpoint file fsync failed");
#endif
  const int close_result = std::fclose(file); file = nullptr;
  require(close_result == 0, "checkpoint close failed");
  invoke(steps.rename);
  rename_atomic(temporary, destination);
  invoke(steps.directory);
  sync_directory(destination.parent_path());
 } catch (...) {
  if (file) std::fclose(file);
  std::error_code ignored; std::filesystem::remove(temporary, ignored);
  throw;
 }
}
std::uint64_t geometry_fingerprint(const earth::EarthWorldDefinition& definition) {
 // Stable endian-independent summary of the admitted authoritative cells. Heights
 // are quantized to one micrometre, avoiding irrelevant libm last-bit differences.
 std::uint64_t value=14695981039346656037ULL;
 const auto add=[&](std::uint64_t number){for(int i=0;i<8;++i){value^=number&255;value*=1099511628211ULL;number>>=8;}};
 add(definition.columns());add(definition.rows());
 for(const auto& cell:definition.cells()) {
  add(cell.source_land);add(cell.land);add(cell.guard);add(cell.water_region);
  require(std::isfinite(cell.height_m)&&std::abs(cell.height_m)<1e9,"invalid authoritative cell height");
  add(static_cast<std::uint64_t>(std::llround(cell.height_m*1e6)));
 }
 return value;
}
std::string encode(const WorldSession& session) {
 const auto& c = session.definition->config();
 std::ostringstream body; body.exceptions(std::ios::badbit | std::ios::failbit); body.imbue(std::locale::classic()); body << std::setprecision(std::numeric_limits<double>::max_digits10);
 body << session.world_id << '\n' << std::quoted(session.display_name) << '\n' << session.definition->source_hash() << '\n'
      << session.definition->recipe_hash() << '\n' << geometry_fingerprint(*session.definition) << '\n' << session.presentation_recipe_hash << '\n'
      << c.centre.longitude << ' ' << c.centre.latitude << '\n'
      << c.width_m << ' ' << c.height_m << ' ' << c.world_scale << '\n'
      << c.closure_band_m << ' ' << c.sea_collar_m << ' ' << c.sea_guard_m << '\n'
      << c.coast_width_m << ' ' << c.land_height_m << ' ' << c.water_depth_m << '\n'
      << c.grid_cells_x << ' ' << c.grid_cells_y << ' ' << c.seed << '\n'
      << session.tick << ' ' << static_cast<int>(session.age) << ' ' << session.age_automatic << '\n'
      << session.entities.people << ' ' << session.entities.buildings << ' '
      << session.entities.countries << ' ' << session.entities.tasks << '\n';
 const auto bytes = body.str();
 return "SONNHEIDE_EARTH_EMPTY 1\n" + std::to_string(checksum(bytes)) + "\n" + bytes;
}
std::shared_ptr<WorldSession> decode(const std::string& bytes, std::shared_ptr<const earth::GeoAtlas> atlas,
                                     const std::string& presentation_hash, const std::function<bool()>& cancelled = {}) {
 require(bytes.size() <= max_checkpoint_bytes, "checkpoint exceeds bounded envelope");
 std::istringstream in(bytes); in.imbue(std::locale::classic()); std::string magic; int version{}; std::uint64_t digest{};
 require(static_cast<bool>(in >> magic >> version >> digest), "invalid checkpoint header");
 require(magic == "SONNHEIDE_EARTH_EMPTY" && version == 1, "incompatible checkpoint schema");
 require(in.get() == '\n', "invalid checkpoint envelope");
 const auto body_start = static_cast<std::size_t>(in.tellg());
 require(checksum(bytes.substr(body_start)) == digest, "checkpoint integrity check failed");
 auto session = std::make_shared<WorldSession>(); earth::WorldConfig c;
 std::string source_hash, recipe_hash; std::uint64_t geometry_hash{}; int age{}, automatic{};
 require(static_cast<bool>(in >> session->world_id >> std::quoted(session->display_name) >> source_hash >> recipe_hash >> geometry_hash >> session->presentation_recipe_hash
  >> c.centre.longitude >> c.centre.latitude >> c.width_m >> c.height_m >> c.world_scale
  >> c.closure_band_m >> c.sea_collar_m >> c.sea_guard_m
  >> c.coast_width_m >> c.land_height_m >> c.water_depth_m
  >> c.grid_cells_x >> c.grid_cells_y >> c.seed
  >> session->tick >> age >> automatic
  >> session->entities.people >> session->entities.buildings >> session->entities.countries >> session->entities.tasks),
  "invalid checkpoint body");
 in >> std::ws; require(in.eof(), "unexpected checkpoint trailing fields");
 require(identifier(session->world_id), "invalid WorldId");
 require(valid_name(session->display_name), "invalid UTF-8 world name");
 require(source_hash == atlas->source_hash(), "checkpoint source package mismatch");
 require(session->presentation_recipe_hash == presentation_hash, "checkpoint presentation recipe mismatch");
 require(session->tick == 0 && age == 0 && automatic == 0, "phase-one checkpoint must be initial Light/manual");
 require(session->entities.people == 0 && session->entities.buildings == 0 && session->entities.countries == 0 && session->entities.tasks == 0,
         "phase-one checkpoint contains unsupported entities");
 session->definition = earth::EarthWorldDefinition::create(std::move(atlas), c, cancelled);
 require(session->definition->recipe_hash() == recipe_hash, "checkpoint frozen geometry recipe mismatch");
 require(geometry_fingerprint(*session->definition)==geometry_hash,"checkpoint authoritative geometry mismatch");
 return session;
}
std::string pointer_id(const std::string& bytes) {
 std::istringstream in(bytes); std::string magic, id; int version{};
 require(static_cast<bool>(in >> magic >> version >> id) && magic == "SONNHEIDE_CONTINUE" && version == 1 && identifier(id),
         "invalid continue pointer");
 in >> std::ws; require(in.eof(), "invalid continue pointer suffix"); return id;
}
}
struct CreateCoordinator::Pending {
 CreationTicket ticket;
 std::shared_ptr<std::atomic<bool>> cancelled;
 std::filesystem::path checkpoint;
 std::future<std::shared_ptr<WorldSession>> prepared;
};
CreateCoordinator::CreateCoordinator(std::filesystem::path saves, std::shared_ptr<const earth::GeoAtlas> atlas,
                                   std::string admitted_presentation_recipe_hash)
 : saves_(std::move(saves)), atlas_(std::move(atlas)), presentation_hash_(std::move(admitted_presentation_recipe_hash)),
   writer_(std::this_thread::get_id()) {
 require(atlas_ != nullptr && hex_hash(atlas_->source_hash()), "geographic source package is not admitted");
 require(hex_hash(presentation_hash_), "presentation recipe is not admitted");
 std::vector<std::filesystem::path> missing_directories;
 for (auto path=std::filesystem::absolute(saves_); !std::filesystem::exists(path); path=path.parent_path()) missing_directories.push_back(path);
 std::filesystem::create_directories(saves_);
 for (auto it=missing_directories.rbegin();it!=missing_directories.rend();++it) { sync_directory(*it); sync_directory(it->parent_path()); }
 sync_directory(saves_);
 std::random_device random;
 next_generation_ = (static_cast<std::uint64_t>(random()) << 32 | random()) & 0x7fffffffffffffffULL;
}
void CreateCoordinator::require_writer() const { require(std::this_thread::get_id() == writer_, "world publication requires its single writer"); }
void CreateCoordinator::fault(PersistenceStep step) const { if (injector_) injector_(step); }
void CreateCoordinator::set_fault_injector(std::function<void(PersistenceStep)> injector) { require_writer(); injector_ = std::move(injector); }
CreationTicket CreateCoordinator::begin(DraftRevision revision, std::string display_name) {
 require_writer(); require(valid_name(display_name), "world name must be valid UTF-8 and at most 96 characters"); draft_name_ = std::move(display_name);
 require(next_request_ != std::numeric_limits<std::uint64_t>::max() && next_generation_ != std::numeric_limits<std::uint64_t>::max(), "creation identity space exhausted");
 if (pending_) pending_->cancelled->store(true);
 current_ = {++next_request_, ++next_generation_, revision}; cancelled_ = false; return current_;
}
void CreateCoordinator::cancel(CreationTicket ticket) { require_writer(); if (current(ticket)) { cancelled_ = true; if (pending_) pending_->cancelled->store(true); } }
void CreateCoordinator::invalidate_draft(DraftRevision revision) { require_writer(); if (current_.draft_revision != revision) { cancelled_ = true; if (pending_) pending_->cancelled->store(true); current_.draft_revision = revision; } }
bool CreateCoordinator::current(CreationTicket ticket) const {
 return ticket.request != 0 && ticket.request == current_.request && ticket.generation == current_.generation && ticket.draft_revision == current_.draft_revision;
}
std::string CreateCoordinator::validate_readiness(const earth::EarthWorldDefinition& definition, const ResourceReadiness& readiness) const {
 if (definition.source_hash() != atlas_->source_hash()) return "candidate geographic package is stale";
 if (definition.cells().empty()) return "candidate has no admitted geometry";
 if (readiness.terrain_recipe_hash != definition.recipe_hash()) return "GPU terrain belongs to another candidate";
 if (readiness.presentation_recipe_hash != presentation_hash_) return "GPU presentation recipe is stale";
 if (!readiness.terrain_ready || !readiness.materials_ready || !readiness.light_sky_ready || !readiness.dark_sky_ready)
  return "candidate terrain, material or sky resources are not ready";
 return {};
}
CreateCoordinator::~CreateCoordinator() {
 if (pending_) { pending_->cancelled->store(true); pending_->prepared.wait();
  std::error_code ignored; std::filesystem::remove(pending_->checkpoint, ignored); }
}
bool CreateCoordinator::busy() const { require_writer(); return pending_ != nullptr; }
CreationResult CreateCoordinator::start(CreationTicket ticket,
                                        std::shared_ptr<const earth::EarthWorldDefinition> candidate,
                                        const ResourceReadiness& readiness) {
 require_writer();
 if (!current(ticket)) return {CreateStatus::Stale, "creation request is stale", {}};
 if (cancelled_) return {CreateStatus::Cancelled, "creation was cancelled", {}};
 if (pending_) return {CreateStatus::Failed, "another bounded creation job is still finishing", {}};
 if (!candidate) return {CreateStatus::Failed, "candidate geometry is missing", {}};
 if (auto error = validate_readiness(*candidate, readiness); !error.empty()) return {CreateStatus::Failed, std::move(error), {}};
 try {
  auto session = std::make_shared<WorldSession>();
  session->world_id = new_id(); require(identifier(session->world_id), "WorldId generation failed"); session->display_name = draft_name_; session->session_generation = ticket.generation;
  session->definition = std::move(candidate); session->presentation_recipe_hash = presentation_hash_;
  auto pending = std::make_unique<Pending>(); pending->ticket = ticket;
  pending->cancelled = std::make_shared<std::atomic<bool>>(false);
  pending->checkpoint = saves_ / (session->world_id + ".world");
  auto checkpoint = pending->checkpoint;
  const auto directory = saves_; const auto atlas = atlas_; const auto presentation_hash = presentation_hash_;
  const auto cancelled = pending->cancelled; const auto injector = injector_;
  pending->prepared = std::async(std::launch::async, [session, checkpoint, directory, atlas, presentation_hash, cancelled, injector] {
   try {
    DirectoryLock lock(directory);
    require(!std::filesystem::exists(checkpoint), "WorldId collision");
    const auto hook = [&](PersistenceStep step) {
     if (injector) injector(step);
     require(!cancelled->load(), "creation was cancelled or superseded");
    };
    durable_write(checkpoint, encode(*session), hook, checkpoint_steps, session->world_id);
    hook(PersistenceStep::CheckpointReadback);
    auto checked = decode(read_file(checkpoint), atlas, presentation_hash, [cancelled] { return cancelled->load(); });
    require(checked->world_id == session->world_id && checked->definition->recipe_hash() == session->definition->recipe_hash(),
            "checkpoint readback does not match candidate");
    require(!cancelled->load(), "creation was cancelled or superseded");
    return session;
   } catch (...) {
    std::error_code ignored; std::filesystem::remove(checkpoint, ignored); throw;
   }
  });
  pending_ = std::move(pending);
  return {CreateStatus::Preparing, {}, {}};
 } catch (const std::exception& exception) { return {CreateStatus::Failed, exception.what(), {}}; }
}
std::optional<CreationResult> CreateCoordinator::poll() {
 require_writer();
 if (!pending_) return {};
 if (pending_->prepared.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return {};
 auto pending = std::move(pending_);
 std::filesystem::path pointer;
 std::optional<std::string> old_pointer; bool pointer_attempted = false;
 std::unique_ptr<DirectoryLock> commit_lock;
 try {
  pointer = saves_ / "continue.pointer";
  auto session = pending->prepared.get();
  if (!current(pending->ticket) || cancelled_ || pending->cancelled->load()) {
   std::error_code ignored; std::filesystem::remove(pending->checkpoint, ignored);
   return CreationResult{current(pending->ticket) ? CreateStatus::Cancelled : CreateStatus::Stale, "prepared creation is no longer current", {}};
  }
  commit_lock = std::make_unique<DirectoryLock>(saves_);
  if (std::filesystem::exists(pointer)) old_pointer = read_file(pointer);
  const auto hook = [&](PersistenceStep step) {
   fault(step);
   require(current(pending->ticket) && !cancelled_, "creation was cancelled or superseded");
  };
  pointer_attempted = true;
  durable_write(pointer, "SONNHEIDE_CONTINUE 1\n" + session->world_id + "\n", hook, pointer_steps, session->world_id);
  active_ = std::move(session);
  return CreationResult{CreateStatus::Published, {}, active_};
 } catch (const std::exception& exception) {
  std::string message = exception.what();
  if (pointer_attempted) {
   try {
    if (old_pointer) durable_write(pointer, *old_pointer, {}, pointer_steps, new_id());
    else { std::filesystem::remove(pointer); sync_directory(saves_); }
   } catch (const std::exception& rollback_error) {
    return CreationResult{CreateStatus::Failed, message + "; continue-pointer recovery failed: " + rollback_error.what(), {}};
   }
  }
  std::error_code ignored; std::filesystem::remove(pending->checkpoint, ignored);
  return CreationResult{!current(pending->ticket) ? CreateStatus::Stale : (cancelled_ ? CreateStatus::Cancelled : CreateStatus::Failed), std::move(message), {}};
 }
}
CreationResult CreateCoordinator::create(CreationTicket ticket,
                                        std::shared_ptr<const earth::EarthWorldDefinition> candidate,
                                        const ResourceReadiness& readiness) {
 auto result = start(ticket, std::move(candidate), readiness);
 if (result.status != CreateStatus::Preparing) return result;
 for (;;) { if (auto done = poll()) return *done; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
}
std::shared_ptr<const WorldSession> CreateCoordinator::prepare_continue(const std::function<bool()>& cancelled) const {
 DirectoryLock lock(saves_);
 const auto id = pointer_id(read_file(saves_ / "continue.pointer"));
 auto session = decode(read_file(saves_ / (id + ".world")), atlas_, presentation_hash_, cancelled);
 require(session->world_id == id, "continue pointer and checkpoint identity disagree");
 return session;
}
CreationResult CreateCoordinator::publish_continue(std::shared_ptr<const WorldSession> prepared, const ResourceReadiness& readiness, CreationTicket ticket) {
 require_writer();
 try {
  if (ticket.request != 0 && !current(ticket)) return {CreateStatus::Stale, "continue request is stale", {}};
  if (ticket.request != 0 && cancelled_) return {CreateStatus::Cancelled, "continue request was cancelled", {}};
  require(prepared != nullptr && prepared->definition != nullptr, "prepared continue world is missing");
  require(valid_name(prepared->display_name) && identifier(prepared->world_id), "prepared continue identity is invalid");
  require(prepared->tick == 0 && prepared->age == EnvironmentAge::Light && !prepared->age_automatic &&
   prepared->entities.people == 0 && prepared->entities.buildings == 0 && prepared->entities.countries == 0 && prepared->entities.tasks == 0,
   "prepared continue contains unsupported state");
  require(pointer_id(read_file(saves_ / "continue.pointer")) == prepared->world_id, "continue pointer changed during preparation");
  if (auto error = validate_readiness(*prepared->definition, readiness); !error.empty()) return {CreateStatus::Failed, std::move(error), {}};
  require(next_generation_ != std::numeric_limits<std::uint64_t>::max(), "session identity space exhausted");
  auto session = std::make_shared<WorldSession>(*prepared); session->session_generation = ++next_generation_;
  if (pending_) pending_->cancelled->store(true);
  active_ = std::move(session); current_ = {}; cancelled_ = false;
  return {CreateStatus::Published, {}, active_};
 } catch (const std::exception& exception) { return {CreateStatus::Failed, exception.what(), {}}; }
}
CreationResult CreateCoordinator::load_continue(const ResourceReadiness& readiness) {
 require_writer();
 try { return publish_continue(prepare_continue(), readiness); }
 catch (const std::exception& exception) { return {CreateStatus::Failed, exception.what(), {}}; }
}
bool CreateCoordinator::can_continue() const {
 require_writer();
 try {
  const auto id = pointer_id(read_file(saves_ / "continue.pointer"));
  const auto bytes = read_file(saves_ / (id + ".world")); std::istringstream in(bytes);
  std::string magic, saved_id, name, source, recipe, presentation; int version{}; std::uint64_t digest{}, geometry_hash{};
  if (!(in >> magic >> version >> digest) || magic != "SONNHEIDE_EARTH_EMPTY" || version != 1 || in.get() != '\n') return false;
  const auto offset = static_cast<std::size_t>(in.tellg());
  if (checksum(bytes.substr(offset)) != digest) return false;
  return static_cast<bool>(in >> saved_id >> std::quoted(name) >> source >> recipe >> geometry_hash >> presentation) && saved_id == id &&
         valid_name(name) && source == atlas_->source_hash() && presentation == presentation_hash_;
 }
 catch (...) { return false; }
}
}
