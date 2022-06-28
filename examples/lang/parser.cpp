#include <tiny_bnf.h>

#include <iostream>

namespace bnf = tiny_bnf;

void printTree(bnf::Node node, bool isRoot = true) {
  if (!isRoot)
    std::cout << node.symbol;

  if (size(node.children)) {
    if (size(node.children) != 1) {
      if (!isRoot)
        std::cout << '[';
    } else {
      std::cout << ':';
    }

    for (size_t i = 0; i < size(node.children); ++i) {
      printTree(node.children[i], false);
      if (i != size(node.children) - 1)
        std::cout << ' ';
    }

    if (size(node.children) != 1)
      if (!isRoot)
        std::cout << ']';
  }
}

void printRules(bnf::Specification spec) {
  for (auto r : spec) {
    std::cout << r.symbol << " ::= ";
    for (auto e : r.expr)
      std::cout << e.symbol << ' ';
    std::cout << '\n';
  }
}

void parse(std::string text, bnf::Terminals terminals, bnf::Specification spec) {
  text[0] = std::tolower(text[0]);
  if (text[0] == 'i')
    text[0] = 'I';
  if (auto tokens = bnf::tokenize(terminals, text, true))
    if (auto trees = bnf::parse(spec, *tokens))
      for (auto tree : *trees) {
        printTree(tree);
        std::cout << '\n';
      }
    else
      std::cout << "parser error: " << trees.error() << '\n';
  else
    std::cout << "tokenizer error: " << tokens.error() << '\n';

  std::cout << '\n';
}

int main() {
  auto [spec, terminals] = bnf::parseSpec("examples/lang/grammar.txt");

  bnf::forEachLine(bnf::readFile("examples/lang/sentences.txt"),
                   [spec = spec, terminals = terminals](std::string line) { parse(line, terminals, spec); });
}