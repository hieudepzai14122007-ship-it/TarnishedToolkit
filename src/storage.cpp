#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "runtime.hpp"
#include "storage.hpp"
#include "catalog.hpp"
#include <fstream>
#include <mutex>
namespace tt {
namespace {
std::mutex storageMutex;
std::vector<Bookmark> savedBookmarks;
std::string readFile(const char* name){
    auto path=dataDir()/name;std::error_code ec;auto size=std::filesystem::file_size(path,ec);
    if(ec || size>65536)return {};
    std::ifstream in(path);return std::string(std::istreambuf_iterator<char>(in),{});
}
bool writeFile(const std::string& name,const std::string& text){
    auto path=dataDir()/name,tmp=dataDir()/(name+".tmp");
    {std::ofstream out(tmp,std::ios::trunc);out<<text;out.flush();if(!out)return false;}
    return MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}
std::vector<Profile> readProfiles(){
    auto text=readFile("profiles.txt");if(text.empty())return {};
    auto parsed=parseProfiles(text);if(!parsed){log("Profiles rejected: invalid schema or content.");return {};}
    return *parsed;
}
std::vector<std::string> readFavorites(){
    auto text=readFile("favorites.txt");std::istringstream in(text);std::string tag;int schema{},count{};
    if(!(in>>tag>>schema>>count) || tag!="TTFavorites" || schema!=1 || count<0 || count>8)return {};
    std::vector<std::string> list;std::string id;
    for(int i=0;i<count;++i){if(!(in>>std::quoted(id)) || id.size()!=1 || id[0]<'1' || id[0]>'7' || std::find(list.begin(),list.end(),id)!=list.end())return {};list.push_back(id);}
    if(in>>id)return {};return list;
}
}
std::vector<Profile> profiles(){std::lock_guard lock(storageMutex);return readProfiles();}
bool saveProfile(const Profile& p){
    if(!validProfile(p) || !validName(p.name))return false;std::lock_guard lock(storageMutex);
    auto list=readProfiles();auto at=std::find_if(list.begin(),list.end(),[&](auto& x){return x.name==p.name;});
    if(at!=list.end())*at=p;else{if(list.size()>=32)return false;list.push_back(p);}
    return writeFile("profiles.txt",encodeProfiles(list));
}
bool saveBookmark(const Bookmark& b){
    if(!validName(b.name) || !b.handle || !std::isfinite(b.angle))return false;
    for(float x:b.position)if(!std::isfinite(x))return false;
    std::lock_guard lock(storageMutex);
    auto at=std::find_if(savedBookmarks.begin(),savedBookmarks.end(),[&](auto& x){return x.name==b.name;});
    if(at!=savedBookmarks.end())*at=b;else{if(savedBookmarks.size()>=32)return false;savedBookmarks.push_back(b);}return true;
}
std::vector<Bookmark> bookmarks(){std::lock_guard lock(storageMutex);return savedBookmarks;}
std::vector<std::string> favorites(){std::lock_guard lock(storageMutex);return readFavorites();}
bool toggleFavorite(std::string id){
    if(id.size()!=1 || id[0]<'1' || id[0]>'7')return false;
    std::lock_guard lock(storageMutex);auto list=readFavorites();auto at=std::find(list.begin(),list.end(),id);
    if(at==list.end())list.push_back(id);else list.erase(at);
    std::ostringstream out;out<<"TTFavorites 1 "<<list.size()<<'\n';for(auto& x:list)out<<std::quoted(x)<<'\n';return writeFile("favorites.txt",out.str());
}
bool savePlan(std::string name,const std::vector<PlanItem>& items){
    if(!validName(name) || items.size()>64)return false;std::set<uint32_t> seen;
    for(auto& x:items){auto item=findItem(x.id);if(!item || x.quantity<1 || x.quantity>item->stack || !seen.insert(x.id).second)return false;}
    std::ostringstream out;out<<"TTPlan 1 "<<items.size()<<'\n';for(auto& x:items)out<<x.id<<' '<<x.quantity<<'\n';
    std::lock_guard lock(storageMutex);return writeFile("plan-"+name+".txt",out.str());
}
std::vector<PlanItem> loadPlan(std::string name){
    if(!validName(name))return {};std::lock_guard lock(storageMutex);auto file="plan-"+name+".txt";
    std::istringstream in(readFile(file.c_str()));std::string tag;int schema{},count{};
    if(!(in>>tag>>schema>>count) || tag!="TTPlan" || schema!=1 || count<0 || count>64)return {};
    std::vector<PlanItem> result;std::set<uint32_t> seen;
    for(int i=0;i<count;++i){PlanItem x{};if(!(in>>x.id>>x.quantity))return {};auto item=findItem(x.id);
        if(!item || x.quantity<1 || x.quantity>item->stack || !seen.insert(x.id).second)return {};result.push_back(x);}
    if(in>>tag)return {};return result;
}
}
