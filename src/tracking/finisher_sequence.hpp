#pragma once
namespace amalur {
// Native Lua993/Fate_Shift_Start explicitly sets player state314/Fate_Shift
// true on_effect_begin (instructions120-131), false on_effect_complete (75-86).
// It initializes sync camera/button mash and handles ReckoningMode separately.
// This is active sequence ownership, never eligibility to initiate a finisher.
// Captured mode83 and84 both occur; mode IDs must not limit the script lifetime.
inline constexpr bool nativeFinisherSequence(bool playerFateShift){
    return playerFateShift;
}
}
