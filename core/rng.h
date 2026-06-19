
#pragma once

#include <cstdint>

#include "irng.h"

namespace pyrelite
{
    // Deterministic, cross-platform pseudo-random generator (splitmix64).
    // The same seed always yields the same sequence on every platform, so it can
    // back reproducible runs / daily seeds and be asserted in headless tests.
    // Note: avoids std::*_distribution on purpose (those are not portable).
    class Rng : public IRng
    {
    public:
        explicit Rng(std::uint64_t seed) : m_state(seed) {}

        std::uint64_t next() override;
        std::uint32_t below(std::uint32_t bound) override;
        bool chance(std::uint32_t percent) override;

    private:
        std::uint64_t m_state;
    };
} // namespace pyrelite
