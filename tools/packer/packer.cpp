// lumora-packer: build and verify .lumora packages. Usage:
//   lumora-packer pack <source-dir> <output.lumora>
//   lumora-packer verify <package.lumora>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

#include "pack_dir.h"
#include "zip_store.h"

using namespace lumora::scene;

int main(int argc, char** argv) {
  if (argc == 4 && std::strcmp(argv[1], "pack") == 0) {
    const auto r = pack_directory(argv[2]);
    if (!r.ok) { std::fprintf(stderr, "pack failed: %s\n", r.error.c_str()); return 1; }
    const auto z = write_zip(r.files, 6);
    std::ofstream out(argv[3], std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(z.data()), static_cast<std::streamsize>(z.size()));
    if (!out) { std::fprintf(stderr, "cannot write %s\n", argv[3]); return 1; }
    std::printf("packed %zu files into %s\n", r.files.size(), argv[3]);
    return 0;
  }
  if (argc == 3 && std::strcmp(argv[1], "verify") == 0) {
    std::ifstream in(argv[2], std::ios::binary);
    std::vector<std::uint8_t> z((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (z.empty()) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 1; }
    const auto zr = read_zip_store(z.data(), z.size());
    if (!zr.ok) { std::fprintf(stderr, "invalid archive: %s\n", zr.error.c_str()); return 1; }
    const auto lr = load_package(zr.files);
    if (!lr.ok()) { std::fprintf(stderr, "invalid package: %s\n", lr.message.c_str()); return 1; }
    std::printf("ok: %s (%zu files)\n", lr.manifest.name.c_str(), zr.files.size());
    return 0;
  }
  std::fprintf(stderr, "usage: lumora-packer pack <dir> <out.lumora> | verify <file.lumora>\n");
  return 2;
}
