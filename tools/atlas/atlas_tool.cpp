// lumora-atlas: packs PNG files into one atlas PNG and a JSON index. See docs/atlas.md.
#include <iostream>
#include <string>
#include <vector>

#include "atlas_cli.h"

int main(int argc, char** argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
  return lumora::atlas_tool::run(args, std::cout, std::cerr);
}
