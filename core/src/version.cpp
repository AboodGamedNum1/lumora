#include "lumora/version.h"

#include "lumora/config.h"

namespace lumora {

std::string version_string() {
  return std::string(kProductName) + " " + LUMORA_VERSION_STRING;
}

std::string toolchain_string() {
  std::string s;
#if defined(_MSC_VER) && !defined(__clang__)
  s = "msvc " + std::to_string(_MSC_VER);
#elif defined(__clang__)
  s = "clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__GNUC__)
  s = "gcc " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#else
  s = "unknown";
#endif
#if defined(_MSVC_LANG)
  s += " lang=" + std::to_string(_MSVC_LANG);
#else
  s += " lang=" + std::to_string(__cplusplus);
#endif
  return s;
}

}  // namespace lumora
