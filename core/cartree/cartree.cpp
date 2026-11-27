// Cartesian tree of a u32 array (contract in cartree.h): min-heap, leftmost minimum is the root, built in one left-to-right pass.
// No allocation, no recursion, no global state, no scratch: the output arrays are the working space. The right spine of the tree built so far is
// reached from the last node i - 1 through the parent pointers, so no stack is needed.
#include "cartree.h"
namespace lumora::cartree {
CartResult cartesian_tree(const std::uint32_t* a, std::uint32_t n, std::int32_t* parent, std::int32_t* left, std::int32_t* right, std::int32_t* root) {
  if (root == nullptr) return CartResult::NullArgument;
  if (n > kMaxN) return CartResult::TooLarge;
  if (n == 0u) {
    *root = kNone;
    return CartResult::Ok;
  }
  if (a == nullptr || parent == nullptr || left == nullptr || right == nullptr) return CartResult::NullArgument;
  std::int32_t top = 0;  // the root of the tree built over a[0 .. i-1]
  parent[0] = kNone;
  left[0] = kNone;
  right[0] = kNone;
  for (std::uint32_t i = 1; i < n; ++i) {
    const std::int32_t node = static_cast<std::int32_t>(i);
    parent[i] = kNone;
    left[i] = kNone;
    right[i] = kNone;
    // Walk up the right spine from i - 1 while the value is strictly greater than a[i]: those nodes go below i (equal values stay above, so the
    // leftmost minimum stays the root). Each node is passed over at most once, because after it is passed it is no longer on the right spine.
    std::int32_t cur = node - 1;
    std::int32_t passed = kNone;  // the last (highest) node passed over
    while (cur != kNone && a[static_cast<std::uint32_t>(cur)] > a[i]) {
      passed = cur;
      cur = parent[static_cast<std::uint32_t>(cur)];
    }
    if (passed != kNone) {
      left[i] = passed;
      parent[static_cast<std::uint32_t>(passed)] = node;
    }
    if (cur != kNone) {
      right[static_cast<std::uint32_t>(cur)] = node;
      parent[i] = cur;
    } else {
      top = node;
    }
  }
  *root = top;
  return CartResult::Ok;
}
}  // namespace lumora::cartree
