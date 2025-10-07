#pragma once
#include <cstdint>
#include <string>

namespace signature {

uintptr_t resolveSignature(const std::string &signature,
                           const std::string &moduleName = "libminecraftpe.so");

} // namespace signature