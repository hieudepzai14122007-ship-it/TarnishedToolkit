#pragma once
#include <cstdint>
#include <span>
#include <cmath>
#include <string_view>
namespace tt {
struct CatalogItem {
    uint32_t id;
    const char* name;
    const char* category;
    int stack;
    int content; // 0 base, 1 DLC, 2 not classified by upstream
    bool grantable;
};
std::span<const CatalogItem> catalog();
const CatalogItem* findItem(uint32_t id);
// Weapon stack=1 is a per-instance count, not a limit of one owned copy.
// Default base ID and ash=-1 follow ItemViewModel.SpawnSingleItem.
inline bool canGrant(const CatalogItem* item, double amount, int owned) {
    if(!item || !item->grantable || item->content!=0 || owned<0 || !std::isfinite(amount) || amount<1 || std::floor(amount)!=amount)return false;
    if(std::string_view(item->category)=="Weapons")return amount==1 && owned<9999;
    return amount<=item->stack && owned<=item->stack-amount;
}
}
