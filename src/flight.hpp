#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <optional>

namespace tt {
// World-space axes: X/Z horizontal, Y vertical, matching the pinned coordinate layout.
inline std::optional<std::array<float,3>> flightPosition(std::array<float,3> position,
        std::array<float,3> axes,float speed,double elapsed) {
    if(!std::isfinite(speed) || speed<.5f || speed>10.f || !std::isfinite(elapsed) || elapsed<0)return {};
    float length{};
    for(int i=0;i<3;++i){
        if(!std::isfinite(position[i]) || std::abs(position[i])>1000000.f ||
           !std::isfinite(axes[i]) || std::abs(axes[i])>1.f)return {};
        length+=axes[i]*axes[i];
    }
    // Normalize diagonals and discard accumulated time after stalls/focus changes.
    float distance=speed*static_cast<float>(std::min(elapsed,.1));
    float divisor=std::max(1.f,std::sqrt(length));
    for(int i=0;i<3;++i){position[i]+=axes[i]/divisor*distance;if(std::abs(position[i])>1000000.f)return {};}
    return position;
}
}
