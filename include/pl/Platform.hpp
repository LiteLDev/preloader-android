#pragma once

#include "Export.hpp"
#include <string_view>

namespace pl::platform {

PL_EXPORT bool setClipboardText(std::string_view text);

}
