#pragma once
namespace sonnheide::native {
// Pure presentation state; no GPU, World mutation or simulation random numbers.
struct EarthCamera {
 double target_x_m{},target_z_m{};
 float height_m{2000},yaw_deg{0},pitch_deg{55};
 bool orthographic{false};
};
enum class GroundDiagnostic { Full, MeanBase, FlatNormal, MeanArm };
}
