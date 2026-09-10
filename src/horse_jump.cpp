#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <MinHook.h>
#include <array>
#include <atomic>
#include "horse_jump.hpp"

// Original implementation using factual instruction sites identified from the
// Hexinton reference and independently checked against the exact 2.7.1 PE.
// No trainer implementation or player-on-foot patch is included.
extern "C" {
std::atomic<uintptr_t> tt_horse_ride{},tt_horse_player{},tt_horse_handle{};
uintptr_t tt_horse_worldSlot{},tt_horse_menuSlot{};
void *tt_horse_original1{},*tt_horse_original2{},*tt_horse_resume1{},*tt_horse_resume2{};
}
static_assert(std::atomic<uintptr_t>::is_always_lock_free);
static_assert(sizeof(std::atomic<uintptr_t>)==8);

#if defined(__GNUC__) && defined(__x86_64__)
// These detours replace one six-byte MOV, so preserve all incoming registers
// and flags. No C++ call or guessed game-function signature is involved.
#define HORSE_GUARD \
"pushfq\n push %rax\n" \
"mov tt_horse_ride(%rip),%rax\n test %rax,%rax\n jz 1f\n cmp %rax,%rdi\n jne 1f\n" \
"mov 8(%rdi),%rax\n test %rax,%rax\n jz 1f\n cmp tt_horse_player(%rip),%rax\n jne 1f\n" \
"mov 8(%rax),%rax\n cmp tt_horse_handle(%rip),%rax\n jne 1f\n" \
"mov tt_horse_worldSlot(%rip),%rax\n mov (%rax),%rax\n test %rax,%rax\n jz 1f\n" \
"mov 0x1e508(%rax),%rax\n cmp tt_horse_player(%rip),%rax\n jne 1f\n" \
"mov 0x10(%rdi),%rax\n test %rax,%rax\n jz 1f\n cmpl $0,0x50(%rax)\n je 1f\n" \
"mov tt_horse_menuSlot(%rip),%rax\n mov (%rax),%rax\n test %rax,%rax\n jz 1f\n" \
"cmpb $1,0x94(%rax)\n jne 1f\n cmpb $0,0x96(%rax)\n jne 1f\n" \
"cmp tt_horse_ride(%rip),%rdi\n jne 1f\n pop %rax\n popfq\n"
extern "C" void tt_horse_first();
extern "C" void tt_horse_second();
asm(".text\n .p2align 4\n .globl tt_horse_first\n tt_horse_first:\n"
    HORSE_GUARD
    "movb $1,0x134(%rdi)\n jmp *tt_horse_resume1(%rip)\n"
    "1: pop %rax\n popfq\n jmp *tt_horse_original1(%rip)\n"
    ".p2align 4\n .globl tt_horse_second\n tt_horse_second:\n"
    HORSE_GUARD
    "movb $1,0x135(%rdi)\n jmp *tt_horse_resume2(%rip)\n"
    "1: pop %rax\n popfq\n jmp *tt_horse_original2(%rip)\n");
#undef HORSE_GUARD
#endif

namespace tt::horseJump {
namespace {
bool installed{};
bool install(void* first,void* second,uintptr_t worldSlot,uintptr_t menuSlot){
#if defined(__GNUC__) && defined(__x86_64__)
    if(installed)return true;
    auto status=MH_Initialize();if(status!=MH_OK && status!=MH_ERROR_ALREADY_INITIALIZED)return false;
    tt_horse_worldSlot=worldSlot;tt_horse_menuSlot=menuSlot;
    tt_horse_resume1=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(first)+6);
    tt_horse_resume2=reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(second)+6);
    if(MH_CreateHook(first,reinterpret_cast<void*>(&tt_horse_first),&tt_horse_original1)!=MH_OK)return false;
    if(MH_CreateHook(second,reinterpret_cast<void*>(&tt_horse_second),&tt_horse_original2)!=MH_OK){MH_RemoveHook(first);return false;}
    // Targets stay zero throughout installation. Partial installation remains
    // pass-through even if another tool prevents complete hook cleanup.
    if(MH_EnableHook(first)!=MH_OK || MH_EnableHook(second)!=MH_OK){
        MH_DisableHook(first);MH_DisableHook(second);MH_RemoveHook(first);MH_RemoveHook(second);return false;
    }
    installed=true;return true;
#else
    return false;
#endif
}
template<size_t N> bool matches(uintptr_t address,const std::array<unsigned char,N>& expected){
    std::array<unsigned char,N> actual{};SIZE_T read{};
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),actual.data(),N,&read) && read==N && actual==expected;
}
}
bool initialize(uintptr_t base,std::string_view hash){
    stop();
    if(hash!="1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891")return false;
    constexpr std::array<unsigned char,19> first{0x88,0x87,0x34,0x01,0,0,0x48,0x8b,0x83,0x90,0x01,0,0,0x48,0x8b,0x48,0x68,0x80,0xb9};
    constexpr std::array<unsigned char,24> second{0x88,0x87,0x35,0x01,0,0,0x48,0x8b,0x83,0x90,0x01,0,0,0x48,0x8b,0x48,0x70,0xe8,0xdb,0x84,0xfd,0xff,0x88,0x87};
    if(!matches(base+0x475e1c,first) || !matches(base+0x475e4f,second))return false;
    return install(reinterpret_cast<void*>(base+0x475e1c),reinterpret_cast<void*>(base+0x475e4f),base+0x3D69FF8,base+0x3D6F820);
}
void stop(){tt_horse_ride.store(0,std::memory_order_release);}
void publish(uintptr_t ride,uintptr_t player,uint64_t handle){
    stop();if(!installed || !ride || !player || !handle)return;
    tt_horse_player.store(player,std::memory_order_relaxed);tt_horse_handle.store(handle,std::memory_order_relaxed);
    tt_horse_ride.store(ride,std::memory_order_release);
}
#ifdef TT_HORSE_TESTS
bool installFixture(void* first,void* second,uintptr_t* worldSlot,uintptr_t* menuSlot){
    return install(first,second,reinterpret_cast<uintptr_t>(worldSlot),reinterpret_cast<uintptr_t>(menuSlot));
}
#endif
}
