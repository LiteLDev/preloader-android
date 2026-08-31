#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace pl::runtime {

struct GameHookSignatures {
  std::string pauseMenuDtor;
  std::string pauseMenuOpen;
  std::string hudScreenDtor;
  std::string hudScreenOpen;
  std::string isShowingMenu;
  std::optional<std::size_t> isShowingMenuVtableIndex;
};

void ConfigureGameHookRules(std::string rulesPath, std::string minecraftVersion);
std::optional<GameHookSignatures> LoadConfiguredGameHookSignatures();

} // namespace pl::runtime
