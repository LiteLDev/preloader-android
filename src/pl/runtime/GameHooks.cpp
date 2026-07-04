#include "pl/runtime/GameHooks.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

#include "pl/Logger.h"
#include "pl/c/Hook.h"
#include "pl/c/Signature.h"
#include "pl/cpp/Signature.hpp"
#include "pl/runtime/GameHookRules.h"

namespace pl::runtime {
namespace {

std::atomic_bool g_isPauseMenuOpen{false};
std::atomic_bool g_isHudScreenOpen{false};
std::atomic_bool g_isShowingMenu{false};
std::atomic_bool g_forceGlobalModMenu{true};
std::once_flag g_gameHooksOnce;

void (*orig_PauseMenuDtor)(void *) = nullptr;
void hook_PauseMenuDtor(void *_this) {
  g_isPauseMenuOpen.store(false, std::memory_order_relaxed);
  if (orig_PauseMenuDtor) {
    orig_PauseMenuDtor(_this);
  }
}

void (*orig_PauseMenuOpen)(void *) = nullptr;
void hook_PauseMenuOpen(void *_this) {
  g_isPauseMenuOpen.store(true, std::memory_order_relaxed);
  if (orig_PauseMenuOpen) {
    orig_PauseMenuOpen(_this);
  }
}

void (*orig_HudScreenDtor)(void *) = nullptr;
void hook_HudScreenDtor(void *_this) {
  g_isHudScreenOpen.store(false, std::memory_order_relaxed);
  if (orig_HudScreenDtor) {
    orig_HudScreenDtor(_this);
  }
}

void (*orig_HudScreenOpen)(void *) = nullptr;
void hook_HudScreenOpen(void *_this) {
  g_isHudScreenOpen.store(true, std::memory_order_relaxed);
  if (orig_HudScreenOpen) {
    orig_HudScreenOpen(_this);
  }
}

bool (*orig_isShowingMenu)(void *) = nullptr;
bool hook_isShowingMenu(void *_this) {
  bool res = false;
  if (orig_isShowingMenu) {
    res = orig_isShowingMenu(_this);
  }
  g_isShowingMenu.store(res, std::memory_order_relaxed);
  return res;
}

uintptr_t ResolveResult(
    const std::unordered_map<std::string, uintptr_t> &results,
    const std::string &signature) {
  auto it = results.find(signature);
  return it == results.end() ? 0 : it->second;
}

bool InstallHook(uintptr_t target, PLFuncPtr detour, PLFuncPtr *original,
                 const char *name) {
  if (!target) {
    preloader_logger.warn("Preloader hook target is missing: {}", name);
    return false;
  }

  if (pl_hook(reinterpret_cast<PLFuncPtr>(target), detour, original,
              PL_HOOK_PRIORITY_NORMAL) != 0) {
    preloader_logger.warn("Failed to install Preloader hook: {}", name);
    return false;
  }
  return true;
}

} // namespace

void ConfigureGameHooks(std::string rulesPath, std::string minecraftVersion) {
  ConfigureGameHookRules(std::move(rulesPath), std::move(minecraftVersion));
  g_forceGlobalModMenu.store(true, std::memory_order_relaxed);
}

void InitGameHooks() {
  std::call_once(g_gameHooksOnce, [] {
    g_forceGlobalModMenu.store(true, std::memory_order_relaxed);
    auto signatures = LoadConfiguredGameHookSignatures();
    if (!signatures) {
      return;
    }

    auto results = pl::signature::resolveSignatures(
        {signatures->pauseMenuDtor, signatures->pauseMenuOpen,
         signatures->hudScreenDtor, signatures->hudScreenOpen,
         signatures->isShowingMenu},
        "libminecraftpe.so");

    uintptr_t pauseDtor = ResolveResult(results, signatures->pauseMenuDtor);
    uintptr_t pauseOpen = ResolveResult(results, signatures->pauseMenuOpen);
    uintptr_t hudDtor = ResolveResult(results, signatures->hudScreenDtor);
    uintptr_t hudOpen = ResolveResult(results, signatures->hudScreenOpen);
    uintptr_t isShowingMenuAddr =
        ResolveResult(results, signatures->isShowingMenu);

    bool hooksReady = true;
    hooksReady &= InstallHook(pauseDtor, (PLFuncPtr)hook_PauseMenuDtor,
                              (PLFuncPtr *)&orig_PauseMenuDtor,
                              "PauseMenuDtor");
    hooksReady &= InstallHook(pauseOpen, (PLFuncPtr)hook_PauseMenuOpen,
                              (PLFuncPtr *)&orig_PauseMenuOpen,
                              "PauseMenuOpen");
    hooksReady &= InstallHook(hudDtor, (PLFuncPtr)hook_HudScreenDtor,
                              (PLFuncPtr *)&orig_HudScreenDtor,
                              "HudScreenDtor");
    hooksReady &= InstallHook(hudOpen, (PLFuncPtr)hook_HudScreenOpen,
                              (PLFuncPtr *)&orig_HudScreenOpen,
                              "HudScreenOpen");
    hooksReady &= InstallHook(isShowingMenuAddr, (PLFuncPtr)hook_isShowingMenu,
                              (PLFuncPtr *)&orig_isShowingMenu,
                              "isShowingMenu");

    if (!hooksReady) {
      preloader_logger.warn(
          "Preloader runtime data is not fully usable; forcing global Mod Menu");
      return;
    }

    g_forceGlobalModMenu.store(false, std::memory_order_relaxed);
  });
}

bool IsPauseMenuOpen() {
  return g_isPauseMenuOpen.load(std::memory_order_relaxed);
}

bool IsHudScreenOpen() {
  return g_isHudScreenOpen.load(std::memory_order_relaxed);
}

bool IsShowingMenu() {
  return g_isShowingMenu.load(std::memory_order_relaxed);
}

bool ShouldForceGlobalModMenu() {
  return g_forceGlobalModMenu.load(std::memory_order_relaxed);
}

} // namespace pl::runtime
