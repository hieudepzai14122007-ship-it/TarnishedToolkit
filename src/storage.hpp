#pragma once
#include "core.hpp"
#include <iomanip>
#include <set>
namespace tt {
inline bool validName(std::string_view name){
    if(name.empty() || name.size()>48 || name.front()==' ' || name.back()==' ')return false;
    for(unsigned char c:name)if(!((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c==' ' || c=='_' || c=='-'))return false;
    return true;
}
inline std::optional<std::vector<Profile>> parseProfiles(std::string_view text){
    std::istringstream in{std::string(text)};std::string tag;int schema{},count{};
    if(!(in>>tag>>schema>>count) || tag!="TTProfiles" || schema!=1 || count<0 || count>32)return {};
    std::vector<Profile> result;std::set<std::string> names;
    for(int i=0;i<count;++i){Profile p;int flags{},mask{};
        if(!(in>>std::quoted(p.name)>>flags>>mask>>p.speed) || flags<0 || flags>15 || mask<0 || mask>127 || !validName(p.name) || !names.insert(p.name).second)return {};
        p.statusMask=static_cast<uint8_t>(mask);for(int j=0;j<4;++j)p.modifiers[j]=(flags&(1<<j))!=0;
        if(!validProfile(p))return {};result.push_back(p);
    }
    std::string extra;if(in>>extra)return {};return result;
}
inline std::string encodeProfiles(const std::vector<Profile>& list){
    std::ostringstream out;out<<"TTProfiles 1 "<<list.size()<<'\n';
    for(auto& p:list){int flags{};for(int i=0;i<4;++i)if(p.modifiers[i])flags|=1<<i;
        out<<std::quoted(p.name)<<' '<<flags<<' '<<int(p.statusMask)<<' '<<p.speed<<'\n';}
    return out.str();
}
}
