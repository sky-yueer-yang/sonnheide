#include "sonnheide/ocean_fft.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
using namespace sonnheide::presentation::ocean;
using Complex = std::complex<double>;

void check(bool condition, const std::string& why) {
    if (!condition) throw std::runtime_error(why);
}

template<class Exception, class F>
void rejects(F&& operation, const std::string& why) {
    bool caught = false;
    try { operation(); } catch (const Exception&) { caught = true; }
    check(caught, why);
}

// Independent O(N^4) IDFT oracle; never used by the presentation adapter.
std::vector<Complex> direct_idft(
    const std::vector<Complex>& spectrum, std::size_t n,
    FrequencyLayout layout, InverseNormalization normalization, double exponent_sign = 1.0) {
    std::vector<Complex> output(n * n);
    const auto center = layout == FrequencyLayout::Centered ? static_cast<long>(n / 2) : 0;
    const double scale = normalization == InverseNormalization::DivideBySampleCount
        ? 1.0 / static_cast<double>(n * n) : 1.0;
    for (std::size_t z = 0; z < n; ++z) {
        for (std::size_t x = 0; x < n; ++x) {
            Complex sum{};
            for (std::size_t kz = 0; kz < n; ++kz) {
                for (std::size_t kx = 0; kx < n; ++kx) {
                    const auto phase_index =
                        (static_cast<long>(kx) - center) * static_cast<long>(x) +
                        (static_cast<long>(kz) - center) * static_cast<long>(z);
                    const auto angle = exponent_sign * 2.0 * std::numbers::pi_v<double> *
                                       static_cast<double>(phase_index) / static_cast<double>(n);
                    sum += spectrum[kz * n + kx] * Complex(std::cos(angle), std::sin(angle));
                }
            }
            output[z * n + x] = sum * scale;
        }
    }
    return output;
}

double error(const std::vector<Complex>& actual, const std::vector<Complex>& expected) {
    check(actual.size() == expected.size(), "comparison size mismatch");
    double worst = 0.0;
    for (std::size_t i = 0; i < actual.size(); ++i) {
        worst = std::max(worst, std::abs(actual[i] - expected[i]));
    }
    return worst;
}

std::vector<Complex> asymmetric_spectrum(std::size_t n) {
    std::vector<Complex> input(n * n);
    for (std::size_t z = 0; z < n; ++z) {
        for (std::size_t x = 0; x < n; ++x) {
            input[z * n + x] = {
                std::sin(static_cast<double>(3 * x + 7 * z) * 0.29) + 0.1 * static_cast<double>(x),
                std::cos(static_cast<double>(11 * x + 2 * z) * 0.43) - 0.07 * static_cast<double>(z)};
        }
    }
    return input;
}

} // namespace

int main() {
    int passed = 0;
    const auto test = [&](const char* name, const std::function<void()>& scenario) {
        try {
            scenario();
            ++passed;
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& exception) {
            std::cerr << "FAIL " << name << ": " << exception.what() << '\n';
            throw;
        }
    };
    try {
        test("positive inverse exponent, both axes, layouts and scaling agree with IDFT", [] {
            for (const std::size_t n : {2, 4, 8, 16}) {
                const ButterflyLut lut(n);
                const auto input = asymmetric_spectrum(n);
                for (const auto layout : {FrequencyLayout::Unshifted, FrequencyLayout::Centered}) {
                    for (const auto normalization : {InverseNormalization::Unnormalized,
                                                     InverseNormalization::DivideBySampleCount}) {
                        const auto actual = inverse_fft_2d(lut, input, layout, normalization);
                        const auto expected = direct_idft(input, n, layout, normalization);
                        check(error(actual, expected) < 1e-10,
                              "butterfly differs from independent direct IDFT at N=" + std::to_string(n));
                    }
                }
            }
            const ButterflyLut lut(8);
            const auto input = asymmetric_spectrum(8);
            const auto actual = inverse_fft_2d(lut, input);
            const auto wrong_direction = direct_idft(input, 8, FrequencyLayout::Centered,
                                                     InverseNormalization::Unnormalized, -1.0);
            check(error(actual, wrong_direction) > 1.0, "fixture cannot distinguish inverse direction");
        });
        test("centered DC preserves upstream unnormalized amplitude", [] {
            constexpr std::size_t n = 8;
            const ButterflyLut lut(n);
            std::vector<Complex> input(n * n);
            input[(n / 2) * n + n / 2] = {2.5, -0.75};
            for (const auto value : inverse_fft_2d(lut, input)) {
                check(std::abs(value - Complex(2.5, -0.75)) < 1e-12,
                      "centered DC was shifted or implicitly divided by N*N");
            }
            for (const auto value : inverse_fft_2d(lut, input, FrequencyLayout::Centered,
                                                   InverseNormalization::DivideBySampleCount)) {
                check(std::abs(value - Complex(2.5, -0.75) / static_cast<double>(n * n)) < 1e-12,
                      "explicit normalization has wrong factor");
            }
            const auto unshifted = inverse_fft_2d(lut, input, FrequencyLayout::Unshifted);
            for (std::size_t z = 0; z < n; ++z) {
                for (std::size_t x = 0; x < n; ++x) {
                    const double sign = ((x + z) & 1) != 0 ? -1.0 : 1.0;
                    check(std::abs(unshifted[z * n + x] - sign * Complex(2.5, -0.75)) < 1e-12,
                          "fixture cannot distinguish centered from unshifted indexing");
                }
            }
        });
        test("Hermitian centered spectrum produces a real spatial field", [] {
            constexpr std::size_t n = 8;
            const ButterflyLut lut(n);
            std::vector<Complex> input(n * n);
            const Complex amplitude(0.7, -0.4);
            input[(n / 2 + 2) * n + n / 2 + 1] = amplitude;
            input[(n / 2 - 2) * n + n / 2 - 1] = std::conj(amplitude);
            const auto output = inverse_fft_2d(lut, input);
            for (std::size_t z = 0; z < n; ++z) {
                for (std::size_t x = 0; x < n; ++x) {
                    const auto angle = 2.0 * std::numbers::pi_v<double> *
                                       static_cast<double>(x + 2 * z) / static_cast<double>(n);
                    const auto expected = 2.0 * (amplitude * Complex(std::cos(angle), std::sin(angle))).real();
                    const auto value = output[z * n + x];
                    check(std::abs(value.imag()) < 1e-12 && std::abs(value.real() - expected) < 1e-12,
                          "Hermitian pair lost reality, axis orientation or amplitude");
                }
            }
        });
        test("resolution and lookup boundaries reject malformed access", [] {
            for (const std::size_t n : {0, 1, 3, 5, 1025}) {
                rejects<std::invalid_argument>([&] { const ButterflyLut rejected(n); },
                                               "invalid resolution admitted");
            }
            rejects<std::invalid_argument>([] {
                const ButterflyLut rejected(std::numeric_limits<std::size_t>::max());
            }, "overflow-sized resolution admitted");
            const ButterflyLut largest(max_reference_resolution);
            check(largest.stages() == 10 && largest.resolution() == 1024, "maximum LUT metadata wrong");
            for (std::size_t stage = 0; stage < largest.stages(); ++stage) {
                for (std::size_t row = 0; row < largest.resolution(); ++row) {
                    const auto& entry = largest.at(stage, row);
                    check(entry.first < largest.resolution() && entry.second < largest.resolution() &&
                          std::abs(std::abs(entry.twiddle) - 1.0) < 1e-12, "invalid LUT entry");
                }
            }
            rejects<std::out_of_range>([&] { (void)largest.at(largest.stages(), 0); }, "invalid stage admitted");
            rejects<std::out_of_range>([&] { (void)largest.at(0, largest.resolution()); }, "invalid row admitted");
            ButterflyLut source(4);
            const ButterflyLut destination(std::move(source));
            check(destination.resolution() == 4 && destination.at(0, 0).first == 0,
                  "moving LUT lost the lookup");
            rejects<std::out_of_range>([&] { (void)source.at(0, 0); }, "moved-from LUT accessed empty storage");
        });
        test("malformed input rejects before returning a field", [] {
            const ButterflyLut lut(4);
            std::vector<Complex> short_input(15), long_input(17), input(16);
            rejects<std::invalid_argument>([&] { (void)inverse_fft_2d(lut, short_input); }, "short spectrum admitted");
            rejects<std::invalid_argument>([&] { (void)inverse_fft_2d(lut, long_input); }, "long spectrum admitted");
            rejects<std::invalid_argument>([&] {
                (void)inverse_fft_2d(lut, input, static_cast<FrequencyLayout>(99));
            }, "invalid layout admitted");
            rejects<std::invalid_argument>([&] {
                (void)inverse_fft_2d(lut, input, FrequencyLayout::Centered,
                                     static_cast<InverseNormalization>(99));
            }, "invalid normalization admitted");
            input[3] = {std::numeric_limits<double>::quiet_NaN(), 0.0};
            rejects<std::invalid_argument>([&] { (void)inverse_fft_2d(lut, input); }, "NaN spectrum admitted");
            input[3] = {0.0, std::numeric_limits<double>::infinity()};
            rejects<std::invalid_argument>([&] { (void)inverse_fft_2d(lut, input); }, "infinite spectrum admitted");
            std::fill(input.begin(), input.end(), Complex(std::numeric_limits<double>::max(), 0.0));
            rejects<std::overflow_error>([&] { (void)inverse_fft_2d(lut, input); }, "non-finite output admitted");
        });
        std::cout << passed << " ocean FFT reference scenarios passed\n";
        return 0;
    } catch (const std::exception&) {
        return 1;
    }
}
