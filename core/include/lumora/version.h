#pragma once
#include <string>

namespace lumora {
// Returns "<product name> <version>", for example "Lumora 0.0.1".
std::string version_string();
// Returns a short description of the compiler and language level used for the build.
std::string toolchain_string();
}  // namespace lumora
