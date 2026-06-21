
#pragma once

namespace pyrelite
{
    // Turns variable real-frame durations into a whole number of fixed-size
    // simulation steps, so the core advances in deterministic quanta regardless
    // of frame rate. Leftover sub-step time is carried in the accumulator across
    // calls, so the simulation rate tracks real time without drift. A single
    // frame is clamped to maxFrameMs to bound catch-up after the render loop
    // pauses (e.g. a backgrounded web tab), avoiding a stall / explosion lawine.
    class FixedTimestep
    {
    public:
        FixedTimestep(int stepMs, double maxFrameMs);

        // Add one rendered frame's duration (milliseconds) and return how many
        // fixed steps of stepMs() should run now. A non-positive frame adds
        // nothing and yields zero steps.
        int advance(double frameMs);

        int stepMs() const { return m_stepMs; }

    private:
        int m_stepMs;
        double m_maxFrameMs;
        double m_accumulator = 0.0;
    };
} // namespace pyrelite
