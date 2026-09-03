#pragma once

#include "Export.hpp"
#include <string>
#include <string_view>

namespace pl::platform {

struct HttpResponse {
    int status{};
    std::string body;
    std::string retryAfter;

    [[nodiscard]] bool ok() const { return status >= 200 && status < 300; }
};

PL_EXPORT bool setClipboardText(std::string_view text);
PL_EXPORT HttpResponse httpGet(std::string_view url, int timeoutMs = 5000);

}
