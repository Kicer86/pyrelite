
#pragma once

#include <cstdint>

namespace pyrelite {

// Deterministic, cross-platform pseudo-random generator (splitmix64).
// The same seed always yields the same sequence on every platform, so it can
// back reproducible runs / daily seeds and be asserted in headless tests.
// Note: avoids std::*_distribution on purpose (those are not portable).
class Rng {
public:
    explicit Rng(std::uint64_t seed) : state_(seed) {}

    // Raw 64-bit value.
    std::uint64_t next();

    // Uniformly-ish in [0, bound); bound must be > 0.
    std::uint32_t below(std::uint32_t bound);

    // True with the given probability in percent (0..100).
    bool chance(std::uint32_t percent);

private:
    std::uint64_t state_;
};

} // namespace pyrelite
