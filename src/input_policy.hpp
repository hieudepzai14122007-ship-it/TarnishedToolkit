#pragma once
#include <cstdint>
namespace tt {
// Kept independent of Windows so focus transitions can be tested without a game.
struct InputPolicy {
    bool captured{};
    enum class Transition { None, Acquire, Release };
    Transition update(bool menu, bool foreground) {
        bool next=menu && foreground;
        if(next==captured)return Transition::None;
        captured=next;
        return next?Transition::Acquire:Transition::Release;
    }
    bool blockWarp(bool foreground) const {return captured && foreground;}
};
struct HoldActivation {
    uint64_t began{};bool tracking{},fired{};
    bool update(bool held,bool foreground,uint64_t now){
        if(!held || !foreground){tracking=false;fired=false;return false;}
        if(!tracking){tracking=true;began=now;}
        if(!fired && now-began>=650){fired=true;return true;}
        return false;
    }
};
}
