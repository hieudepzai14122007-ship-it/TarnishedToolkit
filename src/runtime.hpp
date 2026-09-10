#pragma once
#include "core.hpp"
#include <filesystem>
#include <atomic>
namespace tt {
void initialize();
void poll();
Snapshot snapshot();
void enqueue(Command command);
void disableAll();
void log(std::string_view message);
std::filesystem::path dataDir();
Settings settings();
bool saveSettings(Settings settings);
void selectItem(uint32_t id);
bool saveProfile(const Profile& profile);
std::vector<Profile> profiles();
bool saveBookmark(const Bookmark& bookmark);
std::vector<Bookmark> bookmarks();
bool toggleFavorite(std::string id);
std::vector<std::string> favorites();
struct PlanItem {uint32_t id;int quantity;};
bool savePlan(std::string name,const std::vector<PlanItem>& items);
std::vector<PlanItem> loadPlan(std::string name);
void drawMenu();
void drawHud();
extern std::atomic_bool menuOpen;
extern std::atomic_int menuKey;
bool installOverlay();
}
