#include <tiny_bnf.h>

#include <fstream>
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

void parse(std::string text, bnf::Terminals terminals, bnf::Specification spec) {
  text[0] = std::tolower(text[0]);
  if (text[0] == 'i')
    text[0] = 'I';
  std::cout << text << '\n';
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

auto readFile(std::string filename) -> std::string {
  std::ifstream file(filename);
  if (!file.is_open())
    std::cout << "cannot open: " << filename << '\n';
  file.seekg(0, file.end);
  std::string str;
  str.resize(file.tellg());
  file.seekg(file.beg);
  file.read(&str[0], size(str));
  return str;
}

auto split(std::string line) {
  std::vector<std::string> words;
  std::stringstream ss(line);
  std::string word;
  while (ss >> word)
    words.push_back(word);
  return words;
}

std::string dir = "examples/lang/";

void importWords(bnf::Specification& spec) {
  std::string type;
  bnf::forEachLine(readFile(dir + "noun.txt"), [&](auto w) {
    if (w[0] == '#')
      type = w.substr(1);
    else
      spec[type] >= w;
  });

  bnf::forEachLine(readFile(dir + "adj.txt"), [&](auto w) { spec["ADJ"] >= w; });

  bnf::forEachLine(readFile(dir + "adv.txt"), [&](auto w) { spec["ADV"] >= w; });

  bnf::forEachLine(readFile(dir + "verb.txt"), [&](auto w) {
    auto words = split(w.substr(0, w.find('/')));
    auto tags = split(w.substr(w.find('/') + 1));
    for (auto tag : tags) {
      if (size(words) == 8) {
        spec["VB~" + tag] >= words[0];
        spec["VBP~" + tag] >= words[1];
        spec["VBP~" + tag] >= words[2];
        spec["VBP~" + tag] >= words[3];
        spec["VBD~" + tag] >= words[4];
        spec["VBD~" + tag] >= words[5];
        spec["VVN~" + tag] >= words[6];
        spec["VAG~" + tag] >= words[7];
      } else {
        spec["VB~" + tag] >= words[0];
        spec["VBP~" + tag] >= words[0];
        spec["VBP~" + tag] >= words[1];
        spec["VBD~" + tag] >= words[2];
        spec["VVN~" + tag] >= words[3];
        spec["VAG~" + tag] >= words[4];
      }
    }
  });
}

int main() {
  auto spec = bnf::parseSpec(readFile(dir + "grammar.txt"));
  importWords(spec);

  auto terminals = bnf::autoTerminals(spec);
  terminals[" "] = "";

  bnf::forEachLine(readFile(dir + "sentences.txt"), [&](auto line) {
    if (line[0] != '#')
      parse(line, terminals, spec);
  });
}