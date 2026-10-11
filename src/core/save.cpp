#include "sonnheide.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <limits>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif
namespace sonn {
namespace {
void le(Bytes&b,uint64_t v,unsigned n){for(unsigned i=0;i<n;i++)b.push_back(uint8_t(v>>(8*i)));}
uint64_t get(std::span<const uint8_t>b,size_t p,unsigned n){if(p>b.size()||n>b.size()-p)throw Error("SAVE_TRUNCATED","integer");uint64_t v=0;for(unsigned i=0;i<n;i++)v|=uint64_t(b[p+i])<<(8*i);return v;}
Hash hash_at(std::span<const uint8_t>b,size_t p){if(p>b.size()||32>b.size()-p)throw Error("SAVE_TRUNCATED","hash");Hash h{};std::copy_n(b.begin()+ptrdiff_t(p),32,h.begin());return h;}
std::string text(std::span<const uint8_t>b){return {reinterpret_cast<const char*>(b.data()),b.size()};}
int64_t signed_value(uint64_t v){if(v>uint64_t(INT64_MAX))throw Error("SAVE_INTEGER","signed64");return int64_t(v);}
uint64_t positive(const Json&j,bool nonzero=false){auto n=j.integer();if(n<0||(nonzero&&n==0))throw Error("SAVE_INTEGER","positive required");return uint64_t(n);}
void keys(const Json&j,std::initializer_list<std::string_view> expected){if(j.object().size()!=expected.size())throw Error("SAVE_SCHEMA","unknown/missing fields");for(auto k:expected)(void)j.at(k);}
Json empty_records(){return Json::Object{{"records",Json::Array{}}};}
struct Segment {uint32_t kind;uint64_t records;Json value;uint32_t schema=1;};
Json hex_chunks(std::span<const uint8_t> raw,size_t stride){Json::Array a;const size_t chunk=2048*stride;for(size_t pos=0;pos<raw.size();pos+=chunk)a.emplace_back(hex(raw.subspan(pos,std::min(chunk,raw.size()-pos))));return a;}
Bytes chunk_bytes(const Json& j,size_t stride,size_t maximum){Bytes out;for(auto& part:j.array()){auto& str=part.string();if(str.empty()||str.size()>2048*stride*2||str.size()%(stride*2))throw Error("TERRAIN_PROFILE","canonical row chunk");auto b=unhex(str);if(out.size()>maximum||b.size()>maximum-out.size())throw Error("TERRAIN_PROFILE","chunk budget");if(!out.empty()&&out.size()%(2048*stride))throw Error("TERRAIN_PROFILE","only last chunk may be short");out.insert(out.end(),b.begin(),b.end());}return out;}
std::vector<Segment> segments(const World&w){Json::Object rules;for(auto&[k,v]:w.rules)rules.emplace(k,v);Json::Array rows,patches;Json terrain;
 if(w.terrain.profile==1){for(int32_t z=0;z<w.terrain.height;z++){Bytes b;for(int32_t x=0;x<w.terrain.width;x++){auto&c=w.terrain.at(x,z);b.push_back(uint8_t(c.kind));b.push_back(c.theme);le(b,c.revision,8);}rows.emplace_back(hex(b));}terrain=Json::Object{{"width",w.terrain.width},{"height",w.terrain.height},{"cell_mm",w.terrain.cell_mm},{"guard",w.terrain.guard},{"geometry_revision",signed_value(w.terrain.geometry_revision)},{"row_encoding","kind_u8_theme_u8_revision_u64le_hex"},{"rows",rows},{"surface_hash",hex(w.terrain.surface_hash())}};
 }else{
  for(auto& row:w.terrain.rows){Bytes b;for(auto&run:row){le(b,uint32_t(run.end_x),4);b.push_back(uint8_t(run.value.kind));b.push_back(run.value.theme);le(b,run.value.revision,8);}rows.emplace_back(hex_chunks(b,14));}
  auto palette_bytes=[](const FinePatch&patch){Bytes b;b.reserve(patch.palette.size()*10);for(auto&c:patch.palette){b.push_back(uint8_t(c.kind));b.push_back(c.theme);le(b,c.revision,8);}return b;};
  // The registry is ordered by its canonical little-endian bytes. Repeated
  // Earth sea/soil palettes are encoded once rather than once per coast cell.
  std::map<Bytes,size_t> palette_ids;for(auto&[key,patch]:w.terrain.fine_patches){(void)key;palette_ids.emplace(palette_bytes(patch),0);}
  Json::Array palettes;for(auto&[raw,id]:palette_ids){id=palettes.size();palettes.emplace_back(hex_chunks(raw,10));}
  for(auto&[key,patch]:w.terrain.fine_patches)patches.emplace_back(Json::Object{{"cell",signed_value(key)},{"index_bits",int(patch.index_bits)},{"index_encoding",int(patch.encoding)},{"palette_id",signed_value(palette_ids.at(palette_bytes(patch)))},{"indices",hex(patch.indices)}});
  terrain=Json::Object{{"profile",2},{"width",w.terrain.width},{"height",w.terrain.height},{"cell_mm",2000},{"guard",8},{"micro_divisions",64},{"storage_budget_bytes",signed_value(w.terrain.storage_budget_bytes)},{"geometry_revision",signed_value(w.terrain.geometry_revision)},{"row_encoding","end_x_u32le_kind_u8_theme_u8_revision_u64le_hex_chunks"},{"rows",rows},{"patch_palettes",palettes},{"fine_patches",patches},{"surface_hash",hex(w.terrain.surface_hash())}};
 }
 Json::Array receipts;for(auto&r:w.receipts)receipts.emplace_back(Json::Object{{"command_id",r.command_id},{"payload_hash",hex(r.payload_hash)},{"committed_revision",signed_value(r.committed_revision)},{"effects",r.effects}});
 Json manifest=Json::Object{{"format","SonnWorldS01"},{"world_id",hex(w.id)},{"world_seed",hex(w.seed)},{"definition_hash",hex(w.definition_hash)},{"asset_hash",hex(w.asset_hash)},{"name",w.name},{"tick",signed_value(w.tick)},{"revision",signed_value(w.revision)},{"boundary_ordinal",signed_value(w.boundary_ordinal)},{"input_sequence",signed_value(w.input_sequence)},{"ticks_per_motion_second",20},{"ticks_per_game_day",12000},{"days_per_year",360},{"world_rules",rules},{"age",Json::Object{{"current",w.age.current==Age::Light?"light":"darkness"},{"automatic",w.age.automatic},{"remaining_tick",signed_value(w.age.remaining_tick)},{"light_duration_tick",signed_value(w.age.light_duration_tick)},{"dark_duration_tick",signed_value(w.age.dark_duration_tick)}}},{"population",0},{"awakened_animals",0},{"buildings",0},{"schema_table",Json::Array{1,w.terrain.profile==2?2:1,1,1,1,1,1,1}}};
 return {{1,1,manifest},{2,uint64_t(rows.size()+patches.size()),terrain,w.terrain.profile==2?2u:1u},{3,0,Json::Object{{"records",Json::Array{}},{"tombstones",Json::Array{}}}},{4,0,empty_records()},{5,0,empty_records()},{6,0,Json::Object{{"records",Json::Array{}},{"rng_streams",Json::Array{}},{"frozen_draws",Json::Array{}}}},{7,1,w.provenance},{8,uint64_t(receipts.size()),Json::Object{{"receipts",receipts}}}};
}
void safe_filename(std::string_view f){if(f.empty()||f.size()>160||f=="."||f=="..")throw Error("UNSAFE_PATH","filename");for(char c:f)if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'))throw Error("UNSAFE_PATH","filename");if(f.find("..")!=std::string_view::npos)throw Error("UNSAFE_PATH","parent");}
std::filesystem::path member(const std::filesystem::path&root,std::string_view name){safe_filename(name);auto p=root/std::string(name);if(std::filesystem::is_symlink(p))throw Error("UNSAFE_PATH","symlink");return p;}
void injected(SaveFault actual,SaveFault point){if(actual==point)throw Error("INJECTED_SAVE_FAILURE",std::to_string(int(point)));}
void write_flush(const std::filesystem::path&p,std::span<const uint8_t>b,bool short_write=false,bool fail_flush=false){
#ifdef _WIN32
 HANDLE h=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)throw Error("SAVE_WRITE","CreateFile");bool ok=true;size_t offset=0,limit=short_write?b.size()/2:b.size();while(offset<limit){DWORD n=0,chunk=DWORD(std::min<size_t>(limit-offset,1048576));if(!WriteFile(h,b.data()+offset,chunk,&n,nullptr)||n!=chunk){ok=false;break;}offset+=n;}if(!ok||short_write||fail_flush||!FlushFileBuffers(h)){CloseHandle(h);throw Error("SAVE_FLUSH","write/flush");}if(!CloseHandle(h))throw Error("SAVE_CLOSE","handle");
#else
 int fd=::open(p.c_str(),O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)throw Error("SAVE_WRITE",std::strerror(errno));size_t offset=0,limit=short_write?b.size()/2:b.size();while(offset<limit){auto n=::write(fd,b.data()+offset,limit-offset);if(n<0&&errno==EINTR)continue;if(n<=0){::close(fd);throw Error("SAVE_WRITE",std::strerror(errno));}offset+=size_t(n);}if(short_write||fail_flush||::fsync(fd)!=0){::close(fd);throw Error("SAVE_FLUSH",short_write?"short write":"file fsync");}if(::close(fd)!=0)throw Error("SAVE_CLOSE",std::strerror(errno));
#endif
}
void write_flush(const std::filesystem::path&p,std::string_view s){write_flush(p,std::span(reinterpret_cast<const uint8_t*>(s.data()),s.size()));}
void rename_file(const std::filesystem::path&a,const std::filesystem::path&b,bool replace=true){
#ifdef _WIN32
 if(!MoveFileExW(a.c_str(),b.c_str(),(replace?MOVEFILE_REPLACE_EXISTING:0)|MOVEFILE_WRITE_THROUGH))throw Error("SAVE_RENAME",std::to_string(GetLastError()));
#else
 if(replace){if(::rename(a.c_str(),b.c_str())!=0)throw Error("SAVE_RENAME",std::strerror(errno));}else{if(::link(a.c_str(),b.c_str())!=0)throw Error("SAVE_RENAME",std::strerror(errno));if(::unlink(a.c_str())!=0)throw Error("SAVE_RENAME",std::strerror(errno));}
#endif
}
void directory_flush(const std::filesystem::path&p){
#ifdef _WIN32
 // https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw
 // Prior MoveFileExW WRITE_THROUGH is the native publication barrier; payload handles were flushed.
 // Target-platform installation evidence is still required; this is not a claim about remote filesystems.
 (void)p;
#else
 int fd=::open(p.c_str(),O_RDONLY|O_DIRECTORY);if(fd<0)throw Error("SAVE_DIRECTORY_FLUSH",std::strerror(errno));if(::fsync(fd)!=0){::close(fd);throw Error("SAVE_DIRECTORY_FLUSH",std::strerror(errno));}if(::close(fd)!=0)throw Error("SAVE_DIRECTORY_FLUSH",std::strerror(errno));
#endif
}
std::string unique(){static std::atomic<uint64_t> seq{0};auto n=++seq;
#ifdef _WIN32
 return std::to_string(GetCurrentProcessId())+"-"+std::to_string(n)+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
#else
 return std::to_string(getpid())+"-"+std::to_string(n)+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}
Json pointer_json(const World&w,std::string_view f,const Hash&hash){return Json::Object{{"format","SonnContinue1"},{"world_id",hex(w.id)},{"revision",signed_value(w.revision)},{"checkpoint",std::string(f)},{"file_sha256",hex(hash)}};}
std::optional<Json> pointer_from_bytes(std::span<const uint8_t>b){if(b.size()>65536)throw Error("FILE_LIMIT","Continue pointer");auto j=parse_json(text(b));if(std::holds_alternative<std::nullptr_t>(j.value))return {};keys(j,{"format","world_id","revision","checkpoint","file_sha256"});if(j.at("format").string()!="SonnContinue1")throw Error("CONTINUE_FORMAT","pointer");safe_filename(j.at("checkpoint").string());return j;}
std::optional<Json> current_pointer(const std::filesystem::path&root){auto pending=member(root,"pending.json");auto p=member(root,std::filesystem::exists(pending)?"continue_previous.json":"continue.json");if(!std::filesystem::exists(p))return {};return pointer_from_bytes(read_file(p,65536));}
void restore_reader_gate(const std::filesystem::path&root,std::string_view candidate,bool fail_flush){auto temp=member(root,"restore-pending-"+unique()+".tmp");auto payload=canonical_json(Json::Object{{"candidate",std::string(candidate)},{"display_only",true}});write_flush(temp,std::span(reinterpret_cast<const uint8_t*>(payload.data()),payload.size()),false,fail_flush);rename_file(temp,member(root,"pending.json"));directory_flush(root);}
World from_pointer(const std::filesystem::path&root,const Json&p,const DefinitionPackage&defs){auto b=read_file(member(root,p.at("checkpoint").string()));if(sha256(b)!=hash_from_hex(p.at("file_sha256").string()))throw Error("CONTINUE_HASH","checkpoint");auto w=decode_save(b,defs);if(w.id!=uuid_from_hex(p.at("world_id").string())||w.revision!=positive(p.at("revision"),true))throw Error("CONTINUE_IDENTITY","pointer differs");return w;}
}
Bytes encode_save(const World&w){w.validate(w.definition_hash);auto seg=segments(w);std::vector<Bytes>payload;Bytes table;uint64_t offset=120+64*seg.size();for(auto&s:seg){auto str=canonical_json(s.value);if(str.size()>134217728)throw Error("SAVE_LIMIT","segment");payload.emplace_back(str.begin(),str.end());auto hash=sha256(payload.back());le(table,s.kind,4);le(table,s.schema,4);le(table,offset,8);le(table,str.size(),8);le(table,s.records,8);table.insert(table.end(),hash.begin(),hash.end());offset+=str.size();}if(offset>268435456)throw Error("SAVE_LIMIT","file");Bytes out{'S','O','N','N','S','A','V','1'};le(out,1,2);le(out,0,2);le(out,0,4);le(out,seg.size(),4);le(out,0,4);le(out,w.tick,8);le(out,w.revision,8);out.insert(out.end(),w.id.begin(),w.id.end());out.insert(out.end(),w.definition_hash.begin(),w.definition_hash.end());auto th=sha256(table);out.insert(out.end(),th.begin(),th.end());out.insert(out.end(),table.begin(),table.end());for(auto&p:payload)out.insert(out.end(),p.begin(),p.end());return out;}
World decode_save(std::span<const uint8_t>b,const DefinitionPackage&defs){if(b.size()<120||b.size()>268435456||text(b.first(8))!="SONNSAV1")throw Error("SAVE_HEADER","magic/size");if(get(b,8,2)!=1||get(b,10,2)!=0||get(b,12,4)||get(b,20,4))throw Error("SAVE_VERSION","unsupported flags/version");auto count=get(b,16,4);if(count<8||count>32||120+count*64>b.size())throw Error("SAVE_TABLE","count");if(hash_at(b,56)!=defs.hash)throw Error("DEFINITION_MISSING","save definition hash");auto table=b.subspan(120,size_t(count)*64);if(sha256(table)!=hash_at(b,88))throw Error("SAVE_HASH","table");uint64_t expected=120+count*64;uint32_t previous=0;std::map<uint32_t,Json>seg;std::map<uint32_t,uint64_t>records;std::map<uint32_t,uint32_t>segment_schemas;
 for(size_t i=0;i<count;i++){size_t p=i*64;uint32_t kind=uint32_t(get(table,p,4));auto schema=get(table,p+4,4),off=get(table,p+8,8),length=get(table,p+16,8),rc=get(table,p+24,8);if(kind<=previous||off!=expected||length>134217728||rc>1000000||off>b.size()||length>b.size()-off)throw Error("SAVE_TABLE","bounds/order/records");previous=kind;expected+=length;auto payload=b.subspan(size_t(off),size_t(length));if(sha256(payload)!=hash_at(table,p+32))throw Error("SAVE_HASH","segment");if(kind>8){if((kind&0x80000000u)==0)throw Error("SAVE_SEGMENT","unknown required");continue;}if(schema!=1&&!(kind==2&&schema==2))throw Error("SAVE_SCHEMA","unsupported segment version");segment_schemas.emplace(kind,uint32_t(schema));auto j=parse_json(text(payload));if(canonical_json(j)!=text(payload))throw Error("SAVE_CANONICAL","noncanonical payload");seg.emplace(kind,std::move(j));records.emplace(kind,rc);
 }if(expected!=b.size()||seg.size()!=8)throw Error("SAVE_TABLE","tail/missing required");auto&m=seg.at(1);keys(m,{"format","world_id","world_seed","definition_hash","asset_hash","name","tick","revision","boundary_ordinal","input_sequence","ticks_per_motion_second","ticks_per_game_day","days_per_year","world_rules","age","population","awakened_animals","buildings","schema_table"});if(m.at("format").string()!="SonnWorldS01"||m.at("ticks_per_motion_second").integer()!=20||m.at("ticks_per_game_day").integer()!=12000||m.at("days_per_year").integer()!=360||records[1]!=1||records[7]!=1)throw Error("SAVE_PROFILE","S01/clock/count");auto&schemas=m.at("schema_table").array();if(schemas.size()!=8)throw Error("SAVE_SCHEMA","schema table");for(uint32_t k=1;k<=8;k++)if(schemas[k-1].integer()!=segment_schemas.at(k))throw Error("SAVE_SCHEMA","schema table/entry mismatch");World w;w.id=uuid_from_hex(m.at("world_id").string());w.seed=hash_from_hex(m.at("world_seed").string());w.definition_hash=hash_from_hex(m.at("definition_hash").string());w.asset_hash=hash_from_hex(m.at("asset_hash").string());if(w.asset_hash!=defs.asset_hash)throw Error("ASSET_MISSING","exact admitted asset package required");w.name=m.at("name").string();w.tick=positive(m.at("tick"));w.revision=positive(m.at("revision"),true);w.boundary_ordinal=positive(m.at("boundary_ordinal"));w.input_sequence=positive(m.at("input_sequence"));w.population=positive(m.at("population"));w.awakened_animals=positive(m.at("awakened_animals"));w.buildings=positive(m.at("buildings"));Uuid header_id{};std::copy_n(b.begin()+40,16,header_id.begin());if(w.tick!=get(b,24,8)||w.revision!=get(b,32,8)||w.id!=header_id)throw Error("SAVE_IDENTITY","header/manifest");auto&a=m.at("age");keys(a,{"current","automatic","remaining_tick","light_duration_tick","dark_duration_tick"});auto an=a.at("current").string();if(an!="light"&&an!="darkness")throw Error("INVALID_AGE","wire");w.age={an=="light"?Age::Light:Age::Darkness,a.at("automatic").boolean(),positive(a.at("remaining_tick")),positive(a.at("light_duration_tick"),true),positive(a.at("dark_duration_tick"),true)};for(auto&[k,v]:m.at("world_rules").object())w.rules.emplace(k,v.boolean());
 auto&t=seg.at(2);auto wi=t.at("width").integer(),he=t.at("height").integer();const bool sparse=segment_schemas.at(2)==2;
 if(wi<48||wi>(sparse?4112:1024)||he<48||he>(sparse?4112:1024)||t.at("guard").integer()!=8||t.at("geometry_revision").integer()<1)throw Error("TERRAIN_PROFILE","save dimensions/revision");w.terrain.profile=sparse?2:1;w.terrain.width=int32_t(wi);w.terrain.height=int32_t(he);w.terrain.cell_mm=int32_t(t.at("cell_mm").integer());w.terrain.guard=8;w.terrain.geometry_revision=positive(t.at("geometry_revision"),true);auto&rows=t.at("rows").array();if(rows.size()!=size_t(he))throw Error("SAVE_COUNT","terrain rows");
 if(!sparse){keys(t,{"width","height","cell_mm","guard","geometry_revision","row_encoding","rows","surface_hash"});if((t.at("cell_mm").integer()!=250&&t.at("cell_mm").integer()!=2000)||t.at("row_encoding").string()!="kind_u8_theme_u8_revision_u64le_hex"||records[2]!=rows.size())throw Error("TERRAIN_PROFILE","legacy profile");w.terrain.cells.reserve(size_t(wi)*size_t(he));for(auto&row:rows){if(row.string().size()!=size_t(wi)*20)throw Error("TERRAIN_PROFILE","row bytes");auto raw=unhex(row.string());for(size_t k=0;k<raw.size();k+=10)w.terrain.cells.push_back({TerrainKind(raw[k]),raw[k+1],get(raw,k+2,8)});}
 }else{
  keys(t,{"profile","width","height","cell_mm","guard","micro_divisions","storage_budget_bytes","geometry_revision","row_encoding","rows","patch_palettes","fine_patches","surface_hash"});
  if(t.at("profile").integer()!=2||t.at("cell_mm").integer()!=2000||t.at("micro_divisions").integer()!=64||t.at("row_encoding").string()!="end_x_u32le_kind_u8_theme_u8_revision_u64le_hex_chunks")throw Error("TERRAIN_PROFILE","sparse profile");
  w.terrain.storage_budget_bytes=positive(t.at("storage_budget_bytes"),true);if(w.terrain.storage_budget_bytes>terrain_geometry_budget_bytes)throw Error("CREATION_BUDGET","load geometry budget");
  w.terrain.rows.reserve(size_t(he));uint64_t admitted=sizeof(Terrain)+uint64_t(he)*sizeof(std::vector<CellRun>);
  for(auto&row:rows){auto raw=chunk_bytes(row,14,size_t(wi)*14);std::vector<CellRun>r;r.reserve(raw.size()/14);for(size_t k=0;k<raw.size();k+=14)r.push_back({int32_t(get(raw,k,4)),{TerrainKind(raw[k+4]),raw[k+5],get(raw,k+6,8)}});admitted+=uint64_t(r.capacity())*sizeof(CellRun);if(admitted>w.terrain.storage_budget_bytes)throw Error("CREATION_BUDGET","load RLE budget");w.terrain.rows.push_back(std::move(r));}
  auto&patches=t.at("fine_patches").array();auto&palette_rows=t.at("patch_palettes").array();if(records[2]!=rows.size()+patches.size()||palette_rows.size()>patches.size())throw Error("SAVE_COUNT","terrain rows+patches/palettes");
  std::vector<std::vector<Cell>>palettes;Bytes previous_palette;uint64_t palette_scratch=0;
  for(auto&row:palette_rows){auto raw=chunk_bytes(row,10,40960);if(raw.size()<20||(!previous_palette.empty()&&raw<=previous_palette))throw Error("TERRAIN_PATCH","sorted unique palette registry");previous_palette=raw;palette_scratch+=uint64_t(raw.size()/10)*sizeof(Cell);if(palette_scratch>w.terrain.storage_budget_bytes)throw Error("CREATION_BUDGET","palette registry scratch");std::vector<Cell>p;p.reserve(raw.size()/10);for(size_t k=0;k<raw.size();k+=10){Cell c{TerrainKind(raw[k]),raw[k+1],get(raw,k+2,8)};if(size_t(c.kind)>=8||c.theme>7||!c.revision||c.revision>uint64_t(INT64_MAX))throw Error("TERRAIN_PATCH","palette cell");if(!p.empty()){auto&v=p.back();bool less=v.kind<c.kind||(v.kind==c.kind&&(v.theme<c.theme||(v.theme==c.theme&&v.revision<c.revision)));if(!less)throw Error("TERRAIN_PATCH","sorted unique palette cells");}p.push_back(c);}palettes.push_back(std::move(p));}
  std::vector<bool>palette_used(palettes.size());uint64_t previous_key=0;bool first=true;
  for(auto&p:patches){
   keys(p,{"cell","index_bits","index_encoding","palette_id","indices"});uint64_t key=positive(p.at("cell")),palette_id=positive(p.at("palette_id"));auto bits=p.at("index_bits").integer(),encoding=p.at("index_encoding").integer();
   if((!first&&key<=previous_key)||key>=uint64_t(wi)*uint64_t(he)||(bits!=1&&bits!=2&&bits!=4&&bits!=8&&bits!=12)||encoding<0||encoding>4||palette_id>=palettes.size())throw Error("TERRAIN_PATCH","ordered key/bits/encoding/palette");previous_key=key;first=false;
   auto&index_text=p.at("indices").string();if(index_text.empty()||(index_text.size()&1)||(encoding==0?index_text.size()!=size_t(4096*bits/8)*2:index_text.size()>1024))throw Error("TERRAIN_PATCH","index bytes");
   admitted+=128+uint64_t(palettes[size_t(palette_id)].size())*sizeof(Cell)+uint64_t(index_text.size()/2);if(admitted>w.terrain.storage_budget_bytes)throw Error("CREATION_BUDGET","load patch budget");
   FinePatch patch;patch.index_bits=uint8_t(bits);patch.encoding=uint8_t(encoding);patch.palette=palettes[size_t(palette_id)];patch.indices=unhex(index_text);Bytes(patch.indices).swap(patch.indices);patch.validate();palette_used[size_t(palette_id)]=true;w.terrain.fine_patches.emplace(key,std::move(patch));
  }
  if(std::find(palette_used.begin(),palette_used.end(),false)!=palette_used.end())throw Error("TERRAIN_PATCH","unused palette registry entry");
 }
 w.terrain.validate();if(w.terrain.surface_hash()!=hash_from_hex(t.at("surface_hash").string()))throw Error("SAVE_HASH","surface");
 keys(seg.at(3),{"records","tombstones"});if(!seg.at(3).at("records").array().empty()||!seg.at(3).at("tombstones").array().empty()||records[3])throw Error("UNSUPPORTED_DOMAIN","S01 entities");for(uint32_t k:{4u,5u}){keys(seg.at(k),{"records"});if(!seg.at(k).at("records").array().empty()||records[k])throw Error("UNSUPPORTED_DOMAIN","S01 ledgers/tasks");}keys(seg.at(6),{"records","rng_streams","frozen_draws"});for(auto name:{"records","rng_streams","frozen_draws"})if(!seg.at(6).at(name).array().empty()||records[6])throw Error("UNSUPPORTED_DOMAIN","S01 decisions");w.provenance=seg.at(7);if(w.provenance.at("initial_population").integer()!=0||w.provenance.at("initial_awakened_animals").integer()!=0||w.provenance.at("initial_buildings").integer()!=0)throw Error("SAVE_PROVENANCE","initial zero");
 keys(seg.at(8),{"receipts"});auto&rs=seg.at(8).at("receipts").array();if(records[8]!=rs.size())throw Error("SAVE_COUNT","receipts");for(auto&r:rs){keys(r,{"command_id","payload_hash","committed_revision","effects"});w.receipts.push_back({r.at("command_id").string(),hash_from_hex(r.at("payload_hash").string()),positive(r.at("committed_revision"),true),r.at("effects")});}w.validate(defs.hash);return w;
}
SaveStore::SaveStore(std::filesystem::path p){std::filesystem::create_directories(p);root_=std::filesystem::canonical(p);if(!std::filesystem::is_directory(root_))throw Error("SAVE_ROOT","directory");}
Checkpoint SaveStore::save(const World&w,const DefinitionPackage&defs,SaveFault fault){Checkpoint result;result.world_id=w.id;result.revision=w.revision;bool replaced=false;try{w.validate(defs.hash);auto bytes=encode_save(w);result.file_hash=sha256(bytes);auto unique_id=unique();result.filename=hex(w.id)+"-"+std::to_string(w.revision)+"-"+unique_id+".sonnsave";auto temp=member(root_,"checkpoint-"+unique_id+".tmp"),final=member(root_,result.filename);injected(fault,SaveFault::BeforeWrite);write_flush(temp,bytes,fault==SaveFault::ShortWrite,fault==SaveFault::FileFlush);injected(fault,SaveFault::Readback);auto durable=read_file(temp);if(sha256(durable)!=result.file_hash||decode_save(durable,defs).deterministic_hash()!=w.deterministic_hash())throw Error("SAVE_READBACK","world/hash mismatch");injected(fault,SaveFault::RenameCheckpoint);rename_file(temp,final,false);injected(fault,SaveFault::CheckpointDirectoryFlush);directory_flush(root_);
 // Replacing Continue does not migrate or require decoding the previous world.
 // Preserve its exact pointer bytes, including an incompatible/corrupt pointer;
 // failures before durable publication still recover the same prior state.
 auto prior_path=member(root_,std::filesystem::exists(member(root_,"pending.json"))?"continue_previous.json":"continue.json");
 auto prior_bytes=isolated_previous_pointer_?*isolated_previous_pointer_:std::filesystem::exists(prior_path)?read_file(prior_path):Bytes{'n','u','l','l'};
 auto oldtemp=member(root_,"previous-"+unique_id+".tmp");write_flush(oldtemp,prior_bytes);rename_file(oldtemp,member(root_,"continue_previous.json"));directory_flush(root_);
 // Freeze the reader's prior bytes before any pointer replacement. Moving this
 // already allocated snapshot cannot introduce a new allocation after commit.
 isolated_previous_pointer_=std::move(prior_bytes);
 auto pendingtemp=member(root_,"pending-"+unique_id+".tmp");write_flush(pendingtemp,canonical_json(Json::Object{{"candidate",result.filename},{"display_only",true}}));rename_file(pendingtemp,member(root_,"pending.json"));directory_flush(root_);
 injected(fault,SaveFault::PointerWrite);auto pointertemp=member(root_,"pointer-"+unique_id+".tmp");write_flush(pointertemp,canonical_json(pointer_json(w,result.filename,result.file_hash)));injected(fault,SaveFault::PointerRename);rename_file(pointertemp,member(root_,"continue.json"));replaced=true;injected(fault,SaveFault::PointerDirectoryFlush);directory_flush(root_);
 // pending is a reader gate; removal and its directory barrier confirm publication.
 injected(fault,SaveFault::PendingRemove);std::error_code ec;std::filesystem::remove(member(root_,"pending.json"),ec);if(ec)throw Error("SAVE_PENDING_REMOVE",ec.message());
 injected(fault==SaveFault::PendingDirectoryFlushAndRecoveryFlush?SaveFault::PendingDirectoryFlush:fault,SaveFault::PendingDirectoryFlush);directory_flush(root_);
 result.status=SaveStatus::Committed;result.code="OK";isolated_previous_pointer_.reset();
 }catch(const Error&e){result.status=replaced?SaveStatus::DurabilityUnknown:SaveStatus::Failed;result.code=replaced?"DURABILITY_UNKNOWN":e.code;}catch(const std::exception&){result.status=replaced?SaveStatus::DurabilityUnknown:SaveStatus::Failed;result.code=replaced?"DURABILITY_UNKNOWN":"SAVE_IO";}
 if(result.status==SaveStatus::DurabilityUnknown){
  // Reinstall only the reader gate; never pretend to roll back continue.json.
  // If storage also rejects this flush, the raw in-process latch still protects
  // Continue. After process loss that latch is gone: restart cannot be promised
  // to select the old slot without a durably restored gate. Keep both checkpoints
  // and report this additional failure for explicit recovery.
  try{restore_reader_gate(root_,result.filename,fault==SaveFault::PendingDirectoryFlushAndRecoveryFlush);result.reader_gate_restored=true;}
  catch(const Error&e){result.reader_gate_error=e.code+": "+e.what();}
  catch(const std::exception&e){result.reader_gate_error=std::string("SAVE_IO: ")+e.what();}
 }
 return result;}
std::optional<World> SaveStore::continue_world(const DefinitionPackage&d)const{auto p=isolated_previous_pointer_?pointer_from_bytes(*isolated_previous_pointer_):current_pointer(root_);if(!p)return {};return from_pointer(root_,*p,d);}
std::vector<std::string> SaveStore::recovery_candidates()const{std::vector<std::string> r;for(auto&e:std::filesystem::directory_iterator(root_)){if(e.is_regular_file()&&!e.is_symlink()&&e.path().extension()==".sonnsave")r.push_back(e.path().filename().string());}std::sort(r.begin(),r.end());return r;}
World SaveStore::recover(std::string_view f,const DefinitionPackage&d)const{if(!f.ends_with(".sonnsave"))throw Error("UNSAFE_PATH","checkpoint extension");return decode_save(read_file(member(root_,f)),d);}
void WorldSession::load_checkpoint(std::string_view f,SaveStore&s,const DefinitionPackage&d){if(writer_!=std::this_thread::get_id())throw Error("NOT_SINGLE_WRITER","session");auto w=s.recover(f,d);uint64_t g=generation_+1;if(g>uint64_t(INT64_MAX))throw Error("INTEGER_OVERFLOW","session");auto ready=std::make_unique<Authority>(w,g);auto result=s.save(w,d);if(result.status!=SaveStatus::Committed)throw Error(result.code,"checkpoint Continue publication failed");generation_=g;draft_.reset();active_=std::move(ready);}
} // sonn
