
#pragma once

#include <cstdint>

namespace pyrelite
{
    // The random-number surface game systems draw from. Depending on this interface
    // (rather than the concrete Rng) lets a unit test inject a scripted generator —
    // e.g. to assert which way a boxed-in enemy turns — without the real splitmix64.
    class IRng
    {
    public:
        virtual ~IRng() = default;

        // Raw 64-bit value.
        virtual std::uint64_t next() = 0;

        // Uniformly-ish in [0, bound); bound must be > 0.
        virtual std::uint32_t below(std::uint32_t bound) = 0;

        // True with the given probability in percent (0..100).
        virtual bool chance(std::uint32_t percent) = 0;
    };
} // namespace pyrelite
