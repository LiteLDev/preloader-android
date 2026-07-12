#pragma once

#include <string>

#include "pl/legacy/LegacyInput.h"

namespace pl::runtime {

PreloaderInput_Interface *GetInputInterface();
bool DispatchTouch(int action, int pointerId, float x, float y);
bool DispatchKeyEvent(int keyCode, unsigned int unicodeChar, bool isKeyDown);
bool DispatchTextInput(std::string text);
bool DispatchMouse(int button, bool isDown);

} // namespace pl::runtime
