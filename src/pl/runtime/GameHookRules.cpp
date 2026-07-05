#include "pl/runtime/GameHookRules.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "pl/Logger.hpp"

namespace pl::runtime {
namespace {

std::mutex g_rulesMutex;
std::string g_rulesPath;
std::string g_minecraftVersion;

std::string Trim(std::string value) {
  auto isNotSpace = [](unsigned char ch) { return !std::isspace(ch); };
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(), isNotSpace));
  value.erase(std::find_if(value.rbegin(), value.rend(), isNotSpace).base(),
              value.end());
  return value;
}

std::optional<std::string> ReadTextFile(const std::string &path) {
  if (path.empty()) {
    return std::nullopt;
  }

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }

  return std::string(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
}

std::optional<std::string> ReadStringField(const nlohmann::json &object,
                                           const char *key) {
  if (!object.is_object()) {
    return std::nullopt;
  }

  auto it = object.find(key);
  if (it == object.end() || !it->is_string()) {
    return std::nullopt;
  }

  std::string value = Trim(it->get<std::string>());
  if (value.empty()) {
    return std::nullopt;
  }
  return value;
}

std::vector<int> ParseVersionParts(std::string_view value) {
  std::vector<int> parts;
  long current = 0;
  bool inNumber = false;

  auto pushPart = [&] {
    if (!inNumber) {
      return;
    }
    parts.push_back(static_cast<int>(
        std::min<long>(current, std::numeric_limits<int>::max())));
    current = 0;
    inNumber = false;
  };

  for (unsigned char ch : value) {
    if (std::isdigit(ch)) {
      current = std::min<long>(
          current * 10 + static_cast<long>(ch - '0'),
          std::numeric_limits<int>::max());
      inNumber = true;
    } else {
      pushPart();
    }
  }
  pushPart();
  return parts;
}

int CompareVersions(std::string_view left, std::string_view right) {
  const auto leftParts = ParseVersionParts(left);
  const auto rightParts = ParseVersionParts(right);
  const size_t count = std::max(leftParts.size(), rightParts.size());

  for (size_t i = 0; i < count; ++i) {
    const int leftPart = i < leftParts.size() ? leftParts[i] : 0;
    const int rightPart = i < rightParts.size() ? rightParts[i] : 0;
    if (leftPart < rightPart) {
      return -1;
    }
    if (leftPart > rightPart) {
      return 1;
    }
  }
  return 0;
}

bool RuleMatchesVersion(const nlohmann::json &rule,
                        const std::string &minecraftVersion) {
  const std::string minVersion = ReadStringField(rule, "min").value_or("");
  const std::string maxVersion = ReadStringField(rule, "max").value_or("");

  if (minecraftVersion.empty()) {
    return minVersion.empty() && maxVersion.empty();
  }

  if (!minVersion.empty() && CompareVersions(minecraftVersion, minVersion) < 0) {
    return false;
  }
  if (!maxVersion.empty() && CompareVersions(minecraftVersion, maxVersion) > 0) {
    return false;
  }
  return true;
}

std::optional<GameHookSignatures> ParseHookSignatures(const nlohmann::json &rule) {
  const nlohmann::json *sigs = &rule;
  auto sigsIt = rule.find("sigs");
  if (sigsIt != rule.end() && sigsIt->is_object()) {
    sigs = &(*sigsIt);
  }

  auto pauseMenuDtor = ReadStringField(*sigs, "pauseMenuDtorSig");
  auto pauseMenuOpen = ReadStringField(*sigs, "pauseMenuOpenSig");
  auto hudScreenDtor = ReadStringField(*sigs, "hudScreenDtorSig");
  auto hudScreenOpen = ReadStringField(*sigs, "hudScreenOpenSig");
  auto isShowingMenu = ReadStringField(*sigs, "isShowingMenuSig");

  if (!pauseMenuDtor || !pauseMenuOpen || !hudScreenDtor || !hudScreenOpen ||
      !isShowingMenu) {
    return std::nullopt;
  }

  return GameHookSignatures{
      *pauseMenuDtor,
      *pauseMenuOpen,
      *hudScreenDtor,
      *hudScreenOpen,
      *isShowingMenu,
  };
}

} // namespace

void ConfigureGameHookRules(std::string rulesPath, std::string minecraftVersion) {
  std::lock_guard<std::mutex> lock(g_rulesMutex);
  g_rulesPath = std::move(rulesPath);
  g_minecraftVersion = std::move(minecraftVersion);
}

std::optional<GameHookSignatures> LoadConfiguredGameHookSignatures() {
  std::string rulesPath;
  std::string minecraftVersion;
  {
    std::lock_guard<std::mutex> lock(g_rulesMutex);
    rulesPath = g_rulesPath;
    minecraftVersion = g_minecraftVersion;
  }

  auto content = ReadTextFile(rulesPath);
  if (!content) {
    preloaderLogger.warn("Preloader runtime data is unavailable: {}",
                          rulesPath.empty() ? "<empty>" : rulesPath);
    return std::nullopt;
  }

  nlohmann::json root = nlohmann::json::parse(*content, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    preloaderLogger.warn("Preloader runtime data JSON is invalid");
    return std::nullopt;
  }

  auto rulesIt = root.find("rules");
  if (rulesIt == root.end() || !rulesIt->is_array()) {
    preloaderLogger.warn("Preloader runtime data missing rules array");
    return std::nullopt;
  }

  for (const auto &rule : *rulesIt) {
    if (!rule.is_object() || !RuleMatchesVersion(rule, minecraftVersion)) {
      continue;
    }

    auto signatures = ParseHookSignatures(rule);
    if (signatures) {
      preloaderLogger.info("Loaded Preloader runtime data for Minecraft {}",
                            minecraftVersion.empty() ? "<unknown>"
                                                     : minecraftVersion);
      return signatures;
    }
  }

  preloaderLogger.warn("No valid Preloader runtime data matches Minecraft {}",
                        minecraftVersion.empty() ? "<unknown>"
                                                 : minecraftVersion);
  return std::nullopt;
}

} // namespace pl::runtime
