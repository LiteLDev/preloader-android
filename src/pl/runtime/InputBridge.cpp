#include "pl/runtime/InputBridge.h"

#include <mutex>
#include <vector>

#include "pl/runtime/JavaRuntime.h"

namespace pl::runtime {
namespace {

std::vector<PreloaderInput_OnTouch_Fn> g_touchCallbacks;
std::vector<PreloaderInput_OnKeyEvent_Fn> g_keyEventCallbacks;
std::vector<PreloaderInput_OnMouse_Fn> g_mouseCallbacks;
std::mutex g_callbackMutex;

void RegisterTouchCallback(PreloaderInput_OnTouch_Fn callback) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_touchCallbacks.push_back(callback);
}

void RegisterKeyEventCallback(PreloaderInput_OnKeyEvent_Fn callback) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_keyEventCallbacks.push_back(callback);
}

void RegisterMouseCallback(PreloaderInput_OnMouse_Fn callback) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_mouseCallbacks.push_back(callback);
}

void ShowKeyboard() { CallActivityVoidMethod("showSoftKeyboard"); }

void HideKeyboard() { CallActivityVoidMethod("hideSoftKeyboard"); }

PreloaderInput_Interface g_inputInterface = {
    .RegisterTouchCallback = RegisterTouchCallback,
    .RegisterKeyEventCallback = RegisterKeyEventCallback,
    .ShowKeyboard = ShowKeyboard,
    .HideKeyboard = HideKeyboard,
    .RegisterMouseCallback = RegisterMouseCallback,
};

} // namespace

PreloaderInput_Interface *GetInputInterface() { return &g_inputInterface; }

bool DispatchTouch(int action, int pointerId, float x, float y) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  bool consumed = false;
  for (auto callback : g_touchCallbacks) {
    if (callback) {
      consumed |= callback(action, pointerId, x, y);
    }
  }
  return consumed;
}

bool DispatchKeyEvent(int keyCode, unsigned int unicodeChar, bool isKeyDown) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  bool consumed = false;
  for (auto callback : g_keyEventCallbacks) {
    if (callback) {
      consumed |= callback(keyCode, unicodeChar, isKeyDown);
    }
  }
  return consumed;
}

bool DispatchMouse(int button, bool isDown) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  bool consumed = false;
  for (auto callback : g_mouseCallbacks) {
    if (callback) {
      consumed |= callback(button, isDown);
    }
  }
  return consumed;
}

} // namespace pl::runtime

