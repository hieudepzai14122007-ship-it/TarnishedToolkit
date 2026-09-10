#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
#include <string>

namespace tt {
inline constexpr std::array<const char*,8> attributeNames{"Vigor","Mind","Endurance","Strength","Dexterity","Intelligence","Faith","Arcane"};
struct AttributeState {
    std::array<int,8> values{};int level{};uint32_t runeMemory{};
    bool operator==(const AttributeState&)const=default;
};
struct AttributeEdit {AttributeState before;std::array<int,8> requested{};};
// Formula and history saturation follow the pinned PlayerService.SetStat /
// CalculateLevelUpCost. A batch uses NET level gain, so reallocations do not
// manufacture rune-history increases. Spendable runes are never written.
inline uint32_t attributeLevelCost(int nextLevel){
    float base=static_cast<float>(nextLevel+80);
    float cost=base*base*(.02f*std::max(0.f,base-92.f)+.1f)+1.f;
    return static_cast<uint32_t>(cost);
}
inline std::string planAttributes(const AttributeEdit& edit,AttributeState& result){
    if(edit.before.level<1 || edit.before.level>713)return "Current level is invalid.";
    int delta{};
    for(int i=0;i<8;++i){
        if(edit.before.values[i]<1 || edit.before.values[i]>99)return "Current attributes failed validation.";
        if(edit.requested[i]<1 || edit.requested[i]>99)return "Every attribute must be between 1 and 99.";
        delta+=edit.requested[i]-edit.before.values[i];
    }
    if(edit.requested==edit.before.values)return "Change at least one attribute before previewing.";
    int level=edit.before.level+delta;
    if(level<1 || level>713)return "The resulting level must be between 1 and 713. Adjust the attribute total.";
    uint64_t history=edit.before.runeMemory;
    for(int next=edit.before.level+1;next<=level;++next)history+=attributeLevelCost(next);
    result={edit.requested,level,static_cast<uint32_t>(std::min<uint64_t>(history,UINT32_MAX))};
    return {};
}
inline std::array<uint32_t,10> attributeFields(const AttributeState& s){
    std::array<uint32_t,10> fields{};for(int i=0;i<8;++i)fields[i]=static_cast<uint32_t>(s.values[i]);
    fields[8]=static_cast<uint32_t>(s.level);fields[9]=s.runeMemory;return fields;
}
enum class AttributeCommit { Applied, Rejected, RolledBack, Incomplete };
// Store::valid rechecks the current character. Only changed, verified fields
// are written. This is an experimental ordered transaction, NOT an atomic
// engine respec callback. Concurrent engine observation remains possible.
template<class Store> AttributeCommit commitAttributes(Store& store,const AttributeEdit& edit){
    AttributeState target;if(!planAttributes(edit,target).empty())return AttributeCommit::Rejected;
    auto before=attributeFields(edit.before),after=attributeFields(target),expected=before;
    auto matches=[&](const auto& fields){
        if(!store.valid())return false;
        for(int i=0;i<10;++i){uint32_t v{};if(!store.get(i,v) || v!=fields[i])return false;}
        return store.valid();
    };
    if(!matches(before))return AttributeCommit::Rejected;
    std::array<bool,10> attempted{};
    auto rollback=[&]{
        bool restored=true;
        for(int i=9;i>=0;--i)if(attempted[i]){
            uint32_t now{};
            if(!store.valid() || !store.get(i,now)){restored=false;continue;}
            if(now==before[i])continue;
            if(now!=after[i] || !store.put(i,before[i])){restored=false;continue;}
            if(!store.get(i,now) || now!=before[i])restored=false;
        }
        return restored?AttributeCommit::RolledBack:AttributeCommit::Incomplete;
    };
    for(int i=0;i<10;++i)if(before[i]!=after[i]){
        if(!matches(expected))return rollback();
        attempted[i]=true;
        if(!store.put(i,after[i]))return rollback();
        expected[i]=after[i];
    }
    return matches(after)?AttributeCommit::Applied:rollback();
}
}
