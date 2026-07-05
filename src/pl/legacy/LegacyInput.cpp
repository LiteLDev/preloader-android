#include "pl/legacy/LegacyInput.h"

#include "pl/runtime/InputBridge.h"

extern "C" {

PL_LEGACY_EXPORT PreloaderInput_Interface *GetPreloaderInput() {
  return pl::runtime::GetInputInterface();
}

} // extern "C"

