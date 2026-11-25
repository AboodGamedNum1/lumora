// Placeholder for the easing module. The contributor replaces this file; the interface is core/anim/easing.h.
#include "easing.h"

namespace lumora::anim {

float ease(Ease, float) { return 0.0f; }
const char* ease_name(Ease) { return "not implemented"; }
bool ease_from_name(const std::string&, Ease&) { return false; }
bool cubic_bezier(float, float, float, float, float, float&) { return false; }
bool tween_value(const Tween&, float, float&) { return false; }

}  // namespace lumora::anim
