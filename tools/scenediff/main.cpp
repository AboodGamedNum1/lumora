// lumora-scenediff: compare two Lumora packages (.lumora archive or scene directory). Usage:
//   lumora-scenediff A B [--format text|json] [--quiet]
// Exit codes: 0 identical, 1 differences, 2 error. Reads only A and B, writes only to stdout and stderr.
// All behaviour lives in lumora::scenediff::run_cli so the tests can run it in-process.
#include <cstdio>
#include <string>
#include <vector>

#include "scene_diff.h"

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);
  std::string out, err;
  const int code = lumora::scenediff::run_cli(args, out, err);
  std::fwrite(out.data(), 1, out.size(), stdout);
  std::fwrite(err.data(), 1, err.size(), stderr);
  return code;
}
