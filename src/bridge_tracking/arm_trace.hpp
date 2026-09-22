#pragma once
#include <mgs5vr/core.hpp>
#include <cstdint>
namespace amalur {
#pragma pack(push,4)
struct ArmTraceRecord {
    uint32_t sequence{},frame{},stage{},flags{},root{},object{},owner{},slot{};
    uint64_t tick{},rightTick{},leftTick{},headTick{},rawRightTick{},rawLeftTick{};
    mgs5vr::Pose rootWorld{},objectWorld{},head{};
    mgs5vr::Pose raw[2]{},target[2]{},solved[2]{},remapped[2]{};
    mgs5vr::Pose shoulder[2]{},elbow[2]{};
};
constexpr unsigned armTraceCapacity=2048;
struct ArmTraceBuffer {
    uint32_t version{1},pid{},capacity{armTraceCapacity},stride{sizeof(ArmTraceRecord)},published{},dropped{};
    ArmTraceRecord records[armTraceCapacity];
};
#pragma pack(pop)
static_assert(sizeof(ArmTraceRecord)==500);
}
