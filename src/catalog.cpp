#include "catalog.hpp"
namespace tt {
static constexpr CatalogItem entries[]{
#include "catalog.inc"
};
std::span<const CatalogItem> catalog(){return entries;}
const CatalogItem* findItem(uint32_t id){for(auto& item:entries)if(item.id==id)return &item;return nullptr;}
}
