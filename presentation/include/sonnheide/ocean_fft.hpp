#pragma once

// CPU reference adapter for the butterfly algorithm in Abyssal Ocean.
// Original algorithm portions: Copyright (c) 2026 Sacha (@squall01337), MIT.
// See third_party/abyssal-ocean/LICENSE and UPSTREAM.json for source provenance.
// This reference has no window/GPU dependency and is not a production ocean.

#include <complex>
#include <cstddef>
#include <span>
#include <vector>

namespace sonnheide::presentation::ocean {

inline constexpr std::size_t max_reference_resolution = 1024;

struct ButterflyEntry {
    std::complex<double> twiddle;
    std::size_t first;
    std::size_t second;
};

// The source texture is [row][stage] with +i inverse-transform twiddles.
// Supports powers of two from 2 through 1024. Only small transforms are
// validated against direct IDFT; this API makes no real-time performance claim.
class ButterflyLut {
public:
    explicit ButterflyLut(std::size_t resolution);

    [[nodiscard]] std::size_t resolution() const noexcept { return resolution_; }
    [[nodiscard]] std::size_t stages() const noexcept { return stages_; }
    [[nodiscard]] const ButterflyEntry& at(std::size_t stage, std::size_t row) const;

private:
    std::size_t resolution_;
    std::size_t stages_;
    std::vector<ButterflyEntry> entries_;
};

enum class FrequencyLayout {
    Unshifted,
    Centered,
};

enum class InverseNormalization {
    Unnormalized,
    DivideBySampleCount,
};

// Square complex field, row-major [z * N + x]. The defaults match the source:
// x then z positive-exponent butterflies, followed by (-1)^(x+z), with no
// 1/(N*N) division. Centered inputs index frequencies (x-N/2, z-N/2).
// The optional normalized output is a reference comparison mode only.
// Rejects an incorrect size, invalid enum values, non-finite input, or overflow.
[[nodiscard]] std::vector<std::complex<double>> inverse_fft_2d(
    const ButterflyLut& lut,
    std::span<const std::complex<double>> spectrum,
    FrequencyLayout layout = FrequencyLayout::Centered,
    InverseNormalization normalization = InverseNormalization::Unnormalized);

} // namespace sonnheide::presentation::ocean
