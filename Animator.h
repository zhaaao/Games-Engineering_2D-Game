#pragma once
/***************************  Animator: advance column indices (0..3)  ***************************
 * This component advances the sprite-sheet column only; the row is chosen by the caller (e.g., by facing direction).
 * Assumes 4 columns (frameCount=4). frameTime controls playback speed in seconds per frame.
 * Keeps time accumulation internally and wraps on frameCount.
 *************************************************************************************************/
class Animator
{
private:
    int   frameCount = 4;      // Fixed to 4 columns; adjust here if the sprite has a different strip length
    float frameTime = 0.12f;  // Seconds per frame; tuned for a brisk walk cycle
    float acc = 0.f;    // Accumulator for delta time
    int   cur = 0;      // Current column (0..frameCount-1)
    bool  playing = false;  // Gate updates without branching in caller code

public:
    // Clamp to a positive step to avoid division-like artifacts in the while-loop
    void setFrameTime(float t) { frameTime = (t > 0.f ? t : 0.1f); }

    // Start advancing frames on subsequent update() calls
    void start() { playing = true; }

    // Stop playback and reset to frame 0 so the idle pose is consistent
    void stop() { playing = false; cur = 0; }

    // Accumulate dt; advance by whole-frame steps to remain stable across variable frame rates
    void update(float dt)
    {
        if (!playing) return;
        acc += dt;
        while (acc >= frameTime)
        {
            acc -= frameTime;
            cur = (cur + 1) % frameCount;  // Wrap to create a looped strip animation
        }
    }

    // Expose current column index; the renderer combines this with the chosen row
    int current() const { return cur; }
};
