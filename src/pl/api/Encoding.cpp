#include "pl/api/Encoding.h"

namespace encoding {
std::string base64Encode(const uint8_t* data, size_t length) {
    const char  base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int         padding = 0;

    for (size_t i = 0; i < length; i += 3) {
        uint32_t buffer = 0;
        int      bytes  = 0;

        for (int j = 0; j < 3; j++) {
            if (i + j < length) {
                buffer = (buffer << 8) | data[i + j];
                bytes++;
            } else {
                buffer <<= 8;
                padding++;
            }
        }
 
        for (int j = 0; j < 4; j++) {
            if (j <= bytes) {
                uint8_t index  = (buffer >> (18 - j * 6)) & 0x3F;
                encoded       += base64Table[index];
            } else {
                encoded += '=';
            }
        }
    }

    return encoded;
}

} // namespace encoding