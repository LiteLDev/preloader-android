#pragma once

#include <string>

namespace pl::runtime {

void ConfigureGameHooks(std::string rulesPath, std::string minecraftVersion);
void InitGameHooks();

bool IsPauseMenuOpen();
bool IsHudScreenOpen();
bool IsShowingMenu();
bool ShouldForceGlobalModMenu();

} // namespace pl::runtime
