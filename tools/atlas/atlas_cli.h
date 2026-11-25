// lumora-atlas command line logic, separate from main() so the tests can run it in-process.
#pragma once
#include <iosfwd>
#include <string>
#include <vector>

namespace lumora::atlas_tool {

// args excludes the program name. Exit codes: 0 success, 1 packer or input error, 2 usage error.
int run(const std::vector<std::string>& args, std::ostream& out, std::ostream& err);

}  // namespace lumora::atlas_tool
