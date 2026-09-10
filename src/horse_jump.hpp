#pragma once
#include <cstdint>
#include <string_view>
namespace tt::horseJump {
bool initialize(uintptr_t base,std::string_view executableHash);
void publish(uintptr_t ride,uintptr_t player,uint64_t handle);
void stop();
#ifdef TT_HORSE_TESTS
bool installFixture(void* first,void* second,uintptr_t* worldSlot,uintptr_t* menuSlot);
#endif
}
