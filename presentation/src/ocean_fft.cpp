// Butterfly LUT and transform stages adapted from Abyssal Ocean,
// commit 142265f5013b6f27bea4f4f819b832dec75c7bad, index.html.
// New reference adapter ownership follows this project's OWNERSHIP.md.
// The following notice applies to the original algorithm portions:
//
// MIT License
//
// Copyright (c) 2026 Sacha (@squall01337)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "sonnheide/ocean_fft.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace sonnheide::presentation::ocean {
namespace {

bool finite(std::complex<double> value) {
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

} // namespace

ButterflyLut::ButterflyLut(std::size_t resolution)
    : resolution_(resolution), stages_(0) {
    if (resolution < 2 || resolution > max_reference_resolution ||
        (resolution & (resolution - 1)) != 0) {
        throw std::invalid_argument("FFT resolution must be a power of two in [2, 1024]");
    }

    for (auto remaining = resolution; remaining > 1; remaining >>= 1) {
        ++stages_;
    }
    std::vector<std::size_t> reversed(resolution);
    for (std::size_t row = 0; row < resolution; ++row) {
        auto bits = row;
        std::size_t value = 0;
        for (std::size_t bit = 0; bit < stages_; ++bit) {
            value = (value << 1) | (bits & 1);
            bits >>= 1;
        }
        reversed[row] = value;
    }

    entries_.resize(resolution * stages_);
    for (std::size_t stage = 0; stage < stages_; ++stage) {
        const auto span = std::size_t{1} << stage;
        for (std::size_t row = 0; row < resolution; ++row) {
            const auto k = (row * (resolution >> (stage + 1))) % resolution;
            const auto angle = 2.0 * std::numbers::pi_v<double> *
                               static_cast<double>(k) / static_cast<double>(resolution);
            const bool top = row % (span * 2) < span;
            std::size_t first;
            std::size_t second;
            if (stage == 0) {
                first = reversed[top ? row : row - 1];
                second = reversed[top ? row + 1 : row];
            } else {
                first = top ? row : row - span;
                second = top ? row + span : row;
            }
            entries_[row * stages_ + stage] = {
                {std::cos(angle), std::sin(angle)}, first, second};
        }
    }
}

const ButterflyEntry& ButterflyLut::at(std::size_t stage, std::size_t row) const {
    if (stage >= stages_ || row >= resolution_ || entries_.size() != resolution_ * stages_) {
        throw std::out_of_range("butterfly lookup outside FFT texture");
    }
    return entries_[row * stages_ + stage];
}

std::vector<std::complex<double>> inverse_fft_2d(
    const ButterflyLut& lut,
    std::span<const std::complex<double>> spectrum,
    FrequencyLayout layout,
    InverseNormalization normalization) {
    const auto n = lut.resolution();
    if (spectrum.size() != n * n) {
        throw std::invalid_argument("FFT spectrum must contain exactly N*N complex samples");
    }
    if (layout != FrequencyLayout::Unshifted && layout != FrequencyLayout::Centered) {
        throw std::invalid_argument("invalid FFT frequency layout");
    }
    if (normalization != InverseNormalization::Unnormalized &&
        normalization != InverseNormalization::DivideBySampleCount) {
        throw std::invalid_argument("invalid FFT normalization");
    }
    for (const auto sample : spectrum) {
        if (!finite(sample)) {
            throw std::invalid_argument("FFT spectrum contains a non-finite sample");
        }
    }

    std::vector<std::complex<double>> source(spectrum.begin(), spectrum.end());
    std::vector<std::complex<double>> destination(source.size());
    for (std::size_t direction = 0; direction < 2; ++direction) {
        for (std::size_t stage = 0; stage < lut.stages(); ++stage) {
            for (std::size_t z = 0; z < n; ++z) {
                for (std::size_t x = 0; x < n; ++x) {
                    const auto& entry = lut.at(stage, direction == 0 ? x : z);
                    const auto first = direction == 0 ? z * n + entry.first : entry.first * n + x;
                    const auto second = direction == 0 ? z * n + entry.second : entry.second * n + x;
                    const auto value = source[first] + entry.twiddle * source[second];
                    if (!finite(value)) {
                        throw std::overflow_error("FFT stage exceeds finite reference range");
                    }
                    destination[z * n + x] = value;
                }
            }
            source.swap(destination);
        }
    }

    const double scale = normalization == InverseNormalization::DivideBySampleCount
        ? 1.0 / static_cast<double>(n * n) : 1.0;
    for (std::size_t z = 0; z < n; ++z) {
        for (std::size_t x = 0; x < n; ++x) {
            const double sign = layout == FrequencyLayout::Centered && ((x + z) & 1) != 0
                ? -1.0 : 1.0;
            source[z * n + x] *= sign * scale;
        }
    }
    return source;
}

} // namespace sonnheide::presentation::ocean
