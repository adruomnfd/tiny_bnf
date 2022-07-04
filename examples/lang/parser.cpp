#include <tiny_bnf.h>

#include <fstream>
#include <iostream>
#include <set>

namespace bnf = tiny_bnf;

void printTree(bnf::Node node, bool isRoot = true) {
  if (!isRoot)
    std::cout << node.symbol;

  if (size(node.children)) {
    if (!isRoot) {
      if (size(node.children) != 1) {
        if (!isRoot)
          std::cout << '[';
      } else {
        std::cout << ':';
      }
    }

    for (size_t i = 0; i < size(node.children); ++i) {
      printTree(node.children[i], false);
      if (i != size(node.children) - 1)
        std::cout << ' ';
    }

    if (!isRoot)
      if (size(node.children) != 1)
        if (!isRoot)
          std::cout << ']';
  }
}

int parse(std::string text, bnf::Terminals terminals, bnf::Specification spec) {
  std::cout << text << '\n';
  if (auto tokens = bnf::tokenize(terminals, text, true))
    if (auto trees = bnf::parse(spec, *tokens))
      for (auto tree : *trees) {
        printTree(tree, false);
        std::cout << '\n';
      }
    else {
      std::cout << "parser error: " << trees.error() << "\n\n";
      return 1;
    }
  else {
    std::cout << "tokenizer error: " << tokens.error() << "\n\n";
    return 1;
  }

  std::cout << '\n';
  return 0;
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

auto importWords(bnf::Specification& spec) {
  std::set<std::string> properNouns = {"I"};

  auto import = [&](auto filename) {
    std::string type;
    bnf::forEachLine(readFile(dir + filename), [&](auto w) {
      if (auto p = w.find_first_of(' '); p != w.npos)
        w = w.substr(0, p);
      if (w[0] == '#')
        type = w.substr(1);
      else {
        spec[type] >= w;
        if (type == "NPR" || type == "NPRS")
          properNouns.insert(w);
      }
    });
  };

  import("noun.txt");
  import("adj.txt");
  import("adv.txt");
  import("p.txt");

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

  return properNouns;
}

int main() {
  auto spec = bnf::parseSpec(readFile(dir + "grammar2.txt"));
  auto properNouns = importWords(spec);

  auto terminals = bnf::autoTerminals(spec);
  terminals[" "] = "";

  int ret = 0;

  bnf::forEachLine(readFile(dir + "sentence.txt"), [&](auto line) {
    if (line[0] != '#') {
      std::stringstream ss(line);
      std::string word;
      line.clear();
      while (ss >> word) {
        if (properNouns.find(word) == end(properNouns))
          for (auto& l : word)
            if (std::isalpha(l))
              l = std::tolower(l);
        line += word + " ";
      }
      line.pop_back();

      ret += parse(line, terminals, spec);
    }
  });

  if (ret)
    abort();

  return ret;
}