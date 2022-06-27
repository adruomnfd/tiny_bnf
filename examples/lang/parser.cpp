#include <tiny_bnf.h>

#include <fstream>
#include <iostream>

namespace bnf = tiny_bnf;

auto toLower(std::string text) {
  for (auto& c : text)
    if (c >= 'A' && c <= 'Z')
      c += 'a' - 'A';
  return text;
}

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

void importSpec(bnf::Specification& spec, bnf::Terminals& terminals, std::string sym, std::string filename) {
  std::ifstream lines(filename);
  std::string line;
  while (std::getline(lines, line)) {
    spec[sym] >= line;
    terminals[line];
  }
}

void parse(std::string text, bnf::Terminals terminals, bnf::Specification spec) {
  text = toLower(text);
  if (auto tokens = bnf::tokenize(terminals, text, true))
    if (auto tree = bnf::parse(spec, *tokens))
      printTree(*tree);
    else
      std::cout << "parser error: " << tree.error() << '\n';
  else
    std::cout << "tokenizer error: " << tokens.error() << '\n';
  std::cout << '\n';
};

int main() {
  constexpr auto OR = bnf::OR;
  using bnf::arb;
  using bnf::opt;

  bnf::Specification spec;
  bnf::Terminals terminals;
  spec["IP-MAT"] >= "SBJ", opt("ADVP"), "VB", "OB", arb("PP"), opt("PU");

  spec["SBJ"] >= "NP";
  spec["OB"] >= "NP";

  spec["NP"] >= opt("D"), arb("ADJP"), "N";
  spec["ADJP"] >= opt("ADVP"), "ADJ";
  spec["ADVP"] >= opt("ADVP"), "ADV";
  spec["PP"] >= "P-ROLE", "NP";

  spec["P-ROLE"] >= "P";
  spec["D"] >= "the";
  spec["PU"] >= ".";

  importSpec(spec, terminals, "N", "examples/lang/nouns.txt");
  importSpec(spec, terminals, "VB", "examples/lang/verbs.txt");
  importSpec(spec, terminals, "ADJ", "examples/lang/adj.txt");
  importSpec(spec, terminals, "ADV", "examples/lang/adv.txt");
  importSpec(spec, terminals, "P", "examples/lang/p.txt");
  terminals["the"];
  terminals["."] = ".";
  terminals[" "] = "";

  std::ifstream lines("examples/lang/sentences.txt");
  std::string line;
  while (std::getline(lines, line))
    parse(line, terminals, spec);
}