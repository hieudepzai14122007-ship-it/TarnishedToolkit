#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "runtime.hpp"
#include "catalog.hpp"
#include <fstream>
#include <iostream>
#include <cstdlib>
namespace {std::filesystem::path root;int checks{};}
namespace tt {std::filesystem::path dataDir(){return root;}void log(std::string_view){} }
void check(bool ok,const char* name){++checks;if(!ok){std::cerr<<"FAIL "<<name<<'\n';std::exit(1);}}
int main(){
    using namespace tt;
    wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,32768);
    auto parent=std::filesystem::canonical(std::filesystem::path(module).parent_path());
    root=parent/("storage-tests-"+std::to_string(GetCurrentProcessId()));
    check(root.parent_path()==parent && std::filesystem::create_directory(root),"isolated test directory next to test executable");
    Profile p{"Practice",{false,true,true,false},3,.5f};
    check(saveProfile(p),"save profile");auto list=profiles();check(list.size()==1 && list[0].statusMask==3,"load profile from disk");
    p.speed=.75f;check(saveProfile(p) && profiles().size()==1 && profiles()[0].speed==.75f,"replace named profile without duplication");
    check(!saveProfile({"../escape",{},0,1}),"path-like profile rejected");
    {std::ofstream out(root/"profiles.txt");out<<"TTProfiles 999 1 broken";}
    check(profiles().empty(),"corrupt on-disk profiles fall back empty");
    check(saveProfile(p) && profiles().size()==1,"explicit save recovers corrupt profile store");
    check(toggleFavorite("1") && toggleFavorite("3") && favorites().size()==2,"favorites saved and reloaded");
    check(toggleFavorite("1") && favorites().size()==1 && favorites()[0]=="3","favorite toggles off");
    check(!toggleFavorite("../bad") && !toggleFavorite("8"),"invalid favorite rejected");
    auto item=catalog()[0];std::vector<PlanItem> plan{{item.id,1}};
    check(savePlan("Strength",plan),"save plan");auto loaded=loadPlan("Strength");check(loaded.size()==1 && loaded[0].id==item.id,"load catalog-backed plan");
    check(!savePlan("../escape",plan),"plan traversal rejected");
    check(!savePlan("Custom",{{0xFFFFFFFF,1}}),"unknown plan item rejected");
    check(!savePlan("Custom",{{item.id,item.stack+1}}),"plan stack overflow rejected");
    check(!savePlan("Custom",{{item.id,1},{item.id,1}}),"duplicate plan entries rejected");
    {std::ofstream out(root/"plan-Strength.txt");out<<"TTPlan 1 1 4294967295 1";}
    check(loadPlan("Strength").empty(),"corrupted plan contents rejected on read");
    check(savePlan("Strength",{}) && loadPlan("Strength").empty(),"empty plan persists");
    Bookmark b{"Spot",7,123,456,{1,2,3},.5f};check(saveBookmark(b) && bookmarks().size()==1,"session bookmark stored");
    b.position[0]=5;check(saveBookmark(b) && bookmarks().size()==1 && bookmarks()[0].position[0]==5,"named bookmark replaced");
    check(!std::filesystem::exists(root/"bookmarks.txt"),"bookmarks are not reused across process sessions");
    check(!std::filesystem::exists(root/"profiles.txt.tmp") && !std::filesystem::exists(root/"plan-Strength.txt.tmp"),"successful atomic writes leave no temp files");
    // Remove only files created by this test; never recurse through a computed path.
    for(auto name:{"profiles.txt","favorites.txt","plan-Strength.txt"})std::filesystem::remove(root/name);
    check(std::filesystem::remove(root),"isolated test directory cleaned");
    std::cout<<"All "<<checks<<" storage checks passed. No game or user configuration was accessed.\n";
}
