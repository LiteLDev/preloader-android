#pragma once

/**
 * @file Input.hpp
 * @brief Input callback API for native mods.
 */

#include <functional>
#include <string>

#include "pl/Export.hpp"

namespace pl::input {

/**
 * @brief Touch event delivered from the Android view.
 */
struct TouchEvent {
  int action{};
  int pointerId{};
  float x{};
  float y{};
};

/**
 * @brief Keyboard event delivered from the Android view.
 */
struct KeyEvent {
  int keyCode{};
  unsigned int unicodeChar{};
  bool isKeyDown{};
};

/**
 * @brief Text committed by an input method.
 */
struct TextInputEvent {
  std::string text;
};

/**
 * @brief Mouse button event delivered from the Android view.
 */
struct MouseEvent {
  int button{};
  bool isDown{};
};

using TouchCallback = std::function<bool(const TouchEvent &)>;
using KeyCallback = std::function<bool(const KeyEvent &)>;
using TextInputCallback = std::function<bool(const TextInputEvent &)>;
using MouseCallback = std::function<bool(const MouseEvent &)>;

/**
 * @brief Registers a process-wide touch callback.
 */
PL_EXPORT void registerTouchCallback(TouchCallback callback);

/**
 * @brief Registers a process-wide key callback.
 */
PL_EXPORT void registerKeyCallback(KeyCallback callback);

/**
 * @brief Registers a process-wide committed text callback.
 */
PL_EXPORT void registerTextInputCallback(TextInputCallback callback);

/**
 * @brief Registers a process-wide mouse callback.
 */
PL_EXPORT void registerMouseCallback(MouseCallback callback);

/**
 * @brief Requests the Android soft keyboard.
 */
PL_EXPORT void showKeyboard();

/**
 * @brief Hides the Android soft keyboard.
 */
PL_EXPORT void hideKeyboard();

} // namespace pl::input
