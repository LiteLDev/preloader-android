#include "pl/legacy/LegacyModMenu.h"

#include "pl/runtime/ModMenuBridge.h"

extern "C" {

PL_LEGACY_EXPORT PLModMenu_Interface *GetPreloaderModMenu() {
  return pl::runtime::GetModMenuInterface();
}

} // extern "C"

