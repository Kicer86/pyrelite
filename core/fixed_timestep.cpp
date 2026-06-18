
#include "fixed_timestep.h"

#include <algorithm>

namespace pyrelite
{
    FixedTimestep::FixedTimestep(int stepMs, double maxFrameMs)
        : m_stepMs(std::max(1, stepMs)) // guard against an infinite accumulate loop
        , m_maxFrameMs(std::max(maxFrameMs, static_cast<double>(m_stepMs)))
    {
    }

    int FixedTimestep::advance(double frameMs)
    {
        if (frameMs <= 0.0)
            return 0;

        m_accumulator += std::min(frameMs, m_maxFrameMs);

        int steps = 0;
        while (m_accumulator >= m_stepMs)
        {
            m_accumulator -= m_stepMs;
            ++steps;
        }
        return steps;
    }
} // namespace pyrelite
