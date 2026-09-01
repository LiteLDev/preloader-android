#pragma once

#include <functional>
#include <string>

#include "pl/Input.hpp"
#include "pl/legacy/LegacyInput.h"

namespace pl::runtime {

PreloaderInput_Interface *GetInputInterface();
bool DispatchTouch(int action, int pointerId, float x, float y);
bool DispatchKeyEvent(int keyCode, unsigned int unicodeChar, bool isKeyDown);
bool DispatchTextInput(std::string text);
bool DispatchMouse(int button, bool isDown);
bool RequestDocument(std::string mimeType,
                     pl::input::DocumentCallback callback);
void DispatchDocumentResult(pl::input::DocumentResult result);
void ClearDocumentRequest();

} // namespace pl::runtime
