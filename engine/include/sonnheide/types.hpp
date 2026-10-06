#pragma once
#include <cstdint>
#include <string>
#include <variant>

namespace sonnheide {
using EntityId = std::uint64_t;
using Quantity = std::int64_t;
struct Cell { int x{}; int z{}; auto operator<=>(const Cell&) const = default; };
enum class Surface : std::uint8_t { Water, Land };
enum class Principal { Player, Scheduler, Observer };
// Supplied by the trusted in-process dispatcher, never read from command payloads.
struct AuthorityContext { Principal principal; EntityId actor; };
enum class JobPhase { Reserved, UnderConstruction, Complete, Cancelled };
enum class Status { Accepted, Rejected, RequiresRepreview };
struct PlanReclamation { Cell cell; EntityId materialOwner; };
struct ContributeWork { EntityId job; Quantity units; };
struct CancelReclamation { EntityId job; };
struct PlaceRoad { Cell cell; };
// Geometry probe, not the full v0.6 production PlacePort economic/legal command.
struct KernelPlacePort { Cell anchor; int width; int depth; int rotation; bool ocean; std::uint64_t navigationEpoch; };
using Payload = std::variant<PlanReclamation, ContributeWork, CancelReclamation, PlaceRoad, KernelPlacePort>;
struct Command { std::string id; std::uint64_t expectedRevision; Payload payload; };
struct Result { Status status; std::string reason; std::uint64_t revision; EntityId created; auto operator<=>(const Result&) const = default; };
std::string canonical_command(const Command& command);
std::string status_name(Status status);
} // namespace sonnheide
