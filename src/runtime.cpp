#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include "runtime.hpp"
#include "catalog.hpp"
#include "horse_jump.hpp"
#include "flight.hpp"
#include "input.hpp"
#include <fstream>
#include <mutex>
#include <deque>
#include <vector>
#include <iomanip>
#include <algorithm>

namespace tt {
namespace {
std::mutex stateMutex, logMutex;
Snapshot state;
Settings prefs;
std::deque<Command> commands;
uintptr_t base{};
uint64_t generation{};
std::array<OwnedByte,3> overrides;
OwnedBit noDamage;
OwnedByte noGravity;
uint64_t lastFlightTick{};
OwnedValue<float> simulation;
uint32_t selectedItem{};uint64_t inspected{},lastQuantityTick{};int cachedQuantity=-1;
bool entryPoints{};
bool recognized{};
std::string lastDataStatus;
// MIT-licensed TarnishedTool 2a7a76939d3dd21c233ee4f1e5df3b2dddf0e49c,
// Memory/Offsets.cs: Version2_7_0. See docs/COMPATIBILITY.md.
constexpr uintptr_t worldRva=0x3D69FF8, menuRva=0x3D6F820, flagsRva=0x3D6A210;
constexpr std::array<uintptr_t,3> resourceOffsets{0x138,0x148,0x154};
constexpr std::array<uintptr_t,3> flagOffsets{0,5,4};
constexpr uintptr_t gameDataRva=0x3D61F98,flipperRva=0x458DB58,mapItemRva=0x3D6BAC0;
constexpr uintptr_t spawnRva=0x561400,quantityRva=0x785E50,runeRva=0x25E0E0;
#include "engine_headers.inc"
struct Memory {
    template<class T> bool read(uintptr_t p,T& out) {
        SIZE_T size{}; return p>0x10000 && ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),&out,sizeof(T),&size) && size==sizeof(T);
    }
    template<class T> bool write(uintptr_t p,T value) {
        SIZE_T size{}; return p>0x10000 && WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),&value,sizeof(T),&size) && size==sizeof(T);
    }
} memory;
struct Context {
    uintptr_t world{},player{},modules{},data{},resist{},physics{},gameData{},flipper{};
    uint64_t handle{};uint32_t map{};
    bool same(const Context& other)const{return player==other.player && modules==other.modules && data==other.data && handle==other.handle && map==other.map && resist==other.resist && physics==other.physics && gameData==other.gameData && flipper==other.flipper;}
} current;
bool sampleContext(Context& c,std::string* issue=nullptr){
    uintptr_t manager{};
    auto fail=[&](const char* why){if(issue)*issue=why;return false;};
    if(!memory.read(base+worldRva,c.world) || !c.world)return fail("World manager is unavailable.");
    if(!memory.read(c.world+0x1E508,c.player) || !c.player)return fail("No loaded player: load a character and finish loading.");
    if(!memory.read(c.player+8,c.handle))return fail("Player identity could not be read.");
    if(!memory.read(c.player+0x190,c.modules) || !c.modules)return fail("Player modules are unavailable.");
    if(!memory.read(c.modules,c.data) || !c.data)return fail("Player resource data is unavailable.");
    if(!memory.read(c.player+0x6D0,c.map))return fail("Player map data could not be read.");
    memory.read(c.modules+0x20,c.resist);memory.read(c.modules+0x68,c.physics);
    if(memory.read(base+gameDataRva,manager))memory.read(manager+8,c.gameData);
    memory.read(base+flipperRva,c.flipper);return true;
}
bool stillCurrent(){Context next;return recognized && sampleContext(next) && current.same(next);}
bool stopFlight(){
    if(!noGravity.owned){state.flying=false;lastFlightTick=0;return true;}
    Context next;
    bool same=recognized && sampleContext(next) && next.player==current.player && next.handle==current.handle &&
        next.physics==current.physics && noGravity.address==next.physics+0x1D6;
    bool restored=noGravity.restore(memory,same?generation:UINT64_MAX);
    state.flying=false;lastFlightTick=0;return restored;
}
bool flightData(std::array<float,3>& position,uintptr_t& fall){
    uint8_t gravity{},loaded{},fading{};float timer{};uintptr_t menu{};int hp{};
    // Offsets.cs ChrPhysicsOffsets and Resources.resx NoClip_InAirTimer, pinned MIT source.
    return state.ready && !state.riding && stillCurrent() && current.physics &&
        memory.read(base+menuRva,menu) && menu && memory.read(menu+0x94,loaded) && loaded==1 &&
        memory.read(menu+0x96,fading) && fading==0 && memory.read(current.data+0x138,hp) && hp>0 &&
        memory.read(current.physics+0x1D6,gravity) && gravity<=1 &&
        memory.read(current.physics+0x70,position) && flightPosition(position,{},state.flightSpeed,0).has_value() &&
        memory.read(current.modules+0x70,fall) && fall && memory.read(fall+0x18,timer) &&
        std::isfinite(timer) && timer>=0.f && timer<100000.f;
}
struct AttributeStore {
    bool valid(){
        uintptr_t menu{};uint8_t loaded{},fading{};int hp{};
        return stillCurrent() && current.gameData && memory.read(base+menuRva,menu) && memory.read(menu+0x94,loaded) && loaded==1 && memory.read(menu+0x96,fading) && fading==0 && memory.read(current.data+0x138,hp) && hp>0;
    }
    uintptr_t address(int i){return current.gameData+(i<8?0x3C+i*4:i==8?0x68:0x70);}
    bool get(int i,uint32_t& value){return i>=0 && i<10 && memory.read(address(i),value);}
    bool put(int i,uint32_t value){return i>=0 && i<10 && valid() && memory.write(address(i),value);}
};
bool auditAttributes(std::string_view phase,const AttributeEdit& edit,const AttributeState& target){
    std::ofstream out(dataDir()/"attribute-edits.log",std::ios::app);
    out<<GetTickCount64()<<" "<<phase<<" generation="<<generation<<" map="<<current.map<<" level="<<edit.before.level<<"->"<<target.level<<" rune_history="<<edit.before.runeMemory<<"->"<<target.runeMemory;
    for(int i=0;i<8;++i)out<<" "<<attributeNames[i]<<"="<<edit.before.values[i]<<"->"<<target.values[i];
    out<<"\n";out.flush();return static_cast<bool>(out);
}
bool matchEntry(uintptr_t rva,const std::array<unsigned char,16>& expected){std::array<unsigned char,16> actual{};return memory.read(base+rva,actual) && actual==expected;}
int quantity(uint32_t id){
    if(!entryPoints || !stillCurrent())return -1;
    // Calling convention from pinned Resources.resx ItemSpawn: RCX=&ID, EAX=count.
    int n=reinterpret_cast<int(*)(const uint32_t*)>(base+quantityRva)(&id);
    return n>=0 && n<=1000000?n:-1;
}
std::string fingerprint(const std::filesystem::path& path) {
    BCRYPT_ALG_HANDLE alg{}; BCRYPT_HASH_HANDLE hash{};
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) return {};
    if(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)<0) {BCryptCloseAlgorithmProvider(alg,0); return {};}
    std::ifstream input(path,std::ios::binary); std::vector<unsigned char> buf(1<<20);
    bool ok=input.good();
    while(input) {input.read(reinterpret_cast<char*>(buf.data()),buf.size()); auto n=input.gcount(); if(n && BCryptHashData(hash,buf.data(),static_cast<ULONG>(n),0)<0) ok=false;}
    std::array<unsigned char,32> digest{};
    ok=ok && !input.bad() && BCryptFinishHash(hash,digest.data(),digest.size(),0)>=0;
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg,0);
    if(!ok) return {};
    std::ostringstream result; for(auto b:digest) result<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(b);
    return result.str();
}
bool clearOverrides(bool sameEntity=true) {
    horseJump::stop();
    bool restored=stopFlight();
    for(auto& o:overrides) restored=o.restore(memory,0) && restored;
    restored=noDamage.restore(memory,sameEntity?generation:UINT64_MAX) && restored;
    uintptr_t flipper{};
    bool sameFlipper=simulation.owned && memory.read(base+flipperRva,flipper) && flipper && simulation.address==flipper+0x2CC;
    restored=simulation.restore(memory,sameFlipper?0:UINT64_MAX) && restored;
    resetTemporaryState(state);
    if(!restored) {state.status="Restoration incomplete: ownership or generation changed."; log(state.status);}
    return restored;
}
bool setModifier(size_t i,bool enabled){
    bool ok=false;
    if(i<3)ok=enabled?overrides[i].set(memory,base+flagsRva+flagOffsets[i],1,0):overrides[i].restore(memory,0);
    else if(i==3 && stillCurrent())ok=enabled?noDamage.set(memory,current.data+0x19B,2,generation):noDamage.restore(memory,generation);
    if(ok)state.active[i]=enabled;return ok;
}
bool setSpeed(float value){
    if(!state.speedValid || !current.flipper)return value==1.f && !simulation.owned;
    bool ok=value==1.f?simulation.restore(memory,0):simulation.set(memory,current.flipper+0x2CC,value,0);
    if(ok)state.speedActive=value!=1.f;return ok;
}
void sampleTargets(){
    state.targets.clear();state.inspected=inspected;uintptr_t begin{},end{};
    if(!memory.read(current.world+0x1F1B8,begin) || !memory.read(current.world+0x1F1C0,end) || end<begin || (end-begin)%8 || (end-begin)/8>4096)return;
    for(uintptr_t p=begin;p<end && state.targets.size()<128;p+=8){
        uintptr_t chr{},modules{},data{},poise{};Target t;
        if(!memory.read(p,chr) || !chr || chr==current.player || !memory.read(chr+8,t.handle) || !memory.read(chr+0x64,t.id) || !memory.read(chr+0x190,modules) || !memory.read(modules,data))continue;
        if(!memory.read(data+0x138,t.hp) || !memory.read(data+0x13C,t.maxHp) || t.maxHp<=0 || t.maxHp>10000000 || t.hp<0 || t.hp>t.maxHp)continue;
        t.poiseValid=memory.read(modules+0x40,poise) && memory.read(poise+0x10,t.poise) && memory.read(poise+0x14,t.maxPoise) && std::isfinite(t.poise) && std::isfinite(t.maxPoise) && t.poise>=0 && t.maxPoise>0 && t.maxPoise<100000;
        state.targets.push_back(t);
    }
}
void execute(Command& c){
    if(c.action==Action::Flight && !c.enabled){state.status=stopFlight()?"Flying stopped; original gravity restored.":"Flying stopped, but gravity ownership changed; restoration could not be verified.";log(state.status);return;}
    if(c.action==Action::TorrentJump && !c.enabled){horseJump::stop();state.torrentJump=false;state.status="Torrent jumps returned to normal.";return;}
    if(c.action==Action::DisableAll){bool restored=clearOverrides(stillCurrent());state.status=restored?"Temporary modifiers stopped. Persistent grants are not undone.":"Restoration incomplete: an owned value changed; see log. Persistent grants are not undone.";log(state.status);return;}
    auto reason=rejection(state,c);if(!reason.empty()){state.status=reason;log("Rejected: "+reason);return;}
    if(c.action==Action::ArmOffline){bool restored=c.enabled || clearOverrides(stillCurrent());if(!restored)state.status="Temporary restoration incomplete. See log.";return;}
    if(c.action==Action::InspectTarget){inspected=c.handle;return;}
    if(!stillCurrent()){clearOverrides(false);state.status="Character changed before execution; action discarded and modifiers stopped.";return;}
    bool ok=true;
    switch(c.action){
    case Action::Flight:{
        std::array<float,3> position{};uintptr_t fall{};
        ok=flightData(position,fall) && noGravity.set(memory,current.physics+0x1D6,1,generation);
        if(ok){state.flying=true;lastFlightTick=GetTickCount64();}break;
    }
    case Action::FlightSpeed:state.flightSpeed=static_cast<float>(c.value);break;
    case Action::EditAttributes:{
        AttributeState target;
        if(!planAttributes(c.attributes,target).empty() || !auditAttributes("requested",c.attributes,target)){
            state.status="Attribute edit stopped: validation or audit recording failed.";log(state.status);return;
        }
        AttributeStore store;auto result=commitAttributes(store,c.attributes);
        if(result==AttributeCommit::Applied){
            state.attributes=target.values;state.level=target.level;state.runeMemory=target.runeMemory;
            bool restored=clearOverrides(stillCurrent());
            state.status="Attributes stored and read back; level "+std::to_string(target.level)+". Reload the character to refresh derived stats. Temporary modifiers stopped.";
            if(!restored)state.status+=" Temporary restoration incomplete; see log.";
        }else if(result==AttributeCommit::Rejected){
            state.status="Attribute edit rejected: current stats or character no longer match the preview. No writes attempted.";
        }else{
            clearOverrides(stillCurrent());
            state.status=result==AttributeCommit::RolledBack?"Attribute edit failed; owned changes rolled back. Temporary modifiers stopped.":"Attribute edit incomplete: character or owned values changed; full rollback could not be verified. Temporary modifiers stopped. See attribute-edits.log.";
        }
        const char* outcome=result==AttributeCommit::Applied?"applied":result==AttributeCommit::Rejected?"rejected":result==AttributeCommit::RolledBack?"rolled_back":"incomplete";
        if(!auditAttributes(outcome,c.attributes,target))state.status+=" Result audit could not be saved.";
        log(state.status);return;
    }
    case Action::TorrentJump:state.torrentJump=true;break;
    case Action::Refill:
        for(size_t i=0;i<3;++i){int maximum{};if(!stillCurrent() || !memory.read(current.data+resourceOffsets[i]+4,maximum) || maximum!=state.maximum[i]){ok=false;break;}ok=memory.write(current.data+resourceOffsets[i],maximum) && ok;}break;
    case Action::Health:case Action::Focus:case Action::Stamina:ok=setModifier(static_cast<size_t>(c.action)-1,c.enabled);break;
    case Action::NoDamage:ok=setModifier(3,c.enabled);break;
    case Action::ClearStatus:ok=memory.write(current.resist+0x10+c.index*4,0);break;
    case Action::SuppressStatus:if(c.enabled)state.statusMask|=uint8_t(1u<<c.index);else state.statusMask&=uint8_t(~(1u<<c.index));break;
    case Action::SimulationSpeed:ok=setSpeed(static_cast<float>(c.value));break;
    case Action::ReturnBookmark:{
        std::array<float,3> local{};ok=memory.read(current.physics+0x70,local);
        if(ok){for(int i=0;i<3;++i)local[i]+=c.bookmark.position[i]-state.position[i];ok=stillCurrent() && memory.write(current.physics+0x70,local);if(ok)ok=memory.write(current.player+0x6CC,c.bookmark.angle);}break;
    }
    case Action::AddRunes:{
        if(!entryPoints || !current.gameData){ok=false;break;}int before{};
        if(!memory.read(current.gameData+0x6C,before) || before!=c.expectedBefore){ok=false;break;}
        // Native engine workflow from PlayerService.GiveRunes, not a raw stat edit.
        reinterpret_cast<void(*)(void*,int)>(base+runeRva)(reinterpret_cast<void*>(current.gameData),static_cast<int>(c.value));
        int after{};ok=stillCurrent() && memory.read(current.gameData+0x6C,after);
        state.status=ok?"Rune action: before "+std::to_string(before)+", observed after "+std::to_string(after)+" (persistent).":"Rune result could not be confirmed; no retry.";log(state.status);return;
    }
    case Action::GrantItem:{
        auto item=findItem(c.item);
        if(!canGrant(item,c.value,c.expectedBefore)){ok=false;break;}
        int before=quantity(item->id);uintptr_t manager{};
        if(before!=c.expectedBefore || !memory.read(base+mapItemRva,manager) || !manager || !stillCurrent()){ok=false;break;}
        // Exact argument layout follows pinned Resources.resx ItemSpawn.
        struct Spawn{int count;uint32_t id;int quantity,unknown,ash;} args{1,item->id,static_cast<int>(c.value),-1,-1};
        alignas(16) std::array<unsigned char,4096> result{};
        reinterpret_cast<void(*)(void*,Spawn*,void*,int)>(base+spawnRva)(reinterpret_cast<void*>(manager),&args,result.data(),0);
        int after=quantity(item->id);state.ownedQuantity=after;cachedQuantity=after;lastQuantityTick=GetTickCount64();
        state.status="Item action: "+std::string(item->name)+"; before "+std::to_string(before)+", observed after "+std::to_string(after)+". Persistent; no retry.";log(state.status);return;
    }
    case Action::ApplyProfile:
        if(!stopFlight()){ok=false;break;}
        horseJump::stop();state.torrentJump=false;
        for(size_t i=0;i<4;++i)if(!setModifier(i,c.profile.modifiers[i])){ok=false;break;}
        if(ok){state.statusMask=state.statusValid?c.profile.statusMask:0;ok=setSpeed(c.profile.speed);}break;
    default:ok=false;break;
    }
    if(!ok){clearOverrides(stillCurrent());}
    state.status=ok?"Experimental action applied. Check the result in-game.":"Action failed; temporary modifiers stopped. See log.";log(state.status);
}
}
std::atomic_bool menuOpen{true};
std::atomic_int menuKey{VK_INSERT};
std::filesystem::path dataDir() {
    wchar_t buf[32768]{}; DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",buf,32768);
    std::filesystem::path path = n && n<32768 ? std::filesystem::path(buf) : std::filesystem::temp_directory_path();
    path/=L"TarnishedToolkit"; std::error_code ec; std::filesystem::create_directories(path,ec); return path;
}
void log(std::string_view message) {
    std::lock_guard lock(logMutex); auto path=dataDir()/"toolkit.log"; std::error_code ec;
    if(std::filesystem::file_size(path,ec)>1024*1024 && !ec) {
        std::filesystem::remove(dataDir()/"toolkit.previous.log",ec);
        std::filesystem::rename(path,dataDir()/"toolkit.previous.log",ec);
    }
    std::ofstream out(path,std::ios::app); out<<GetTickCount64()<<" "<<message<<"\n";
}
Settings settings() {std::lock_guard lock(stateMutex); return prefs;}
bool saveSettings(Settings p) {
    std::ostringstream out; out<<"TarnishedToolkit 2 "<<p.scale<<" "<<p.menuKey<<" "<<int(p.hud)<<" "<<p.controllerChord<<"\n";
    if(!parseSettings(out.str())) return false;
    auto path=dataDir()/"settings.txt", tmp=dataDir()/"settings.tmp";
    {std::ofstream file(tmp,std::ios::trunc); file<<out.str(); file.flush(); if(!file) return false;}
    if(!MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return false;
    std::lock_guard lock(stateMutex); prefs=p; menuKey=p.menuKey; return true;
}
void initialize() {
    std::lock_guard lock(stateMutex);
    wchar_t exe[32768]{}; GetModuleFileNameW(nullptr,exe,32768);
    const auto sha=fingerprint(exe);
    state.gameVersion=supportedVersion(sha);recognized=!state.gameVersion.empty();
    base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    state.fingerprint=recognized;
    state.torrentJumpAvailable=recognized && horseJump::initialize(base,sha);
    entryPoints=recognized && matchEntry(spawnRva,itemSpawnHeader) && matchEntry(quantityRva,itemQuantityHeader) && matchEntry(runeRva,giveRunesHeader);
    state.status=recognized?"Build matched. Experimental controls start disarmed.":"Unknown executable: gameplay reads and writes disabled.";
    log("0.2.6 beta loaded. Executable SHA256: "+sha); log(state.status);
    auto config=dataDir()/"settings.txt";
    if(std::filesystem::exists(config)) {
        std::ifstream file(config); std::string text((std::istreambuf_iterator<char>(file)),{});
        auto parsed=parseSettings(text);
        if(parsed) prefs=*parsed; else {state.status="Invalid settings: defaults restored. Gameplay writes remain locked."; log(state.status);}
    }
    menuKey=prefs.menuKey;
}
Snapshot snapshot() {std::lock_guard lock(stateMutex); return state;}
void selectItem(uint32_t id){std::lock_guard lock(stateMutex);selectedItem=id;state.selectedItem=id;state.ownedQuantity=-1;lastQuantityTick=0;cachedQuantity=-1;}
void enqueue(Command command) {
    std::lock_guard lock(stateMutex);
    if(command.action==Action::DisableAll || (command.action==Action::ArmOffline && !command.enabled) || (command.action==Action::TorrentJump && !command.enabled))horseJump::stop();
    queueCommand(state,commands,std::move(command));
}
void disableAll() {enqueue({Action::DisableAll,0,false});}
void poll() {
    std::lock_guard lock(stateMutex);
    bool wasReady=state.ready;
    state.dataStatus=recognized?"Reading player data...":"Unsupported game executable. This mod supports exact file builds 2.7.0.0 and 2.7.1.0; an update requires a matching mod profile.";
    Context next;bool valid=recognized && sampleContext(next,&state.dataStatus);
    if(!valid || !current.same(next)){
        clearOverrides(valid && current.player==next.player && current.data==next.data && current.handle==next.handle);
        if(current.player || next.player)++generation;
        current=valid?next:Context{};cachedQuantity=-1;lastQuantityTick=0;
    }else current=next;
    state.generation=generation;state.handle=current.handle;state.map=current.map;
    state.ready=false;state.playerPresent=false;state.positionValid=false;state.statusValid=false;state.statsValid=false;state.speedValid=false;state.attributesEditable=false;state.flightAvailable=false;
    state.adapterReady=recognized;state.itemApi=entryPoints;state.session=Session::Unknown;
    state.current.fill(0);state.maximum.fill(0);state.targets.clear();state.riding=true;
    state.selectedItem=selectedItem;state.ownedQuantity=-1;
    uintptr_t menu{};uint8_t loaded{},fading{};
    if(valid){
        if(!memory.read(base+menuRva,menu) || !menu || !memory.read(menu+0x94,loaded) || !memory.read(menu+0x96,fading)){
            valid=false;state.dataStatus="Loading-state data could not be read.";
        }else if(loaded!=1){valid=false;state.dataStatus="Character loading is not complete (loaded flag "+std::to_string(loaded)+").";}
        else if(fading!=0){valid=false;state.dataStatus="Screen transition is active (fade flags "+std::to_string(fading)+").";}
    }
    if(valid){
        for(size_t i=0;i<3;++i){
            valid=memory.read(current.data+resourceOffsets[i],state.current[i]) && memory.read(current.data+resourceOffsets[i]+4,state.maximum[i]) && valid;
            valid=valid && state.maximum[i]>0 && state.maximum[i]<=1000000 && state.current[i]>=0 && state.current[i]<=state.maximum[i];
        }
        state.playerPresent=valid;state.ready=valid && state.current[0]>0;
        state.dataStatus=!valid?"HP, FP or stamina data is unreadable or outside the expected range.":!state.ready?"Player has no health; waiting for respawn.":"Character data available.";
        state.positionValid=state.ready && memory.read(current.player+0x6C0,state.position) && memory.read(current.player+0x6CC,state.angle) && std::isfinite(state.angle);
        for(float x:state.position)state.positionValid=state.positionValid && std::isfinite(x);
        state.statusValid=state.ready && current.resist;
        for(int i=0;i<7;++i){
            bool statusOk=memory.read(current.resist+0x10+4*i,state.buildup[i]) && memory.read(current.resist+0x2C+4*i,state.resistance[i]);
            state.statusValid=state.statusValid && statusOk && state.buildup[i]>=0 && state.buildup[i]<=1000000 && state.resistance[i]>0 && state.resistance[i]<=1000000;
        }
        state.statsValid=state.ready && current.gameData && memory.read(current.gameData+0x6C,state.runes) && memory.read(current.gameData+0x68,state.level) && memory.read(current.gameData+0x3C,state.attributes) && state.runes>=0 && state.runes<=999999999 && state.level>=1 && state.level<=713;
        for(int x:state.attributes)state.statsValid=state.statsValid && x>=1 && x<=99;
        state.attributesEditable=state.statsValid && memory.read(current.gameData+0x70,state.runeMemory);
        state.speedValid=current.flipper && memory.read(current.flipper+0x2CC,state.speed) && std::isfinite(state.speed) && state.speed>=.01f && state.speed<=10.f;
        uintptr_t ride{},node{};int riding{};
        if(memory.read(current.modules+0xE8,ride) && memory.read(ride+0x10,node) && memory.read(node+0x50,riding))state.riding=riding!=0;
        if(state.ready)sampleTargets();
    }
    if(!state.ready){clearOverrides(recognized && stillCurrent());if(wasReady){++generation;state.generation=generation;}}
    {std::array<float,3> position{};uintptr_t fall{};state.flightAvailable=flightData(position,fall);}
    if(state.dataStatus!=lastDataStatus){log("Player reader: "+state.dataStatus);lastDataStatus=state.dataStatus;}
    // Mode is a user declaration, never a claim of automatic session detection.
    // Native calls below are experimental worker dispatch, not a verified game task hook.
    if(state.ready && state.offlineDeclared && entryPoints && selectedItem){
        auto now=GetTickCount64();if(!lastQuantityTick || now-lastQuantityTick>=250){cachedQuantity=quantity(selectedItem);lastQuantityTick=now;}
        state.ownedQuantity=cachedQuantity;
    }else{cachedQuantity=-1;lastQuantityTick=0;}
    while(!commands.empty()){auto command=commands.front();commands.pop_front();execute(command);}
    if(state.flying){
        std::array<float,3> position{},axes{};uintptr_t fall{},freshFall{};uint8_t gravity{};
        bool ok=state.offlineDeclared && state.session!=Session::Online && flightData(position,fall) &&
            memory.read(current.physics+0x1D6,gravity) && gravity==1 && noGravity.owned;
        auto now=GetTickCount64();double elapsed=lastFlightTick?double(now-lastFlightTick)/1000.:0.;lastFlightTick=now;
        if(!menuOpen.load() && input::gameplayFocused()){
            auto down=[](int key){return (GetAsyncKeyState(key)&0x8000)?1.f:0.f;};
            axes={down('L')-down('J'),down(VK_PRIOR)-down(VK_NEXT),down('I')-down('K')};
        }
        auto target=flightPosition(position,axes,state.flightSpeed,elapsed);
        // Timer suppression is transient, like clearing buildup; no stale timer is restored.
        ok=ok && target.has_value() && stillCurrent() && memory.read(current.modules+0x70,freshFall) && freshFall==fall && memory.write(fall+0x18,0.f);
        if(ok && (axes[0]!=0 || axes[1]!=0 || axes[2]!=0))ok=stillCurrent() && memory.write(current.physics+0x70,*target);
        if(!ok){bool restored=stopFlight();state.status=restored?"Flying stopped: character, mount or flight data changed.":"Flying stopped; original gravity could not be restored safely.";log(state.status);}
    }
    if(state.torrentJump){
        uintptr_t ride{},owner{},node{};int mounted{};
        bool validRide=state.ready && state.offlineDeclared && state.session!=Session::Online && state.riding && stillCurrent() &&
            memory.read(current.modules+0xE8,ride) && ride && memory.read(ride+8,owner) && owner==current.player &&
            memory.read(ride+0x10,node) && node && memory.read(node+0x50,mounted) && mounted!=0;
        if(validRide)horseJump::publish(ride,current.player,current.handle);
        else{horseJump::stop();state.torrentJump=false;state.status="Torrent jumps stopped: dismounted or character data changed.";}
    }else horseJump::stop();
    if(state.ready && state.offlineDeclared && state.statusMask){
        bool ok=state.statusValid && stillCurrent();
        if(ok)for(int i=0;i<7;++i)if(state.statusMask&(1u<<i))ok=stillCurrent() && memory.write(current.resist+0x10+4*i,0) && ok;
        if(!ok){clearOverrides(stillCurrent());state.status="Status suppression stopped: character data changed.";log(state.status);}
    }
}
}
