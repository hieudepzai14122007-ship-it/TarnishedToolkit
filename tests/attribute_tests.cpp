#include "core.hpp"
#include <iostream>
#include <cstdlib>
#include <limits>
using namespace tt;
int checks{};
void check(bool ok,const char* label){++checks;if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}}
struct Store {
    std::array<uint32_t,10> fields{};
    uint32_t spendableRunes=777;int puts{},failAt{},externalAt{},unmodifiedAt{},invalidateAt{};bool live=true,partial{};
    bool valid(){return live;}
    bool get(int i,uint32_t& out){out=fields[i];return true;}
    bool put(int i,uint32_t value){
        ++puts;
        if(puts==failAt){if(partial)fields[i]=0x12345678;return false;}
        fields[i]=value;
        if(puts==externalAt)fields[i]=77;
        if(puts==unmodifiedAt)fields[7]=20;
        if(puts==invalidateAt)live=false;
        return true;
    }
};
int main(){
    AttributeState before{{10,10,10,10,10,10,10,10},1,100};AttributeEdit edit{before,before.values};AttributeState planned;
    check(!planAttributes(edit,planned).empty(),"unchanged draft rejected");
    edit.requested[0]=20;check(planAttributes(edit,planned).empty() && planned.level==11 && planned.values[0]==20,"level follows aggregate attribute delta");
    uint64_t expectedHistory=100;for(int n=2;n<=11;++n)expectedHistory+=attributeLevelCost(n);
    check(planned.runeMemory==expectedHistory,"history sums new levels only");
    check(attributeLevelCost(2)==673 && attributeLevelCost(12)==847 && attributeLevelCost(13)==1038,"pinned source formula boundary examples");
    for(int invalid:{0,-1,100,std::numeric_limits<int>::max()}){edit.requested[0]=invalid;check(!planAttributes(edit,planned).empty(),"invalid attribute bounds rejected before arithmetic");}
    edit={before,before.values};edit.requested[0]=9;check(!planAttributes(edit,planned).empty(),"level zero rejected");
    edit.requested[1]=11;check(planAttributes(edit,planned).empty() && planned.level==1 && planned.runeMemory==100,"same-total reallocation preserves level and history");
    edit={before,before.values};edit.requested.fill(99);check(planAttributes(edit,planned).empty() && planned.level==713,"all-99 valid total caps at level 713");
    edit.before.level=2;check(!planAttributes(edit,planned).empty(),"result above 713 rejected");
    edit.before=before;edit.before.runeMemory=UINT32_MAX-5;check(planAttributes(edit,planned).empty() && planned.runeMemory==UINT32_MAX,"rune history saturates without wrapping");
    edit.before={{20,10,10,10,10,10,10,10},11,50000};edit.requested=before.values;
    check(planAttributes(edit,planned).empty() && planned.level==1 && planned.runeMemory==50000,"level reduction does not erase history");
    edit={before,before.values};edit.requested[0]=11;edit.requested[1]=12;
    planAttributes(edit,planned);auto original=attributeFields(before),target=attributeFields(planned);
    Store store{original};check(commitAttributes(store,edit)==AttributeCommit::Applied && store.fields==target && store.puts==4,"only changed attributes level and history are written");
    check(store.spendableRunes==777,"spendable rune balance untouched");
    for(int failure=1;failure<=4;++failure){Store fault{original};fault.failAt=failure;
        check(commitAttributes(fault,edit)==AttributeCommit::RolledBack && fault.fields==original,"write failure restores previously owned values");}
    Store stale{original};stale.fields[0]=11;check(commitAttributes(stale,edit)==AttributeCommit::Rejected && stale.puts==0,"stale source values cause no writes");
    Store lost{original};lost.live=false;check(commitAttributes(lost,edit)==AttributeCommit::Rejected && lost.puts==0,"invalid character causes no writes");
    Store conflict{original};conflict.externalAt=1;
    check(commitAttributes(conflict,edit)==AttributeCommit::Incomplete && conflict.fields[0]==77,"rollback does not overwrite an external modification");
    Store independent{original};independent.unmodifiedAt=1;
    check(commitAttributes(independent,edit)==AttributeCommit::RolledBack && independent.fields[0]==10 && independent.fields[7]==20,"unmodified external field retained while owned change rolls back");
    Store reload{original};reload.invalidateAt=1;
    check(commitAttributes(reload,edit)==AttributeCommit::Incomplete && reload.puts==1,"no rollback writes to stale character");
    Store partial{original};partial.failAt=1;partial.partial=true;
    check(commitAttributes(partial,edit)==AttributeCommit::Incomplete && partial.fields[0]==0x12345678,"partial unknown write is reported rather than force overwritten");
    AttributeEdit bad=edit;bad.requested[2]=100;Store invalid{original};
    check(commitAttributes(invalid,bad)==AttributeCommit::Rejected && invalid.puts==0,"bad plan never reaches memory writes");
    Snapshot s;s.fingerprint=true;s.ready=true;s.adapterReady=true;s.offlineDeclared=true;s.generation=7;s.statsValid=true;s.attributesEditable=true;s.attributes=before.values;s.level=before.level;s.runeMemory=before.runeMemory;s.current={10,10,10};s.maximum={10,10,10};
    Command command{Action::EditAttributes,7};command.attributes=edit;
    check(!rejection(s,command).empty(),"unconfirmed attribute command blocked");command.confirmed=true;
    check(rejection(s,command).empty(),"confirmed supported attribute command allowed");
    s.attributes[0]=11;check(!rejection(s,command).empty(),"changed attribute invalidates preview");s.attributes=before.values;
    s.level=2;check(!rejection(s,command).empty(),"changed level invalidates preview");s.level=before.level;
    ++s.runeMemory;check(!rejection(s,command).empty(),"changed rune history invalidates preview");s.runeMemory=before.runeMemory;
    s.generation=8;check(!rejection(s,command).empty(),"character generation invalidates preview");s.generation=7;
    s.attributesEditable=false;check(!rejection(s,command).empty(),"missing attribute capability blocks apply");s.attributesEditable=true;
    s.session=Session::Online;check(!rejection(s,command).empty(),"online state blocks attribute edit");s.session=Session::Unknown;
    s.offlineDeclared=false;check(!rejection(s,command).empty(),"disarmed offline testing blocks attribute edit");
    std::cout<<"All "<<checks<<" attribute planning, transaction and command checks passed.\n";
}
