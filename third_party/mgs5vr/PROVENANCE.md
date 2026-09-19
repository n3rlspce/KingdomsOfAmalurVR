# MGS5VR reuse

Source: https://github.com/nikamigaming-create/MGS5VR.
Pinned inspected commit: e20d92223a0a6dfccd8d75eac9d4c7841e635b75.

`include/mgs5vr/core.hpp` and `src/core.cpp` copied unchanged from the research
checkout. `LICENSE` retains the upstream MIT notice. The pose composition,
inversion, validation, and quaternion-vector rotation are used by Amalur's camera
adapter. Additional upstream helpers compile but are not enabled as mod features.

No FOX-engine camera offsets or projection-sign assumptions are reused.

