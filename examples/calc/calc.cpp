#include "calc.h"

int main() {
  auto parser = buildParser();

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line == "quit") break;
    if (auto ret = eval(line, parser))
      std::cout << "= " << *ret << '\n';
    else
      std::cout << ret.error() << '\n';
  }
}