#include "runtime.hpp"
#include "catalog.hpp"
#include "storage.hpp"
#include "imgui.h"
#include <chrono>
#include <fstream>
#include <vector>
#include <cstdio>
#include <algorithm>
#include <cctype>

namespace tt {
namespace {
const ImVec4 gold{.78f,.66f,.40f,1}, muted{.60f,.62f,.61f,1}, ivory{.92f,.91f,.85f,1};
using Clock=std::chrono::steady_clock;
struct Attempt {float seconds; bool modified;};
std::vector<Attempt> history;
bool timing{}, modified{}; Clock::time_point started;
std::string localStatus;
const char* labels[]{"Home","Player","Combat","Items & Builds","Travel","Practice","Camera","Settings"};
const char* modifierNames[]{"Prevent ordinary death","Infinite FP","Infinite stamina","HP damage protection"};
const char* statusNames[]{"Poison","Scarlet rot","Hemorrhage","Death blight","Frostbite","Sleep","Madness"};
std::vector<std::string> pinned;
std::vector<Profile> savedProfiles;
int category{}; char search[96]{}; bool styled{};
float elapsed(){return std::chrono::duration<float>(Clock::now()-started).count();}
void heading(const char* label,const char* description){
    ImGui::TextColored(gold,"%s",label); ImGui::Spacing(); ImGui::TextWrapped("%s",description); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
}
void resource(const char* label,int current,int max,ImVec4 color,bool valid) {
    ImGui::TextColored(muted,"%s",label);
    char text[80]; if(valid)std::snprintf(text,sizeof(text),"%d / %d",current,max);else std::snprintf(text,sizeof(text),"Waiting for character");
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram,color);
    ImGui::ProgressBar(valid?float(current)/std::max(1,max):0,ImVec2(-1,24),text);ImGui::PopStyleColor();
}
void attributeEditor(const Snapshot& s){
    if(!ImGui::CollapsingHeader("Character attributes - edit",ImGuiTreeNodeFlags_DefaultOpen))return;
    static AttributeEdit draft;static uint64_t draftGeneration{};static bool captured{};
    static Command pending{Action::EditAttributes,0};
    if(captured && draftGeneration!=s.generation)captured=false;
    auto readCurrent=[&]{draft.before={s.attributes,s.level,s.runeMemory};draft.requested=s.attributes;draftGeneration=s.generation;captured=true;};
    if(!captured && s.attributesEditable)readCurrent();
    if(captured && s.attributesEditable && draft.requested==s.attributes && draft.before.values!=s.attributes)readCurrent();
    ImGui::TextWrapped("Experimental persistent edit. Each attribute accepts 1-99. Level follows the total attribute change; spendable runes stay unchanged. Starting-class minimums are not enforced.");
    ImGui::BeginDisabled(!s.attributesEditable);
    if(ImGui::Button("Read current attributes / reset draft"))readCurrent();
    ImGui::EndDisabled();
    if(!captured){ImGui::TextDisabled("Load a living character to read its attributes.");return;}
    if(ImGui::BeginTable("attribute-editor",3,ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_SizingStretchProp)){
        ImGui::TableSetupColumn("Attribute",0,1.1f);ImGui::TableSetupColumn("Current",0,.65f);ImGui::TableSetupColumn("New value",0,1.4f);ImGui::TableHeadersRow();
        for(int i=0;i<8;++i){ImGui::PushID(i);ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(attributeNames[i]);
            ImGui::TableNextColumn();ImGui::Text("%d",s.attributes[i]);ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);ImGui::InputInt("##attribute",&draft.requested[i],1,10);ImGui::PopID();}
        ImGui::EndTable();
    }
    AttributeState target;auto reason=planAttributes(draft,target);
    if(reason.empty())ImGui::Text("Level preview: %d -> %d",draft.before.level,target.level);
    Command c{Action::EditAttributes,draftGeneration};c.attributes=draft;c.confirmed=true;
    auto denied=rejection(s,c);if(!denied.empty())reason=denied;
    ImGui::BeginDisabled(!reason.empty());
    if(ImGui::Button("Preview attribute changes")){pending=c;ImGui::OpenPopup("Confirm character attributes");}
    ImGui::EndDisabled();
    if(!reason.empty())ImGui::TextWrapped("%s",reason.c_str());
    if(ImGui::BeginPopupModal("Confirm character attributes",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        AttributeState proposed;auto invalid=planAttributes(pending.attributes,proposed);
        ImGui::TextColored(gold,"PERSISTENT CHARACTER CHANGE");
        for(int i=0;i<8;++i)ImGui::Text("%-13s %2d -> %2d",attributeNames[i],pending.attributes.before.values[i],pending.attributes.requested[i]);
        ImGui::Text("Level: %d -> %d",pending.attributes.before.level,proposed.level);
        ImGui::Text("Rune history: %u -> %u",pending.attributes.before.runeMemory,proposed.runeMemory);
        ImGui::TextUnformatted("Spendable runes are unchanged. Disable All cannot undo this edit.");
        ImGui::TextUnformatted("Use a backed-up test character. Reload afterward to refresh derived stats.");
        ImGui::TextUnformatted("Applying also disables temporary modifiers. Offline confirmation is kept.");
        auto stale=rejection(s,pending);ImGui::BeginDisabled(!invalid.empty() || !stale.empty());
        if(ImGui::Button("Apply attributes once")){enqueue(pending);ImGui::CloseCurrentPopup();}
        ImGui::EndDisabled();ImGui::SameLine();
        if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight))ImGui::CloseCurrentPopup();
        if(!stale.empty())ImGui::TextWrapped("%s",stale.c_str());
        ImGui::EndPopup();
    }
}
void player(const Snapshot& s) {
    heading("PLAYER RESOURCES","Temporary controls. Maximum values are preserved. Protection from scripted deaths is not promised.");
    auto reason=rejection(s,{Action::Refill,s.generation});
    if(!reason.empty()){ImGui::TextColored(gold,"UNAVAILABLE");ImGui::TextWrapped("%s",reason.c_str());}
    ImGui::BeginDisabled(!reason.empty());
    if(ImGui::Button("Restore HP / FP / stamina",ImVec2(-1,40)))enqueue({Action::Refill,s.generation});
    for(int i=0;i<4;++i){bool on=s.active[i];if(ImGui::Checkbox(modifierNames[i],&on))enqueue({i==3?Action::NoDamage:static_cast<Action>(i+1),s.generation,on});}
    ImGui::EndDisabled();
    ImGui::TextWrapped("HP damage protection and ordinary death prevention are separate engine flags. Neither promises protection from scripted death or kill volumes.");
    if(ImGui::CollapsingHeader("Status buildup")){
        ImGui::TextWrapped("Clears or suppresses buildup only; an already active status effect is not removed.");
        ImGui::BeginDisabled(!reason.empty() || !s.statusValid);
        for(int i=0;i<7;++i){ImGui::PushID(i);bool on=(s.statusMask&(1u<<i))!=0;
            if(ImGui::Checkbox(statusNames[i],&on)){Command c{Action::SuppressStatus,s.generation,on};c.index=i;enqueue(c);}
            ImGui::SameLine();ImGui::Text("%d / %d",s.buildup[i],s.resistance[i]);ImGui::SameLine();
            if(ImGui::SmallButton("Clear")){Command c{Action::ClearStatus,s.generation};c.index=i;enqueue(c);}ImGui::PopID();}
        ImGui::EndDisabled();if(!s.statusValid)ImGui::TextDisabled("Buildup data unavailable.");
    }
    ImGui::SeparatorText("FLYING");
    bool flying=s.flying;Command fly{Action::Flight,s.generation,!flying};
    auto flyReason=rejection(s,fly);ImGui::BeginDisabled(!flyReason.empty());
    if(ImGui::Checkbox("Enable flying (on foot)",&flying))enqueue({Action::Flight,s.generation,flying});
    ImGui::EndDisabled();
    if(!flyReason.empty())ImGui::TextWrapped("%s",flyReason.c_str());
    float flightSpeed=s.flightSpeed;Command flightSpeedCommand{Action::FlightSpeed,s.generation};flightSpeedCommand.value=flightSpeed;
    ImGui::BeginDisabled(!rejection(s,flightSpeedCommand).empty());
    if(ImGui::SliderFloat("Flying speed",&flightSpeed,.5f,10.f,"%.1f m/s")){flightSpeedCommand.value=flightSpeed;enqueue(flightSpeedCommand);}
    ImGui::EndDisabled();
    ImGui::TextWrapped("Close the menu: I/K and J/L move horizontally in fixed world directions. Page Up rises; Page Down descends. Movement pauses with the menu open or when unfocused. Keyboard only.");
    ImGui::TextWrapped("Descend near the ground before stopping. Mounting, loading, character changes, profiles and Disable All stop flying. No terrain/cutscene or death-zone protection; this is experimental, not verified noclip.");
    ImGui::SeparatorText("TORRENT");
    bool jumping=s.torrentJump;Command jump{Action::TorrentJump,s.generation,!jumping};
    auto jumpReason=rejection(s,jump);ImGui::BeginDisabled(!jumpReason.empty());
    if(ImGui::Checkbox("Infinite Torrent double jumps",&jumping))enqueue({Action::TorrentJump,s.generation,jumping});
    ImGui::EndDisabled();
    if(!jumpReason.empty())ImGui::TextWrapped("%s",jumpReason.c_str());
    ImGui::TextWrapped("While mounted, press jump again for more air jumps. Turns off on dismount, loading or Disable All. Does not prevent falling damage or death zones.");
    attributeEditor(s);
    ImGui::Spacing(); ImGui::TextColored(muted,"Read from the current character");
    resource("HEALTH",s.current[0],s.maximum[0],{.59f,.24f,.22f,1},s.ready);
    resource("FOCUS",s.current[1],s.maximum[1],{.23f,.41f,.65f,1},s.ready);
    resource("STAMINA",s.current[2],s.maximum[2],{.36f,.54f,.33f,1},s.ready);
}
void practice(const Snapshot& s) {
    heading("BOSS PRACTICE","Manual attempt timing. Start and stop explicitly; no encounter events or boss resets are inferred.");
    if(timing)modified=modified || s.offlineDeclared || std::any_of(s.active.begin(),s.active.end(),[](bool v){return v;});
    ImGui::SetWindowFontScale(2.f);ImGui::Text("%07.2f s",timing?elapsed():history.empty()?0:history.back().seconds);ImGui::SetWindowFontScale(1.f);
    if(!timing){if(ImGui::Button("Start manual attempt",ImVec2(-1,40))){started=Clock::now();timing=true;modified=s.offlineDeclared || std::any_of(s.active.begin(),s.active.end(),[](bool v){return v;});}}
    else if(ImGui::Button("Stop and record",ImVec2(-1,40))){
        Attempt a{elapsed(),modified};history.push_back(a);if(history.size()>100)history.erase(history.begin());timing=false;
        auto path=dataDir()/(s.fingerprint?"attempts.csv":"renderer-test-attempts.csv");bool exists=std::filesystem::exists(path);
        std::ofstream out(path,std::ios::app);if(!exists)out<<"mode,seconds,toolkit_modifiers,outcome\n";out<<"manual,"<<a.seconds<<','<<(a.modified?"yes":"no")<<",manual_stop\n";
        if(!out)log("Attempt export failed.");
    }
    ImGui::BeginDisabled(!rejection(s,{Action::Refill,s.generation}).empty());
    if(ImGui::Button("Refill player resources"))enqueue({Action::Refill,s.generation});ImGui::EndDisabled();
    if(!history.empty()){auto best=std::min_element(history.begin(),history.end(),[](auto& a,auto& b){return a.seconds<b.seconds;});ImGui::Text("Shortest manual attempt: %.2f s (not a verified victory)",best->seconds);}
    ImGui::TextWrapped("Hits taken, damage dealt, automatic outcomes, and encounter restart: unavailable until verified event adapters exist. Attempts are marked modified whenever experimental testing is armed.");
    ImGui::TextColored(muted,"Session history (up to 100 entries)");
    for(int i=int(history.size())-1;i>=0;--i)ImGui::Text("Attempt %d    %.2f s    %s",i+1,history[i].seconds,history[i].modified?"Modified":"Toolkit modifiers off");
    ImGui::TextWrapped("Recorded attempts are appended to attempts.csv in the toolkit data folder.");
}
void unavailable(const char* title,const char* detail){heading(title,detail);ImGui::TextColored(gold,"UNAVAILABLE");ImGui::TextWrapped("This build does not include an implemented adapter for these controls.");}
void profilesPanel(const Snapshot& s){
    static Profile preview;static char name[49]="Custom";
    if(!ImGui::CollapsingHeader("Profiles",ImGuiTreeNodeFlags_DefaultOpen))return;
    ImGui::TextWrapped("Profiles contain temporary settings only. Saving or selecting one never changes the game.");
    if(ImGui::Button("Explorer")){preview={"Explorer",{false,false,true,false},0,1};}ImGui::SameLine();
    if(ImGui::Button("Build Lab")){preview={"Build Lab",{false,true,true,false},0,1};}ImGui::SameLine();
    if(ImGui::Button("Vanilla")){preview={"Vanilla",{},0,1};}
    for(auto& p:savedProfiles)if(ImGui::Selectable(p.name.c_str(),preview.name==p.name))preview=p;
    ImGui::TextColored(gold,"Preview: %s",preview.name.c_str());
    for(int i=0;i<4;++i)ImGui::Text("%s: %s",modifierNames[i],preview.modifiers[i]?"on":"off");
    ImGui::Text("Simulation %.2fx; suppression mask %u",preview.speed,unsigned(preview.statusMask));
    Command apply{Action::ApplyProfile,s.generation};apply.profile=preview;
    auto reason=rejection(s,apply);ImGui::BeginDisabled(!reason.empty());if(ImGui::Button("Apply previewed temporary settings"))enqueue(apply);ImGui::EndDisabled();
    ImGui::InputText("Profile name",name,sizeof(name));
    if(ImGui::Button("Save current settings as profile")){
        Profile p{name,s.active,s.statusMask,s.speedActive?s.speed:1.f};
        bool ok=saveProfile(p);localStatus=ok?"Profile saved.":"Could not save profile. Use 1-48 letters, digits, spaces, dash or underscore.";
        if(ok)savedProfiles=profiles();
    }
    ImGui::TextWrapped("Boss Learner, Glass Cannon and Photo presets await their damage/camera adapters. Vanilla disables temporary effects; it cannot undo persistent grants.");
}
void home(const Snapshot& s){
    heading("OFFLINE PRACTICE","Experimental build 0.2.6. Opening the menu does not pause gameplay.");
    ImGui::PushStyleColor(ImGuiCol_Text,s.ready?ImVec4(.5f,.75f,.45f,1):gold);ImGui::TextWrapped("%s",s.dataStatus.c_str());ImGui::PopStyleColor();
    ImGui::TextWrapped("Gameplay changes have not been tested in-game in this build. Session mode is not detected. Use offline single-player and a backed-up test character; inventory and rune grants can persist.");
    bool armed=s.offlineDeclared;
    if(ImGui::Checkbox("I am playing offline - enable experimental controls",&armed))enqueue({Action::ArmOffline,0,armed});
    if(armed){
        auto blocked=rejection(s,{Action::Refill,s.generation});
        if(!blocked.empty())ImGui::TextWrapped("Confirmation saved. Controls waiting: %s",blocked.c_str());
    }
    ImGui::TextWrapped("Confirmation lasts until you uncheck it, use Disable All, or restart the game. Loading, death and character/map changes stop temporary modifiers; enable them again when ready.");
    for(int i=0;i<4;++i)if(s.active[i]){ImGui::PushID(i);ImGui::TextColored(gold,"Active: %s",modifierNames[i]);ImGui::SameLine();if(ImGui::SmallButton("Disable"))enqueue({i==3?Action::NoDamage:static_cast<Action>(i+1),s.generation,false});ImGui::PopID();}
    if(s.statusMask)ImGui::Text("Status suppression active: mask %u",unsigned(s.statusMask));
    if(s.speedActive)ImGui::Text("Simulation speed active: %.2fx",s.speed);
    if(s.torrentJump){ImGui::TextColored(gold,"Active: Infinite Torrent double jumps");ImGui::SameLine();if(ImGui::SmallButton("Stop Torrent jumps"))enqueue({Action::TorrentJump,s.generation,false});}
    if(s.flying){ImGui::TextColored(gold,"Active: Flying (%.1f m/s)",s.flightSpeed);ImGui::SameLine();if(ImGui::SmallButton("Stop flying"))enqueue({Action::Flight,s.generation,false});}
    if(!pinned.empty()){ImGui::TextColored(gold,"Favorites");for(auto& id:pinned){int n=id[0]-'0';if(ImGui::Button(labels[n]))category=n;ImGui::SameLine();}ImGui::NewLine();}
    resource("HEALTH",s.current[0],s.maximum[0],{.59f,.24f,.22f,1},s.ready);
    resource("FOCUS",s.current[1],s.maximum[1],{.23f,.41f,.65f,1},s.ready);
    resource("STAMINA",s.current[2],s.maximum[2],{.36f,.54f,.33f,1},s.ready);
    profilesPanel(s);
}
void combat(const Snapshot& s){
    heading("TARGET INSPECTOR","Read-only nearby entity list. Selection here is independent of the game's lock-on target.");
    if(s.targets.empty())ImGui::TextDisabled("No valid nearby entity data.");
    ImGui::BeginChild("entities",{0,180},ImGuiChildFlags_Borders);
    for(auto& t:s.targets){char label[160];std::snprintf(label,sizeof(label),"Entity %d | HP %d / %d | %llX",t.id,t.hp,t.maxHp,static_cast<unsigned long long>(t.handle));
        if(ImGui::Selectable(label,t.handle==s.inspected)){Command c{Action::InspectTarget,s.generation};c.handle=t.handle;enqueue(c);}}
    ImGui::EndChild();
    auto at=std::find_if(s.targets.begin(),s.targets.end(),[&](auto& t){return t.handle==s.inspected;});
    if(at!=s.targets.end()){resource("SELECTED ENTITY HP",at->hp,at->maxHp,{.6f,.27f,.2f,1},true);
        if(at->poiseValid)ImGui::Text("Poise / stance sample: %.2f / %.2f",at->poise,at->maxPoise);else ImGui::TextDisabled("Poise data unavailable.");}
    else ImGui::TextDisabled("Select an entity. A lost or unloaded selection is never reused by index.");
    ImGui::Separator();ImGui::TextWrapped("Unavailable: player-only damage multipliers, eligible-consumable preservation, nonlethal sparring, one-hit mode and independent casting/attack speed. Their target and item eligibility hooks are not implemented.");
}
bool containsText(std::string_view text,std::string_view query){
    std::string a(text),b(query);auto lower=[](unsigned char c){return char(std::tolower(c));};std::transform(a.begin(),a.end(),a.begin(),lower);std::transform(b.begin(),b.end(),b.begin(),lower);return a.find(b)!=std::string::npos;
}
void items(const Snapshot& s){
    heading("ITEMS & BUILDS","Catalog entries come from the pinned reference source. Only eligible base-game items can be granted; quest/key and cut-content lists are excluded.");
    static char query[96]{};static int type{},content{},amount=1,planIndex{};static bool eligibleOnly{};
    static const CatalogItem* chosen{};static Command pending{Action::GrantItem,0};
    static const char* planNames[]{"Strength","Dexterity","Sorcery","Faith","Arcane","Custom"};
    static std::vector<PlanItem> plan=loadPlan("Strength");
    const char* types[]{"All types","Weapons","Arrows","Talismans","Armor","CraftingMaterials","UpgradeMaterials","Consumables","Sorceries","Incantations","SpiritAshes"};
    const char* contents[]{"All content","Base game","DLC (locked)","Unclassified (locked)"};
    ImGui::InputTextWithHint("##itemquery","Search item name...",query,sizeof(query));
    ImGui::Combo("Type",&type,types,11);ImGui::Combo("Content",&content,contents,4);ImGui::Checkbox("Only grant-eligible entries",&eligibleOnly);
    int count{};ImGui::BeginChild("itemlist",{0,210},ImGuiChildFlags_Borders);
    for(auto& item:catalog()){
        if((type && std::string_view(item.category)!=types[type]) || (content && item.content!=content-1) || (eligibleOnly && !item.grantable) || !containsText(item.name,query))continue;
        ++count;ImGui::PushID(static_cast<int>(item.id));
        if(ImGui::Selectable(item.name,chosen && chosen->id==item.id)){chosen=&item;amount=1;selectItem(item.id);}ImGui::PopID();
    }ImGui::EndChild();ImGui::Text("%d matches / %zu catalog entries",count,catalog().size());
    if(chosen){
        ImGui::TextColored(gold,"%s",chosen->name);ImGui::Text("ID %08X | %s | stack %d",chosen->id,chosen->category,chosen->stack);
        int owned=s.selectedItem==chosen->id?s.ownedQuantity:-1;
        if(owned>=0)ImGui::Text("Owned quantity: %d",owned);else ImGui::TextDisabled("Ownership unavailable; arm offline testing to query the selected item.");
        if(std::string_view(chosen->category)=="Weapons")ImGui::TextWrapped("Adds one +0 weapon with its default affinity/skill. Existing copies are allowed. Character stats only affect using it.");
        ImGui::InputInt("Quantity",&amount);amount=std::clamp(amount,1,std::max(1,chosen->stack));
        if(!chosen->grantable)ImGui::TextWrapped("Grant unavailable: this entry is DLC, unclassified, or has unsupported item behavior. Stats do not unlock grants.");
        Command c{Action::GrantItem,s.generation};c.item=chosen->id;c.value=amount;c.expectedBefore=owned;c.confirmed=true;
        auto reason=rejection(s,c);bool allowed=canGrant(chosen,amount,owned) && reason.empty();
        ImGui::BeginDisabled(!allowed);
        if(ImGui::Button("Preview persistent item grant")){pending=c;ImGui::OpenPopup("Confirm item grant");}ImGui::EndDisabled();
        if(!reason.empty())ImGui::TextWrapped("%s",reason.c_str());
        else if(chosen->grantable && !allowed)ImGui::TextWrapped("Grant unavailable: quantity exceeds the supported limit for this entry.");
        if(ImGui::Button("Add selected item to local plan")){
            auto at=std::find_if(plan.begin(),plan.end(),[&](auto& x){return x.id==chosen->id;});
            if(at!=plan.end())at->quantity=amount;else if(plan.size()<64)plan.push_back({chosen->id,amount});
        }
    }
    if(ImGui::BeginPopupModal("Confirm item grant",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
        auto item=findItem(pending.item);ImGui::Text("%s",item?item->name:"Invalid item");
        ImGui::Text("Add %d | before %d | requested after %d",int(pending.value),pending.expectedBefore,pending.expectedBefore+int(pending.value));
        ImGui::TextUnformatted("Persistent change. Test with a backed-up disposable character.");
        if(ImGui::Button("Apply once")){enqueue(pending);ImGui::CloseCurrentPopup();}ImGui::SameLine();
        if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight))ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    if(ImGui::CollapsingHeader("Build plans and material packs")){
        ImGui::TextWrapped("These are editable planning slots. No equipment is changed and no items are granted by selecting or saving a plan. Select each row to inspect ownership and grant missing eligible items individually with a preview.");
        if(ImGui::Combo("Plan",&planIndex,planNames,6))plan=loadPlan(planNames[planIndex]);
        if(ImGui::Button("Save plan"))localStatus=savePlan(planNames[planIndex],plan)?"Plan saved.":"Could not save plan.";
        ImGui::SameLine();if(ImGui::Button("Clear plan"))plan.clear();
        if(ImGui::Button("Preview smithing pack (12 each, tiers 1-8)")){
            plan.clear();for(int n=1;n<=8;++n){auto name="Smithing Stone ["+std::to_string(n)+"]";for(auto& item:catalog())if(name==item.name && item.grantable)plan.push_back({item.id,12});}
            localStatus="Pack loaded into the local plan only. Review and grant entries individually.";
        }
        if(ImGui::Button("Preview somber pack (1 each, tiers 1-9)")){
            plan.clear();for(int n=1;n<=9;++n){auto name="Somber Smithing Stone ["+std::to_string(n)+"]";for(auto& item:catalog())if(name==item.name && item.grantable)plan.push_back({item.id,1});}
            localStatus="Pack loaded into the local plan only. Review and grant entries individually.";
        }
        int remove=-1;for(size_t i=0;i<plan.size();++i){auto item=findItem(plan[i].id);if(!item)continue;ImGui::PushID(int(i)+3000);
            std::string label=std::to_string(plan[i].quantity)+" x "+item->name;
            if(ImGui::Selectable(label.c_str(),chosen && chosen->id==item->id)){chosen=item;selectItem(item->id);amount=plan[i].quantity;}
            if(s.selectedItem==item->id && s.ownedQuantity>=0)ImGui::Text("Owned %d | missing %d",s.ownedQuantity,std::max(0,plan[i].quantity-s.ownedQuantity));
            if(ImGui::SmallButton("Remove from plan"))remove=int(i);ImGui::PopID();}
        if(remove>=0)plan.erase(plan.begin()+remove);
    }
    if(ImGui::CollapsingHeader("Runes - persistent addition")){
        static int add=1000;static Command rune{Action::AddRunes,0};
        if(s.statsValid)ImGui::Text("Current runes: %d",s.runes);else ImGui::TextDisabled("Rune data unavailable.");
        ImGui::InputInt("Runes to add",&add);add=std::clamp(add,1,1000000);
        Command c{Action::AddRunes,s.generation};c.value=add;c.expectedBefore=s.runes;c.confirmed=true;
        ImGui::BeginDisabled(!s.itemApi || !rejection(s,c).empty());
        if(ImGui::Button("Preview rune addition")){rune=c;ImGui::OpenPopup("Confirm rune addition");}ImGui::EndDisabled();
        if(ImGui::BeginPopupModal("Confirm rune addition",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            ImGui::Text("Before %d | add %d | requested after %d",rune.expectedBefore,int(rune.value),rune.expectedBefore+int(rune.value));
            ImGui::TextUnformatted("Uses the engine's rune-gain workflow. The observed result is logged.");
            ImGui::TextUnformatted("Persistent. Disable All cannot undo this action.");
            if(ImGui::Button("Apply once")){enqueue(rune);ImGui::CloseCurrentPopup();}ImGui::SameLine();
            if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight))ImGui::CloseCurrentPopup();ImGui::EndPopup();
        }
    }
}
void travel(const Snapshot& s){
    heading("TRAVEL & BOOKMARKS","Session-only bookmarks. These are positions, not full game-state snapshots.");
    if(s.positionValid){ImGui::Text("Map %08X | orientation %.3f",s.map,s.angle);ImGui::Text("X %.3f   Y %.3f   Z %.3f",s.position[0],s.position[1],s.position[2]);}
    else ImGui::TextDisabled("Position unavailable.");
    static char name[49]="Practice spot";static auto list=bookmarks();
    ImGui::InputText("Bookmark name",name,sizeof(name));ImGui::BeginDisabled(!s.positionValid || s.riding);
    if(ImGui::Button("Save current position")){
        bool ok=saveBookmark({name,s.generation,s.handle,s.map,s.position,s.angle});localStatus=ok?"Bookmark saved for this loaded session.":"Bookmark failed validation.";if(ok)list=bookmarks();
    }ImGui::EndDisabled();
    ImGui::TextWrapped("Experimental return moves at most 100 metres within the same loaded map. Save only on stable ground. Ground, combat and cutscene detection are not implemented; do not use return during those transitions.");
    for(size_t i=0;i<list.size();++i){auto& b=list[i];ImGui::PushID(int(i));ImGui::Text("%s | map %08X",b.name.c_str(),b.map);
        Command c{Action::ReturnBookmark,s.generation};c.bookmark=b;auto reason=rejection(s,c);
        ImGui::BeginDisabled(!reason.empty());if(ImGui::Button("Return once (experimental)"))enqueue(c);ImGui::EndDisabled();
        if(!reason.empty())ImGui::TextWrapped("%s",reason.c_str());ImGui::PopID();}
    ImGui::TextWrapped("Bookmarks expire on character/map change, death/loading and game restart. Grace travel, world time and free flight remain unavailable.");
}
void camera(const Snapshot& s){
    heading("CAMERA & SIMULATION","Global simulation speed is experimental and affects the whole simulation. Menu interaction and the manual timer use real time.");
    static float speed=1.f;ImGui::SliderFloat("Requested speed",&speed,.25f,1.5f,"%.2fx");
    if(s.speedValid)ImGui::Text("Observed simulation speed: %.3fx",s.speed);else ImGui::TextDisabled("Simulation speed pointer unavailable.");
    Command c{Action::SimulationSpeed,s.generation};c.value=speed;
    ImGui::BeginDisabled(!s.speedValid || !rejection(s,c).empty());if(ImGui::Button("Apply speed"))enqueue(c);ImGui::SameLine();
    if(ImGui::Button("Restore owned speed")){c.value=1;enqueue(c);speed=1;}ImGui::EndDisabled();
    ImGui::TextWrapped("Hide game HUD, FOV, free camera, pause, frame step and photo presets remain unavailable: camera/cutscene restoration adapters are not implemented.");
}

}
void drawMenu() {
    if(!menuOpen)return;
    bool hadPopup=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId);
    auto s=snapshot();static auto prefs=settings();
    if(!styled){auto& st=ImGui::GetStyle();ImGui::StyleColorsDark();st.WindowRounding=10;st.ChildRounding=7;st.FrameRounding=5;st.WindowPadding={20,18};st.FramePadding={10,8};st.ItemSpacing={10,12};
        st.Colors[ImGuiCol_WindowBg]={.075f,.082f,.084f,.98f};st.Colors[ImGuiCol_ChildBg]={.10f,.11f,.115f,1};st.Colors[ImGuiCol_Text]=ivory;st.Colors[ImGuiCol_TextDisabled]=muted;
        st.Colors[ImGuiCol_Button]={.24f,.22f,.16f,1};st.Colors[ImGuiCol_ButtonHovered]={.39f,.33f,.20f,1};st.Colors[ImGuiCol_ButtonActive]={.47f,.39f,.22f,1};st.Colors[ImGuiCol_CheckMark]=gold;st.Colors[ImGuiCol_SliderGrab]=gold;st.Colors[ImGuiCol_Header]={.32f,.28f,.18f,1};
        st.Colors[ImGuiCol_TitleBg]=st.Colors[ImGuiCol_TitleBgActive]={.14f,.145f,.14f,1};
        st.Colors[ImGuiCol_FrameBg]={.15f,.16f,.16f,1};st.Colors[ImGuiCol_FrameBgHovered]={.25f,.24f,.20f,1};st.Colors[ImGuiCol_FrameBgActive]={.30f,.28f,.20f,1};
        st.Colors[ImGuiCol_HeaderHovered]={.39f,.33f,.20f,1};st.Colors[ImGuiCol_HeaderActive]={.47f,.39f,.22f,1};pinned=favorites();savedProfiles=profiles();styled=true;
    }
    auto& io=ImGui::GetIO();io.FontGlobalScale=prefs.scale;
    ImVec2 display=io.DisplaySize;
    ImGui::SetNextWindowSize({std::min(900.f*prefs.scale,display.x-24),std::min(620.f*prefs.scale,display.y-24)},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},ImGuiCond_FirstUseEver,{.5f,.5f});
    ImGui::SetNextWindowSizeConstraints({std::min(550.f,display.x-24),std::min(360.f,display.y-24)},{display.x-12,display.y-12});
    bool open=true;
    if(ImGui::Begin("Tarnished Toolkit",&open,ImGuiWindowFlags_NoCollapse)){
        // A resolution change can make the previous placement exceed the viewport.
        auto size=ImGui::GetWindowSize(),pos=ImGui::GetWindowPos();
        size.x=std::min(size.x,std::max(100.f,display.x-24));size.y=std::min(size.y,std::max(100.f,display.y-24));
        pos.x=std::clamp(pos.x,0.f,std::max(0.f,display.x-size.x));pos.y=std::clamp(pos.y,0.f,std::max(0.f,display.y-size.y));
        ImGui::SetWindowSize(size);ImGui::SetWindowPos(pos);
        ImGui::TextColored(gold,"T A R N I S H E D   T O O L K I T");ImGui::SameLine();ImGui::TextDisabled("  EXPERIMENTAL  0.2");
        ImGui::Text("%s | %s",s.fingerprint?("Game "+s.gameVersion+" fingerprint matched").c_str():"Unsupported game executable",s.offlineDeclared?"Offline testing armed (user declared)":"Gameplay controls disarmed");
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##search","Find a feature: health, stamina, practice, camera...",search,sizeof(search));
        float footer=78*prefs.scale;
        ImGui::BeginChild("sidebar",{165*prefs.scale,-footer},ImGuiChildFlags_Borders);

        for(int i=0;i<8;++i){if(ImGui::Selectable(labels[i],category==i,0,{0,29*prefs.scale})){category=i;search[0]=0;}}
        ImGui::EndChild();ImGui::SameLine();ImGui::BeginChild("content",{0,-footer},ImGuiChildFlags_Borders);
        if(category>0 && !search[0]){auto id=std::to_string(category);bool isPinned=std::find(pinned.begin(),pinned.end(),id)!=pinned.end();
            if(ImGui::SmallButton(isPinned?"Unpin from Home":"Pin to Home")){if(toggleFavorite(id))pinned=favorites();}}
        if(search[0]){
            heading("FEATURE SEARCH","Select a result to open its category.");
            struct Entry{const char* name;int cat;};
            Entry entries[]{{"health HP focus FP stamina restore",1},{"damage combat target inspector",2},{"items inventory builds runes",3},{"travel position bookmarks",4},{"practice timer attempts history",5},{"camera photo HUD field of view",6},{"settings scale hotkey diagnostics",7}};
            std::string query=search;std::transform(query.begin(),query.end(),query.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            for(auto e:entries){std::string name=e.name;std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});if(name.find(query)!=std::string::npos && ImGui::Selectable(e.name)){category=e.cat;search[0]=0;}}
        } else switch(category){
        case 0:home(s);break;
        case 1:player(s);break;
        case 2:combat(s);break;
        case 3:items(s);break;
        case 4:travel(s);break;
        case 5:practice(s);break;
        case 6:camera(s);break;
        case 7:heading("SETTINGS & DIAGNOSTICS","Preferences are saved locally. Gameplay modifiers never start automatically.");
            ImGui::SliderFloat("UI scale",&prefs.scale,.8f,1.8f,"%.2fx");ImGui::Checkbox("Show practice HUD",&prefs.hud);
            const char* keys[]{"Insert","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12"};int key=prefs.menuKey==0x2d?0:prefs.menuKey-0x70+1;
            if(ImGui::Combo("Menu hotkey",&key,keys,13))prefs.menuKey=key?0x70+key-1:0x2d;
            const char* chords[]{"Unbound","Hold Start + Back","Hold LB + RB + Start"};ImGui::Combo("Controller menu shortcut",&prefs.controllerChord,chords,3);
            ImGui::TextWrapped("Hold the exact combination for 0.65 seconds, then release before using it again. Extra buttons cancel activation. Choose a chord compatible with your bindings; its initial presses may still reach the game.");
            if(ImGui::Button("Save preferences")){if(saveSettings(prefs))log("Preferences saved.");else log("Preferences could not be saved.");}
            // Edits are held locally below until Save is pressed.
            ImGui::TextWrapped("Insert (default): menu. Escape: cancel dialog or close. Ctrl+Shift+Backspace: disable all. Tab/arrows/Enter: navigation. Controller shortcut is unbound by default.");
            ImGui::TextWrapped("This beta uses an explicit offline declaration and a worker command queue. Automatic session detection and game-thread dispatch are not implemented. New gameplay features have not been live tested.");
            ImGui::TextWrapped("Data: %s",dataDir().string().c_str());
            ImGui::TextWrapped("To remove the toolkit, close the game and run Uninstall.ps1. Runtime unloading is not supported.");break;
        }
        ImGui::EndChild();
        ImGui::Separator();if(ImGui::Button("DISABLE ALL",{160*prefs.scale,34*prefs.scale}))disableAll();ImGui::SameLine();ImGui::TextDisabled("%s",s.offlineDeclared?"Offline testing armed":"Testing disarmed");
        ImGui::TextWrapped("%s",s.status.c_str());
        if(!localStatus.empty())ImGui::TextWrapped("%s",localStatus.c_str());
    }ImGui::End();
    if(!hadPopup && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)){
        if(ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight))open=false;
        if(ImGui::IsKeyPressed(ImGuiKey_GamepadL1))category=(category+7)%8;
        if(ImGui::IsKeyPressed(ImGuiKey_GamepadR1))category=(category+1)%8;
    }
    if(!open)menuOpen=false;
}
void drawHud(){if(timing){auto s=snapshot();modified=modified || s.offlineDeclared || s.statusMask || s.speedActive || std::any_of(s.active.begin(),s.active.end(),[](bool x){return x;});}if(!timing || !settings().hud)return;ImGui::SetNextWindowPos({18,18},ImGuiCond_Always);ImGui::SetNextWindowBgAlpha(.8f);ImGui::Begin("##practice-hud",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);ImGui::TextColored(gold,"MANUAL PRACTICE");ImGui::Text("%.2f s  |  %s",elapsed(),modified?"Modified":"Toolkit modifiers off");ImGui::End();}
}
