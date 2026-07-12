#include "pl/runtime/InputBridge.h"

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include "pl/Input.hpp"
#include "pl/runtime/JavaRuntime.h"

namespace pl::runtime {
namespace {

std::vector<PreloaderInput_OnTouch_Fn> g_touchCallbacks;
std::vector<PreloaderInput_OnKeyEvent_Fn> g_keyEventCallbacks;
std::vector<PreloaderInput_OnTextInput_Fn> g_textInputCallbacks;
std::vector<PreloaderInput_OnMouse_Fn> g_mouseCallbacks;
std::vector<pl::input::TouchCallback> g_cppTouchCallbacks;
std::vector<pl::input::KeyCallback> g_cppKeyCallbacks;
std::vector<pl::input::TextInputCallback> g_cppTextInputCallbacks;
std::vector<pl::input::MouseCallback> g_cppMouseCallbacks;
std::mutex g_callbackMutex;

void RegisterLegacyTouchCallback(PreloaderInput_OnTouch_Fn callback) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_touchCallbacks.push_back(callback);
}

void RegisterLegacyKeyEventCallback(PreloaderInput_OnKeyEvent_Fn callback) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_keyEventCallbacks.push_back(callback);
}

void RegisterLegacyTextInputCallback(PreloaderInput_OnTextInput_Fn callback) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_textInputCallbacks.push_back(callback);
}

void RegisterLegacyMouseCallback(PreloaderInput_OnMouse_Fn callback) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_mouseCallbacks.push_back(callback);
}

void ShowKeyboardImpl() { CallActivityVoidMethod("showSoftKeyboard"); }

void HideKeyboardImpl() { CallActivityVoidMethod("hideSoftKeyboard"); }

PreloaderInput_Interface g_inputInterface = {
    .RegisterTouchCallback = RegisterLegacyTouchCallback,
    .RegisterKeyEventCallback = RegisterLegacyKeyEventCallback,
    .ShowKeyboard = ShowKeyboardImpl,
    .HideKeyboard = HideKeyboardImpl,
    .RegisterMouseCallback = RegisterLegacyMouseCallback,
    .RegisterTextInputCallback = RegisterLegacyTextInputCallback,
};

} // namespace

PreloaderInput_Interface *GetInputInterface() { return &g_inputInterface; }

bool DispatchTouch(int action, int pointerId, float x, float y) {
  std::vector<PreloaderInput_OnTouch_Fn> legacyCallbacks;
  std::vector<pl::input::TouchCallback> cppCallbacks;
  {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    legacyCallbacks = g_touchCallbacks;
    cppCallbacks = g_cppTouchCallbacks;
  }

  bool consumed = false;
  for (auto callback : legacyCallbacks) {
    if (callback) {
      consumed |= callback(action, pointerId, x, y);
    }
  }
  const pl::input::TouchEvent event{
      .action = action,
      .pointerId = pointerId,
      .x = x,
      .y = y,
  };
  for (const auto &callback : cppCallbacks) {
    if (callback) {
      consumed |= callback(event);
    }
  }
  return consumed;
}

bool DispatchKeyEvent(int keyCode, unsigned int unicodeChar, bool isKeyDown) {
  std::vector<PreloaderInput_OnKeyEvent_Fn> legacyCallbacks;
  std::vector<pl::input::KeyCallback> cppCallbacks;
  {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    legacyCallbacks = g_keyEventCallbacks;
    cppCallbacks = g_cppKeyCallbacks;
  }

  bool consumed = false;
  for (auto callback : legacyCallbacks) {
    if (callback) {
      consumed |= callback(keyCode, unicodeChar, isKeyDown);
    }
  }
  const pl::input::KeyEvent event{
      .keyCode = keyCode,
      .unicodeChar = unicodeChar,
      .isKeyDown = isKeyDown,
  };
  for (const auto &callback : cppCallbacks) {
    if (callback) {
      consumed |= callback(event);
    }
  }
  return consumed;
}

bool DispatchTextInput(std::string text) {
  std::vector<PreloaderInput_OnTextInput_Fn> legacyCallbacks;
  std::vector<pl::input::TextInputCallback> cppCallbacks;
  {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    legacyCallbacks = g_textInputCallbacks;
    cppCallbacks = g_cppTextInputCallbacks;
  }

  bool consumed = false;
  for (auto callback : legacyCallbacks) {
    if (callback) {
      consumed |= callback(text.data(), text.size());
    }
  }
  const pl::input::TextInputEvent event{.text = std::move(text)};
  for (const auto &callback : cppCallbacks) {
    if (callback) {
      consumed |= callback(event);
    }
  }
  return consumed;
}

bool DispatchMouse(int button, bool isDown) {
  std::vector<PreloaderInput_OnMouse_Fn> legacyCallbacks;
  std::vector<pl::input::MouseCallback> cppCallbacks;
  {
    std::lock_guard<std::mutex> lock(g_callbackMutex);
    legacyCallbacks = g_mouseCallbacks;
    cppCallbacks = g_cppMouseCallbacks;
  }

  bool consumed = false;
  for (auto callback : legacyCallbacks) {
    if (callback) {
      consumed |= callback(button, isDown);
    }
  }
  const pl::input::MouseEvent event{.button = button, .isDown = isDown};
  for (const auto &callback : cppCallbacks) {
    if (callback) {
      consumed |= callback(event);
    }
  }
  return consumed;
}

void RegisterCppTouchCallback(pl::input::TouchCallback callback) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_cppTouchCallbacks.push_back(std::move(callback));
}

void RegisterCppKeyCallback(pl::input::KeyCallback callback) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_cppKeyCallbacks.push_back(std::move(callback));
}

void RegisterCppTextInputCallback(pl::input::TextInputCallback callback) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_cppTextInputCallbacks.push_back(std::move(callback));
}

void RegisterCppMouseCallback(pl::input::MouseCallback callback) {
  std::lock_guard<std::mutex> lock(g_callbackMutex);
  g_cppMouseCallbacks.push_back(std::move(callback));
}

void ShowKeyboard() { ShowKeyboardImpl(); }

void HideKeyboard() { HideKeyboardImpl(); }

} // namespace pl::runtime

namespace pl::input {

void registerTouchCallback(TouchCallback callback) {
  pl::runtime::RegisterCppTouchCallback(std::move(callback));
}

void registerKeyCallback(KeyCallback callback) {
  pl::runtime::RegisterCppKeyCallback(std::move(callback));
}

void registerTextInputCallback(TextInputCallback callback) {
  pl::runtime::RegisterCppTextInputCallback(std::move(callback));
}

void registerMouseCallback(MouseCallback callback) {
  pl::runtime::RegisterCppMouseCallback(std::move(callback));
}

void showKeyboard() { pl::runtime::ShowKeyboard(); }

void hideKeyboard() { pl::runtime::HideKeyboard(); }

} // namespace pl::input
