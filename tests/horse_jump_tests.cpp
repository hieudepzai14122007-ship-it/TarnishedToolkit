#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <MinHook.h>
#include "horse_jump.hpp"
#include <array>
#include <vector>
#include <cstring>
#include <iostream>
#include <cstdlib>
int checks{};
void check(bool ok,const char* name){++checks;if(!ok){std::cerr<<"FAIL "<<name<<'\n';std::exit(1);}}
template<class T> void put(void* p,size_t offset,T value){std::memcpy(static_cast<unsigned char*>(p)+offset,&value,sizeof(value));}
int main(){
    using namespace tt::horseJump;
    check(!initialize(0,"unknown"),"unknown executable cannot install horse hooks");
    check(!initialize(0,"1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891"),"missing instruction guards reject installation");
    // A tiny native fixture executes the exact MOVs that are intercepted in the
    // game. MinHook and our real assembly detours run only in this test process.
    const std::array<unsigned char,24> code{0x57,0x48,0x89,0xcf,0x48,0x89,0xd0,0x48,0x39,0xc9,
        0x88,0x87,0x34,0x01,0,0,0x9c,0x41,0x59,0x4d,0x89,0x08,0x5f,0xc3};
    auto memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    check(memory!=nullptr,"allocate isolated native fixture");
    std::memcpy(memory,code.data(),code.size());std::memcpy(memory+64,code.data(),code.size());memory[64+12]=0x35;
    DWORD prior{};check(VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&prior) && FlushInstructionCache(GetCurrentProcess(),memory,4096),"make fixture executable");
    std::vector<unsigned char> world(0x1e510);std::array<unsigned char,512> player{},ride{},other{};
    std::array<unsigned char,256> node{},menu{};
    auto worldPtr=reinterpret_cast<uintptr_t>(world.data()),menuPtr=reinterpret_cast<uintptr_t>(menu.data());
    auto playerPtr=reinterpret_cast<uintptr_t>(player.data()),ridePtr=reinterpret_cast<uintptr_t>(ride.data());
    put(world.data(),0x1e508,playerPtr);put(player.data(),8,uint64_t{77});put(ride.data(),8,playerPtr);
    put(ride.data(),0x10,reinterpret_cast<uintptr_t>(node.data()));put(node.data(),0x50,int{1});menu[0x94]=1;
    check(installFixture(memory+10,memory+74,&worldPtr,&menuPtr),"install real hook pair into native fixture");
    using Fn=uint64_t(*)(void*,uint64_t,uint64_t*);
    auto first=reinterpret_cast<Fn>(memory),second=reinterpret_cast<Fn>(memory+64);
    auto run=[&](void* target,bool force,const char* name){
        uint64_t flags1{},flags2{};constexpr uint64_t input=0xfedcba9876543280ull;
        auto a=first(target,input,&flags1),b=second(target,input,&flags2);auto bytes=static_cast<unsigned char*>(target);
        check(a==input && b==input && (flags1&0x8d5)==0x44 && (flags2&0x8d5)==0x44 && bytes[0x134]==(force?1:0x80) && bytes[0x135]==(force?1:0x80),name);
    };
    run(ride.data(),false,"disabled hooks preserve original bytes, RAX and arithmetic flags");
    publish(ridePtr,playerPtr,77);run(ride.data(),true,"mounted local ride gets both jump flags without register corruption");
    run(other.data(),false,"another ride object is untouched");
    put(node.data(),0x50,int{0});run(ride.data(),false,"live dismount immediately bypasses override");put(node.data(),0x50,int{1});
    menu[0x94]=0;run(ride.data(),false,"live loading flag blocks override");menu[0x94]=1;
    menu[0x96]=2;run(ride.data(),false,"live fade blocks override");menu[0x96]=0;
    put(player.data(),8,uint64_t{78});run(ride.data(),false,"changed character handle blocks stale target");put(player.data(),8,uint64_t{77});
    put(world.data(),0x1e508,uintptr_t{0});run(ride.data(),false,"world player change blocks stale target");put(world.data(),0x1e508,playerPtr);
    put(ride.data(),8,uintptr_t{0});run(ride.data(),false,"missing ride owner bypasses without dereference");put(ride.data(),8,playerPtr);
    put(ride.data(),0x10,uintptr_t{0});run(ride.data(),false,"missing ride state bypasses without dereference");put(ride.data(),0x10,reinterpret_cast<uintptr_t>(node.data()));
    auto savedWorld=worldPtr;worldPtr=0;run(ride.data(),false,"missing world manager bypasses");worldPtr=savedWorld;
    auto savedMenu=menuPtr;menuPtr=0;run(ride.data(),false,"missing menu manager bypasses");menuPtr=savedMenu;
    stop();run(ride.data(),false,"stop returns both sites to original behavior");
    publish(ridePtr,playerPtr,0);run(ride.data(),false,"zero character handle cannot arm");
    for(int i=0;i<100;++i){publish(ridePtr,playerPtr,77);stop();}run(ride.data(),false,"repeated toggles leave both sites disabled");
    check(MH_RemoveHook(memory+10)==MH_OK && MH_RemoveHook(memory+74)==MH_OK,"remove fixture hook pair");
    check(VirtualFree(memory,0,MEM_RELEASE)!=FALSE,"release isolated fixture");
    std::cout<<"All "<<checks<<" native horse-hook checks passed. No game or graphics host used.\n";
}
