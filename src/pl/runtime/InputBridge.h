#pragma once

#include "pl/c/PreloaderInput.h"

namespace pl::runtime {

PreloaderInput_Interface *GetInputInterface();
bool DispatchTouch(int action, int pointerId, float x, float y);
bool DispatchKeyEvent(int keyCode, unsigned int unicodeChar, bool isKeyDown);

} // namespace pl::runtime

