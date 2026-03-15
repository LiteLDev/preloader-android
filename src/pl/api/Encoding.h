#pragma once

#include <cstdint>
#include <string>

namespace encoding {

inline std::string base64Encode(const uint8_t *data, size_t length) {
    static constexpr char kBase64Table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((length + 2U) / 3U) * 4U);

    for (size_t i = 0; i < length; i += 3U) {
        uint32_t buffer = 0;
        int bytes = 0;

        for (int j = 0; j < 3; ++j) {
            buffer <<= 8U;
            if (i + static_cast<size_t>(j) < length) {
                buffer |= data[i + static_cast<size_t>(j)];
                ++bytes;
            }
        }

        for (int j = 0; j < 4; ++j) {
            if (j <= bytes) {
                const auto index = static_cast<uint8_t>(
                    (buffer >> (18 - j * 6)) & 0x3FU);
                encoded += kBase64Table[index];
            } else {
                encoded += '=';
            }
        }
    }

    return encoded;
}

} // namespace encoding
