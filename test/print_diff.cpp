#include <sstream>
#include <vector>

#include "print_diff.h"

#include "dtl/dtl.hpp"

std::vector<std::string> get_lines(const std::string &code) {
  std::istringstream buf{code};
  std::string line;
  std::vector<std::string> lines;
  while (std::getline(buf, line))
    lines.push_back(line);
  return lines;
}

void print_diff(const std::string &code1, const std::string &code2) {
  auto lines1 = get_lines(code1);
  auto lines2 = get_lines(code2);

  dtl::Diff<std::string> diff(lines1, lines2);
  diff.onHuge();
  diff.compose();

  diff.composeUnifiedHunks();
  diff.printUnifiedFormat();
}
