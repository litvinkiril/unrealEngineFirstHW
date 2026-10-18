#include "StaminaModel.h"
#include <cmath>
#include <cstdlib>
#include <iostream>

static void Check(bool Condition, const char* Message)
{
    if (!Condition)
    {
        std::cerr << "FAIL: " << Message << '\n';
        std::exit(1);
    }
}

static bool Near(float A, float B) { return std::fabs(A - B) < 0.002f; }

int main()
{
    FHomeworkStaminaModel Model;
    Model.Configure(100.0f, 25.0f, 20.0f);
    Check(!Model.IsSprinting() && Near(Model.GetValue(), 100.0f), "initial state");
    Model.Press();
    Model.Advance(1.0f);
    Check(Model.IsSprinting() && Near(Model.GetValue(), 75.0f), "sprint consumes per second");
    Model.Release();
    Model.Advance(0.5f);
    Check(!Model.IsSprinting() && Near(Model.GetValue(), 85.0f), "release restores stamina");
    Model.Advance(1000.0f);
    Check(Near(Model.GetValue(), 100.0f), "recovery capped at maximum");
    Model.Press();
    Model.Advance(4.0f);
    Check(!Model.IsSprinting() && Near(Model.GetValue(), 0.0f), "exhaustion stops sprint");
    Model.Advance(1.0f);
    Model.Press();
    Check(!Model.IsSprinting() && Near(Model.GetValue(), 20.0f), "held key cannot restart after exhaustion");
    Model.Release();
    Model.Press();
    Check(Model.IsSprinting(), "release then press permits restart");
    Model.Advance(1000.0f);
    Check(!Model.IsSprinting() && Near(Model.GetValue(), 0.0f), "large frame clamps to zero");
    Model.Release();
    Model.Press();
    Check(!Model.IsSprinting(), "cannot sprint with zero stamina");
    Model.Advance(1.0f);
    Check(!Model.IsSprinting(), "recovery alone never restarts sprint");
    Model.Release();
    Model.Press();
    Check(Model.IsSprinting(), "new press after recovery works");
    const float Before = Model.GetValue();
    Model.Advance(0.0f);
    Model.Advance(-1.0f);
    Check(Near(Model.GetValue(), Before), "invalid timestep has no effect");

    for (int Frames : {30, 60, 144})
    {
        Model.Configure(100.0f, 25.0f, 20.0f);
        Model.Press();
        for (int Frame = 0; Frame < Frames * 2; ++Frame)
        {
            Model.Advance(1.0f / static_cast<float>(Frames));
        }
        Check(Near(Model.GetValue(), 50.0f), "drain independent of frame rate");
        Model.Release();
        for (int Frame = 0; Frame < Frames; ++Frame)
        {
            Model.Advance(1.0f / static_cast<float>(Frames));
        }
        Check(Near(Model.GetValue(), 70.0f), "recovery independent of frame rate");
    }
    Model.Configure(-1.0f, -25.0f, -20.0f);
    Check(Near(Model.GetMaximum(), 1.0f), "configuration clamps invalid maximum");
    Model.Press();
    Model.Advance(10.0f);
    Check(Near(Model.GetValue(), 1.0f), "configuration clamps negative drain");
    std::cout << "PASS: stamina, exhaustion latch, release, recovery, bounds and frame rates\n";
}
