// Replay driver: runs LLVMFuzzerTestOneInput over every file under the given paths (files or directories).
// Lets the corpus run as an ordinary ctest on any compiler. Not a fuzzer. Owned by the project, fixed.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

int main(int argc, char** argv) {
  namespace fs = std::filesystem;
  std::size_t runs = 0;
  auto run_file = [&](const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::vector<std::uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    LLVMFuzzerTestOneInput(buf.data(), buf.size());
    ++runs;
  };
  for (int i = 1; i < argc; ++i) {
    std::error_code ec;
    if (fs::is_directory(argv[i], ec)) {
      for (const auto& e : fs::recursive_directory_iterator(argv[i], ec))
        if (e.is_regular_file(ec)) run_file(e.path());
    } else {
      run_file(argv[i]);
    }
  }
  // Always exercise the empty input too.
  LLVMFuzzerTestOneInput(nullptr, 0);
  std::printf("replayed %zu inputs\n", runs);
  return 0;
}
