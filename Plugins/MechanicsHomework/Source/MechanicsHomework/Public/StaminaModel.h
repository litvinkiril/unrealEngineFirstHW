#pragma once

#include <algorithm>

// Engine-independent state machine, shared by the character and executable tests.
class FHomeworkStaminaModel
{
public:
    void Configure(float InMaximum, float InDrainRate, float InRecoveryRate)
    {
        Maximum = std::max(1.0f, InMaximum);
        DrainRate = std::max(0.0f, InDrainRate);
        RecoveryRate = std::max(0.0f, InRecoveryRate);
        Value = Maximum;
        bHeld = false;
        bExhaustedUntilRelease = false;
        bSprinting = false;
    }

    void Press()
    {
        // Repeated input callbacks must not clear the exhaustion latch.
        bHeld = true;
        bSprinting = !bExhaustedUntilRelease && Value > 0.0f;
    }

    void Release()
    {
        bHeld = false;
        bExhaustedUntilRelease = false;
        bSprinting = false;
    }

    void Advance(float DeltaSeconds)
    {
        if (DeltaSeconds <= 0.0f)
        {
            return;
        }
        if (bSprinting)
        {
            Value = std::max(0.0f, Value - DrainRate * DeltaSeconds);
            if (Value <= 0.0f)
            {
                bSprinting = false;
                bExhaustedUntilRelease = bHeld;
            }
        }
        else
        {
            Value = std::min(Maximum, Value + RecoveryRate * DeltaSeconds);
        }
    }

    float GetValue() const { return Value; }
    float GetMaximum() const { return Maximum; }
    bool IsSprinting() const { return bSprinting; }

private:
    float Maximum = 100.0f;
    float Value = 100.0f;
    float DrainRate = 25.0f;
    float RecoveryRate = 20.0f;
    bool bHeld = false;
    bool bExhaustedUntilRelease = false;
    bool bSprinting = false;
};
