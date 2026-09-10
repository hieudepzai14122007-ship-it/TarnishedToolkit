#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <optional>
#include <sstream>
#include <cmath>
#include <vector>
#include <algorithm>
#include <deque>
#include "attributes.hpp"
#include "compatibility.hpp"

namespace tt {
enum class Session { Unknown, Offline, Online };
enum class Action { Refill, Health, Focus, Stamina, DisableAll, ArmOffline, NoDamage, ClearStatus, SuppressStatus, SimulationSpeed, ReturnBookmark, AddRunes, GrantItem, InspectTarget, ApplyProfile, EditAttributes, TorrentJump, Flight, FlightSpeed };
struct Target {uint64_t handle{};int id{},hp{},maxHp{};float poise{},maxPoise{};bool poiseValid{};};
struct Profile {std::string name{"Custom"};std::array<bool,4> modifiers{};uint8_t statusMask{};float speed{1.f};};
struct Bookmark {std::string name;uint64_t generation{},handle{};uint32_t map{};std::array<float,3> position{};float angle{};};
struct Snapshot {
    bool fingerprint{}, ready{}, adapterReady{}, offlineDeclared{}, playerPresent{};
    bool torrentJump{},torrentJumpAvailable{};
    bool flying{},flightAvailable{};float flightSpeed{3.f};
    Session session{Session::Unknown};
    uint64_t generation{};
    std::array<int,3> current{}, maximum{};
    std::array<bool,4> active{};
    std::array<float,3> position{};
    uint32_t map{};
    bool positionValid{};
    float angle{},speed{1.f};bool speedActive{},speedValid{},riding{};
    uint64_t handle{};
    std::array<int,7> buildup{},resistance{};
    uint8_t statusMask{};bool statusValid{};
    int runes{},level{};bool statsValid{};
    std::array<int,8> attributes{};
    uint32_t runeMemory{};bool attributesEditable{};
    std::vector<Target> targets;uint64_t inspected{};
    uint32_t selectedItem{};int ownedQuantity{-1};bool itemApi{};
    std::string status{"Waiting for game"};
    std::string gameVersion, dataStatus{"Initializing game reader..."};
};
struct Command {
    Action action; uint64_t generation; bool enabled{};
    double value{}; int index{},expectedBefore{-1};uint32_t item{};
    bool confirmed{};uint64_t handle{}; Bookmark bookmark;Profile profile;
    AttributeEdit attributes;
};
// Offline declaration belongs to this process, while modifiers belong to the
// loaded character. Transitions and failures clear effects without erasing consent.
inline void resetTemporaryState(Snapshot& s){
    s.active.fill(false);s.statusMask=0;s.speedActive=false;s.torrentJump=false;s.flying=false;
}
// Called under the runtime state mutex. Session intent is acknowledged now,
// independently of the 64 ms worker and character generation. Only restoration
// is queued; it must not later overwrite a more recent checkbox click.
inline void queueCommand(Snapshot& s,std::deque<Command>& queue,Command c){
    if(c.action==Action::ArmOffline){
        s.offlineDeclared=c.enabled;
        s.status=c.enabled?"Offline confirmation saved for this game session.":"Offline confirmation cleared; stopping temporary modifiers.";
        if(!c.enabled){queue.clear();queue.push_front(c);}
        return;
    }
    if(c.action==Action::DisableAll){
        s.offlineDeclared=false;queue.clear();queue.push_front(c);return;
    }
    if(c.action==Action::Flight && !c.enabled){
        std::erase_if(queue,[](const Command& pending){return pending.action==Action::Flight;});
        queue.push_front(c);return;
    }
    if(queue.size()<32)queue.push_back(c);
}
inline bool validProfile(const Profile& p){return !p.name.empty() && p.name.size()<=48 && p.name.find_first_of("\r\n") == std::string::npos && p.statusMask<128 && std::isfinite(p.speed) && p.speed>=.25f && p.speed<=1.5f;}
inline std::string rejection(const Snapshot& s, const Command& c) {
    if (c.action == Action::DisableAll || ((c.action==Action::TorrentJump || c.action==Action::Flight) && !c.enabled)) return {};
    if (c.action == Action::ArmOffline) return {}; // Declaration only; no game access.
    if (c.action < Action::Refill || c.action > Action::FlightSpeed) return "Unknown command.";
    if (!s.fingerprint) return "Executable fingerprint is not supported.";
    if(s.session==Session::Online)return "Online sessions are not supported.";
    if(c.action==Action::InspectTarget)return {};
    if(s.session!=Session::Offline && !s.offlineDeclared)return "Arm offline testing first. Session mode is not detected automatically.";
    if (!s.adapterReady) return "The experimental adapter is not ready.";
    if (!s.ready) return "A living, fully loaded character is required.";
    if (s.generation != c.generation) return "Character or map changed; command discarded.";
    if(c.action==Action::TorrentJump && (!s.torrentJumpAvailable || !s.riding))return "Mount Torrent first. Repeated jumps require the supported 2.7.1.0 adapter.";
    if((c.action==Action::Flight || c.action==Action::FlightSpeed) && (!s.flightAvailable || s.riding))return "Dismount and load a supported character with readable flight data first.";
    if(c.action==Action::FlightSpeed && (!std::isfinite(c.value) || c.value<.5 || c.value>10))return "Flying speed must be between 0.5 and 10 metres per second.";
    if(c.action==Action::EditAttributes){
        if(!s.attributesEditable || !s.statsValid)return "Character attribute editing is unavailable.";
        if(!c.confirmed)return "Preview and confirm the persistent attribute changes first.";
        if(c.attributes.before!=AttributeState{s.attributes,s.level,s.runeMemory})return "Attribute preview is stale. Read the current values and preview again.";
        AttributeState target;auto reason=planAttributes(c.attributes,target);if(!reason.empty())return reason;
    }
    if(c.action==Action::SimulationSpeed && (!std::isfinite(c.value) || c.value<.25 || c.value>1.5))return "Simulation speed must be 0.25 to 1.5.";
    if((c.action==Action::ClearStatus || c.action==Action::SuppressStatus) && (c.index<0 || c.index>6 || !s.statusValid))return "Status buildup is unavailable or the index is invalid.";
    if(c.action==Action::ApplyProfile && (!validProfile(c.profile) || (c.profile.statusMask && !s.statusValid)))return "Profile failed validation or requested status data is unavailable.";
    if(c.action==Action::AddRunes && (!c.confirmed || !s.statsValid || !std::isfinite(c.value) || c.value<1 || c.value>1000000 || std::floor(c.value)!=c.value || c.expectedBefore!=s.runes || c.value>999999999-s.runes))return "Rune preview is stale or invalid. Preview again (1 to 1,000,000 per action).";
    if(c.action==Action::GrantItem && (!c.confirmed || !s.itemApi || c.item!=s.selectedItem || c.expectedBefore<0 || c.expectedBefore!=s.ownedQuantity))return "Item preview is stale or unavailable. Preview again.";
    if(c.action==Action::ReturnBookmark){
        if(s.flying)return "Stop flying before returning to a bookmark.";
        auto& b=c.bookmark;
        if(!s.positionValid || s.riding || b.generation!=s.generation || b.handle!=s.handle || b.map!=s.map)return "Bookmark must belong to this character, session and loaded map; dismount first.";
        float distance{};for(int i=0;i<3;++i){if(!std::isfinite(b.position[i]))return "Invalid bookmark coordinates.";float d=b.position[i]-s.position[i];distance+=d*d;}
        if(!std::isfinite(b.angle) || distance>100.f*100.f)return "Bookmark must be within 100 metres. Cross-map travel is unsupported.";
    }
    for (size_t i=0;i<3;++i)
        if(s.maximum[i]<=0 || s.maximum[i]>1000000 || s.current[i]<0 || s.current[i]>s.maximum[i])
            return "Resource data failed validation.";
    return {};
}
struct Settings { float scale{1.f}; int menuKey{0x2d}; bool hud{true}; int controllerChord{}; };
inline std::optional<Settings> parseSettings(std::string_view text) {
    std::istringstream in{std::string(text)};
    Settings s; std::string tag,extra; int schema{},hud{};
    if (!(in>>tag>>schema>>s.scale>>s.menuKey>>hud) || tag!="TarnishedToolkit" || (schema!=1 && schema!=2) ||
        !std::isfinite(s.scale) || s.scale<.8f || s.scale>1.8f ||
        (s.menuKey!=0x2d && (s.menuKey<0x70 || s.menuKey>0x7b)) || (hud!=0 && hud!=1)) return {};
    if(schema==2 && (!(in>>s.controllerChord) || s.controllerChord<0 || s.controllerChord>2))return {};
    if(in>>extra)return {};
    s.hud = hud!=0; return s;
}
struct OwnedByte {
    uintptr_t address{}; uint8_t original{}, applied{}; uint64_t generation{}; bool owned{};
    template<class Memory> bool set(Memory& memory,uintptr_t addr,uint8_t value,uint64_t gen) {
        if(owned && (address!=addr || generation!=gen)) return false;
        uint8_t current{}; if(!memory.read(addr,current)) return false;
        if(owned && current!=applied) return false;
        if(!owned) {original=current; address=addr; generation=gen;}
        if(!memory.write(addr,value)) return false;
        applied=value; owned=true; return true;
    }
    template<class Memory> bool restore(Memory& memory,uint64_t gen) {
        if(!owned) return true;
        owned=false;
        if(gen!=generation) return false;
        uint8_t current{};
        return memory.read(address,current) && current==applied && memory.write(address,original);
    }
};
template<class T> struct OwnedValue {
    uintptr_t address{};T original{},applied{};uint64_t generation{};bool owned{};
    template<class Memory> bool set(Memory& m,uintptr_t p,T value,uint64_t gen){
        T now{};if(!m.read(p,now) || (owned && (address!=p || generation!=gen || now!=applied)))return false;
        if(!owned){original=now;address=p;generation=gen;}
        if(!m.write(p,value))return false;applied=value;owned=true;return true;
    }
    template<class Memory> bool restore(Memory& m,uint64_t gen){
        if(!owned)return true;owned=false;T now{};
        return generation==gen && m.read(address,now) && now==applied && m.write(address,original);
    }
};
struct OwnedBit {
    uintptr_t address{};uint8_t mask{},original{};uint64_t generation{};bool owned{};
    template<class Memory> bool set(Memory& m,uintptr_t p,uint8_t bit,uint64_t gen){
        uint8_t now{};if(!m.read(p,now) || (owned && (address!=p || generation!=gen || mask!=bit || !(now&bit))))return false;
        if(!owned){address=p;mask=bit;original=now&bit;generation=gen;}
        if(!m.write(p,static_cast<uint8_t>(now|bit)))return false;owned=true;return true;
    }
    template<class Memory> bool restore(Memory& m,uint64_t gen){
        if(!owned)return true;owned=false;uint8_t now{};
        if(generation!=gen || !m.read(address,now) || !(now&mask))return false;
        return m.write(address,static_cast<uint8_t>((now&~mask)|original));
    }
};
}
