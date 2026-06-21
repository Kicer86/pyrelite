
#include "rng/rng.h"

namespace pyrelite
{
    std::uint64_t Rng::next()
    {
        // splitmix64
        m_state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }

    std::uint32_t Rng::below(std::uint32_t bound)
    {
        return static_cast<std::uint32_t>(next() % bound);
    }

    bool Rng::chance(std::uint32_t percent)
    {
        return below(100) < percent;
    }
} // namespace pyrelite
