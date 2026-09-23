#pragma once
namespace amalur {
// Measured full native HUD: status reaches x=33.4%; boss bar begins x=54.8%.
// Keep modest margins. Bridge crops and game exclusion share these bounds.
struct WristRegions {
 static constexpr float leftWidth=.35f,rightStart=.54f,topHeight=.30f;
 static constexpr float leftNdc=2*leftWidth-1,rightNdc=2*rightStart-1;
 static constexpr float bottomNdc=1-2*topHeight;
};
}
