#include "core.hpp"
#include "input_policy.hpp"
#include "catalog.hpp"
#include "storage.hpp"
#include <limits>
#include <set>
#include <iostream>
#include <map>
#include <cstdlib>
int checks{};
void check(bool value,const char* name) {++checks;if(!value){std::cerr<<"FAIL "<<name<<'\n';std::exit(1);}}
struct Memory { std::map<uintptr_t,uint8_t> bytes; int writes{}; bool read(uintptr_t p,uint8_t& v){if(!bytes.contains(p))return false;v=bytes[p];return true;} bool write(uintptr_t p,uint8_t v){bytes[p]=v;++writes;return true;} };
int main(){
 using namespace tt;
 check(supportedVersion("d1a84083c6c7c7902162ff098f7d86812839aa6b3575959398857e539c488134")=="2.7.0.0","original executable profile retained");
 check(supportedVersion("1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891")=="2.7.1.0","September executable profile recognized");
 check(supportedVersion("2.7.1.0").empty() && supportedVersion("").empty(),"version label alone cannot enable memory access");
 check(supportedVersion("1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597890").empty(),"one altered hash digit rejected");
 check(supportedVersion("1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891extra").empty(),"partial fingerprint matches rejected");
 Snapshot s; Command c{Action::Refill,7};
 check(!rejection(s,c).empty(),"unknown build blocked");
 s.fingerprint=true; s.ready=true; s.adapterReady=true; s.generation=7; s.current={200,30,40};s.maximum={400,60,80};
 check(!rejection(s,c).empty(),"unknown session blocked");
 s.session=Session::Online; check(!rejection(s,c).empty(),"online blocked");
 s.session=Session::Offline;s.adapterReady=false;check(!rejection(s,c).empty(),"unavailable adapter blocked");
 s.adapterReady=true;check(rejection(s,c).empty(),"valid command accepted");
 c.generation=6;check(!rejection(s,c).empty(),"stale character blocked");
 c.generation=7;s.current[0]=-1;check(!rejection(s,c).empty(),"invalid resource blocked");
 check(rejection(Snapshot{},{Action::DisableAll,0}).empty(),"emergency always permitted");
 check(!rejection(s,{static_cast<Action>(999),7}).empty(),"unknown command blocked");
 check(parseSettings("TarnishedToolkit 1 1.25 45 1").has_value(),"settings roundtrip");
 for(auto text:{"", "TarnishedToolkit 2 1 45 1", "TarnishedToolkit 1 nan 45 1", "TarnishedToolkit 1 3 45 1", "TarnishedToolkit 1 1 8 1", "TarnishedToolkit 1 1 45 2", "TarnishedToolkit 1 1 45 1 junk"})check(!parseSettings(text),"corrupt settings rejected");
 Memory m; m.bytes[123]=0;OwnedByte o;
 check(o.set(m,123,1,7),"take ownership");check(o.restore(m,7)&&m.bytes[123]==0,"restore original");
 check(o.set(m,123,1,7),"reacquire");m.bytes[123]=2;int writes=m.writes;
 check(!o.restore(m,7)&&m.writes==writes,"external change preserved");
 m.bytes[123]=0;check(o.set(m,123,1,7),"reacquire generation");writes=m.writes;
 check(!o.restore(m,8)&&m.writes==writes,"stale entity not written");

 s.current={200,30,40};s.session=Session::Unknown;s.offlineDeclared=true;
 check(rejection(s,c).empty(),"explicit offline test declaration permits beta controls");
 s.session=Session::Online;check(!rejection(s,c).empty(),"known online overrides user declaration");s.session=Session::Unknown;
 check(rejection(s,{Action::ArmOffline,6,true}).empty(),"offline declaration is independent of character generation");
 s.statusValid=true;Command status{Action::ClearStatus,7};status.index=7;check(!rejection(s,status).empty(),"status index upper bound");
 status.index=-1;check(!rejection(s,status).empty(),"status negative index");status.index=6;check(rejection(s,status).empty(),"valid status request");
 Command speed{Action::SimulationSpeed,7};speed.value=std::numeric_limits<double>::quiet_NaN();check(!rejection(s,speed).empty(),"NaN speed blocked");
 speed.value=0;check(!rejection(s,speed).empty(),"freeze not accepted");speed.value=.25;check(rejection(s,speed).empty(),"lower speed bound");speed.value=1.51;check(!rejection(s,speed).empty(),"upper speed bound");
 Command rune{Action::AddRunes,7};s.statsValid=true;s.runes=500;rune.expectedBefore=500;rune.value=1000;
 check(!rejection(s,rune).empty(),"unconfirmed persistent action blocked");rune.confirmed=true;check(rejection(s,rune).empty(),"valid rune preview");
 s.runes=501;check(!rejection(s,rune).empty(),"changed runes invalidate preview");s.runes=500;rune.value=1.5;check(!rejection(s,rune).empty(),"fractional runes rejected");
 rune.value=1000001;check(!rejection(s,rune).empty(),"rune per-action limit");s.runes=999999999;rune.expectedBefore=s.runes;rune.value=1;check(!rejection(s,rune).empty(),"rune cap cannot overflow");
 Command grant{Action::GrantItem,7};grant.confirmed=true;grant.item=10;grant.expectedBefore=1;s.itemApi=true;s.selectedItem=10;s.ownedQuantity=1;
 check(rejection(s,grant).empty(),"matching ownership preview accepted");s.selectedItem=11;check(!rejection(s,grant).empty(),"changed item invalidates grant");s.selectedItem=10;s.ownedQuantity=2;check(!rejection(s,grant).empty(),"changed inventory invalidates grant");s.ownedQuantity=1;s.itemApi=false;check(!rejection(s,grant).empty(),"unavailable engine blocks grant");
 Command travel{Action::ReturnBookmark,7};s.positionValid=true;s.position={0,0,0};s.handle=123;s.map=456;s.riding=false;
 travel.bookmark={"Test",7,123,456,{5,0,0},0};check(rejection(s,travel).empty(),"same-session nearby bookmark");
 travel.bookmark.map=99;check(!rejection(s,travel).empty(),"cross-map travel rejected");travel.bookmark.map=456;travel.bookmark.handle=99;check(!rejection(s,travel).empty(),"other character bookmark rejected");travel.bookmark.handle=123;
 travel.bookmark.position[0]=101;check(!rejection(s,travel).empty(),"distant bookmark rejected");travel.bookmark.position[0]=5;s.riding=true;check(!rejection(s,travel).empty(),"mounted travel rejected");s.riding=false;
 travel.bookmark.position[1]=std::numeric_limits<float>::infinity();check(!rejection(s,travel).empty(),"nonfinite bookmark rejected");
 Profile p{"Practice",{false,true,true,false},3,.5f};auto encoded=encodeProfiles({p});auto parsed=parseProfiles(encoded);
 check(parsed && parsed->size()==1 && parsed->at(0).speed==.5f && parsed->at(0).modifiers[1],"profile serialization preserves settings");
 for(auto text:{"TTProfiles 2 0", "TTProfiles 1 1 \"Bad\" 16 0 1", "TTProfiles 1 1 \"Bad\" 1 128 1", "TTProfiles 1 1 \"Bad\" 1 0 10", "TTProfiles 1 0 trailing", "TTProfiles 1 99", "TTProfiles 1 1 \"../escape\" 0 0 1"})check(!parseProfiles(text),"corrupt or unsafe profile rejected");
 check(!validName("../file") && !validName("a/b") && !validName("a\\b") && !validName(" name"),"plan names cannot escape storage");
 check(!parseProfiles("TTProfiles 1 2 \"Same\" 0 0 1 \"Same\" 0 0 1"),"duplicate profile names rejected");
 OwnedBit bit;m.bytes[321]=0x40;check(bit.set(m,321,2,7),"take bit ownership");m.bytes[321]|=0x80;
 check(bit.restore(m,7) && m.bytes[321]==0xC0,"bit restore preserves unrelated concurrent changes");
 m.bytes[321]=2;check(bit.set(m,321,2,7) && bit.restore(m,7) && m.bytes[321]==2,"preexisting flag retained");
 m.bytes[321]=0;check(bit.set(m,321,2,7),"bit reacquire");writes=m.writes;check(!bit.restore(m,8) && m.writes==writes,"bit restoration rejects stale entity");
 struct Floats{float value=1.f;int writes{};bool read(uintptr_t,float& out){out=value;return true;}bool write(uintptr_t,float v){value=v;++writes;return true;}} floats;
 OwnedValue<float> ownedSpeed;check(ownedSpeed.set(floats,77,.5f,0) && ownedSpeed.set(floats,77,.75f,0) && ownedSpeed.restore(floats,0) && floats.value==1.f,"speed changes preserve original before first override");
 check(ownedSpeed.set(floats,77,.5f,0),"speed reacquire");floats.value=.8f;writes=floats.writes;check(!ownedSpeed.restore(floats,0) && floats.writes==writes,"speed external modification preserved");
 InputPolicy input;using Transition=InputPolicy::Transition;
 check(input.update(false,true)==Transition::None && !input.blockWarp(true),"closed menu does not block cursor");
 check(input.update(true,true)==Transition::Acquire && input.blockWarp(true),"opening foreground menu captures cursor policy");
 check(input.update(true,true)==Transition::None,"no repeated cursor acquisition");
 check(input.update(true,false)==Transition::Release && !input.blockWarp(false),"AltTab releases cursor");
 check(input.update(true,false)==Transition::None,"background does not repeatedly restore confinement");
 check(input.update(true,true)==Transition::Acquire,"foreground reentry can reacquire");
 check(input.update(false,true)==Transition::Release && !input.blockWarp(true),"closing releases recenter block");
 for(int i=0;i<100;++i){input.update(true,true);input.update(false,true);}check(!input.captured,"repeated toggles end released");
 std::set<uint32_t> ids;int eligible{};bool catalogValid=true;
 for(auto& item:catalog()){
     catalogValid=catalogValid && ids.insert(item.id).second && item.stack>=1 && item.name[0] && item.content>=0 && item.content<=2;
     if(item.grantable){++eligible;catalogValid=catalogValid && item.content==0 && std::string_view(item.category)!="Consumables" && std::string_view(item.category)!="SpiritAshes";}
 }
 check(catalog().size()==2034 && eligible==1599 && catalogValid,"entire catalog uniqueness bounds and grant exclusions");
 check(findItem(0xFFFFFFFF)==nullptr,"unknown catalog ID rejected");
 auto dagger=findItem(1000000);auto dlc=findItem(4500000);auto unknown=findItem(13510000);
 check(dagger && std::string_view(dagger->name)=="Dagger" && dagger->content==0 && canGrant(dagger,1,0),"base weapon grant enabled from pinned classification");
 check(canGrant(dagger,1,1) && canGrant(dagger,1,2),"weapon instance stack does not forbid duplicate copies");
 check(!canGrant(dagger,2,0) && !canGrant(dagger,1,9999),"weapon action and sanity bounds enforced");
 check(dlc && dlc->content==1 && !canGrant(dlc,1,0),"DLC weapon remains explicitly blocked");
 check(unknown && unknown->content==2 && !canGrant(unknown,1,0),"unclassified weapon remains blocked");
 check(!canGrant(nullptr,1,0) && !canGrant(dagger,1,-1),"unknown item and unreadable ownership rejected");
 check(!canGrant(dagger,1.5,0) && !canGrant(dagger,0,0) && !canGrant(dagger,std::numeric_limits<double>::quiet_NaN(),0),"noninteger and nonfinite grant amounts rejected");
 int weapons{};bool grantsValid=true;
 for(auto& item:catalog()){
     if(std::string_view(item.category)=="Weapons" && item.grantable)++weapons;
     if(item.grantable && std::string_view(item.category)!="Weapons")grantsValid=grantsValid && canGrant(&item,1,0) && canGrant(&item,item.stack,0) && !canGrant(&item,1,item.stack);
 }
 check(weapons==377 && grantsValid,"all 377 eligible weapons and stackable inventory bounds validated");
 Snapshot transition;transition.offlineDeclared=true;transition.active.fill(true);transition.statusMask=127;transition.speedActive=true;transition.torrentJump=true;
 resetTemporaryState(transition);
 check(transition.offlineDeclared && transition.statusMask==0 && !transition.speedActive && !transition.torrentJump && std::none_of(transition.active.begin(),transition.active.end(),[](bool x){return x;}),"loading reset clears modifiers but retains offline declaration");
 for(int i=0;i<100;++i)resetTemporaryState(transition);
 check(transition.offlineDeclared,"repeated unready polls do not erase offline confirmation");
 transition.ready=false;transition.fingerprint=true;transition.adapterReady=true;
 check(!rejection(transition,{Action::Refill,0}).empty(),"retained declaration never permits gameplay while loading");
 check(rejection(transition,{Action::ArmOffline,999,false}).empty(),"offline confirmation can be unchecked during loading despite stale generation");
 check(rejection(transition,{Action::ArmOffline,0,true}).empty(),"offline declaration can be made while loading");
 transition.offlineDeclared=false;resetTemporaryState(transition);
 check(!transition.offlineDeclared && !Snapshot{}.offlineDeclared,"reset does not arm an unchecked or newly started process");
 Snapshot intent;std::deque<Command> queue;
 queueCommand(intent,queue,{Action::ArmOffline,99,true});
 check(intent.offlineDeclared && queue.empty(),"checkbox acknowledged immediately before worker poll");
 check(!rejection(intent,{Action::Refill,0}).empty(),"confirmation does not bypass unsupported executable");
 intent.fingerprint=true;intent.adapterReady=true;
 check(!rejection(intent,{Action::Refill,0}).empty(),"confirmation does not bypass unavailable character");
 intent.ready=true;intent.generation=7;intent.current={10,10,10};intent.maximum={10,10,10};intent.session=Session::Online;
 check(!rejection(intent,{Action::Refill,7}).empty(),"known online gameplay blocked despite accepted declaration");
 intent.session=Session::Unknown;
 check(rejection(intent,{Action::Refill,7}).empty() && !rejection(intent,{Action::Refill,6}).empty(),"ready gameplay still requires current generation");
 for(int i=0;i<40;++i)queueCommand(intent,queue,{Action::Refill,7});
 check(queue.size()==32,"ordinary command queue remains bounded");
 queueCommand(intent,queue,{Action::ArmOffline,0,false});
 check(!intent.offlineDeclared && queue.size()==1 && queue.front().action==Action::ArmOffline && !queue.front().enabled,"uncheck immediately revokes declaration and cancels queued gameplay even when full");
 queueCommand(intent,queue,{Action::ArmOffline,100,true});
 check(intent.offlineDeclared && queue.size()==1 && !queue.front().enabled,"rapid recheck retains required restoration without delayed arming command");
 resetTemporaryState(intent);queue.pop_front();
 check(intent.offlineDeclared,"delayed restoration cannot overwrite newer confirmation");
 queueCommand(intent,queue,{Action::Refill,7});queueCommand(intent,queue,{Action::DisableAll,0});
 check(!intent.offlineDeclared && queue.size()==1 && queue.front().action==Action::DisableAll,"Disable All immediately clears confirmation and pending gameplay");
 queueCommand(intent,queue,{Action::ArmOffline,0,true});resetTemporaryState(intent);queue.pop_front();
 check(intent.offlineDeclared,"new confirmation after Disable All survives queued restoration");
 for(int i=0;i<100;++i){queueCommand(intent,queue,{Action::ArmOffline,0,false});queueCommand(intent,queue,{Action::ArmOffline,0,true});}
 check(intent.offlineDeclared && queue.size()==1,"rapid clicks retain latest intent and do not fill queue");
 Snapshot mount;mount.fingerprint=true;mount.ready=true;mount.adapterReady=true;mount.offlineDeclared=true;mount.generation=7;mount.current={10,10,10};mount.maximum={10,10,10};
 Command jump{Action::TorrentJump,7,true};
 check(!rejection(mount,jump).empty(),"unavailable horse adapter blocked");mount.torrentJumpAvailable=true;
 check(!rejection(mount,jump).empty(),"horse jump enable requires mounted state");mount.riding=true;
 check(rejection(mount,jump).empty(),"mounted supported horse toggle accepted");
 mount.offlineDeclared=false;check(!rejection(mount,jump).empty(),"horse toggle requires offline confirmation");
 check(rejection(Snapshot{},{Action::TorrentJump,999,false}).empty(),"horse toggle can always be disabled");
 int smithing{},somber{};for(auto& item:catalog()){for(int i=1;i<=8;++i)if(std::string("Smithing Stone [")+std::to_string(i)+"]"==item.name && item.grantable && item.stack>=12)++smithing;for(int i=1;i<=9;++i)if(std::string("Somber Smithing Stone [")+std::to_string(i)+"]"==item.name && item.grantable)++somber;}
 check(smithing==8 && somber==9,"material packs have exact eligible catalog entries");
 auto oldSettings=parseSettings("TarnishedToolkit 1 1 45 1");check(oldSettings && oldSettings->controllerChord==0,"schema 1 migrates with opening chord unbound");
 auto newSettings=parseSettings("TarnishedToolkit 2 1.25 45 1 2");check(newSettings && newSettings->controllerChord==2,"schema 2 retains selected chord");
 check(!parseSettings("TarnishedToolkit 2 1 45 1 3") && !parseSettings("TarnishedToolkit 2 1 45 1 -1"),"unknown chord bindings rejected");
 HoldActivation hold;
 check(!hold.update(true,true,100),"hold starts without immediate toggle");
 check(!hold.update(true,true,749),"hold requires full duration");
 check(hold.update(true,true,750),"hold activates at threshold");
 check(!hold.update(true,true,2000),"held chord never repeats");
 check(!hold.update(false,true,2100) && !hold.update(true,true,2200) && hold.update(true,true,2850),"release permits next activation");
 hold.update(false,true,3000);hold.update(true,true,3100);hold.update(true,false,3750);
 check(!hold.update(true,true,4000) && !hold.update(true,true,4500),"focus loss cancels accumulated hold time");
 check(hold.update(true,true,4650),"fresh foreground hold works");
 std::cout<<"All "<<checks<<" validation and ownership checks passed.\n";
}
