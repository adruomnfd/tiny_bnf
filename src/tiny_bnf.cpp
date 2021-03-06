#include <tiny_bnf.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <regex>

namespace tiny_bnf {

template <typename It, typename T, typename U, typename F>
auto accumulateN(It it, T n, U init, F f) {
  for (T i = 0; i < n; ++i) init = f(init, it++);
  return init;
}

inline auto isWord(char c) -> bool {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

auto autoTerminals(const Specification &spec) -> Terminals {
  Terminals terminals;
  std::map<std::string, bool> ts;

  for (const auto &rule : spec)
    for (const auto &expr : rule.expr) ts[expr.symbol] = true;

  for (const auto &rule : spec) ts[rule.symbol] = false;

  for (const auto &terminal : ts)
    if (terminal.second) terminals[terminal.first];

  return terminals;
}

auto tokenize(const Terminals &terminals, std::string_view input, bool delimit)
    -> Expected<Tokens> {
  Tokens tokens;
  const auto &map = terminals.expr2Sym;

  auto n =
      accumulateN(size_t{1}, std::size(input), size_t{0}, [&](auto a, auto b) {
        if (!delimit || b == size(input) || !isWord(input[b]) ||
            !isWord(input[a]))
          if (auto it = map.find(input.substr(a, b - a)); it != std::end(map)) {
            if (it->second != "") tokens.push_back(it->second);
            return b;
          }
        return a;
      });

  if (n == std::size(input))
    return tokens;
  else
    return Error<>("Unable to tokenize: " + (std::string)input.substr(n));
}

template <typename F>
void traverseNode(Node n, F f) {
  f(n.symbol);
  for (auto &r : n.children) traverseNode(r, f);
}

struct State {
  size_t i = 0;
  size_t p = 0;
  Rule rule;
  Node node;
  size_t start = 0;
};
using StateSets = std::vector<std::vector<State>>;

auto isComplete(const State &state) { return state.p == size(state.rule.expr); }

auto match(const State &state, const Rule &r) {
  if (isComplete(state)) return false;
  for (auto &a : state.rule.expr[state.p].attribs)
    if (r.attributes.find(a) == end(r.attributes)) return false;
  return state.rule.expr[state.p].symbol == r.symbol;
}

auto predict(StateSets &stateSets, size_t k, size_t i, const Expr &next,
             const std::map<std::string, std::vector<Rule>> &rules,
             std::vector<bool> &addedRules) {
  auto it = rules.find(next.symbol);
  if (it == end(rules)) return false;
  for (auto &rule : it->second)
    if (!addedRules[rule.idx]) {
      stateSets[k].push_back(State{k, 0, rule, {rule.symbol, {}}, i});
      addedRules[rule.idx] = true;
    }
  return true;
}

auto scan(StateSets &stateSets, size_t k, const Expr &next, State s) {
  if (!s.rule.expr[s.p].arbitrary) s.p += 1;
  s.node.children.push_back(Node{next.symbol, {}});
  stateSets[k + 1].push_back(s);
}

auto complete(StateSets &stateSets, size_t k, size_t i) {
  auto sz = size(stateSets[stateSets[k][i].i]);
  for (size_t j = stateSets[k][i].start; j < sz; ++j)
    if (match(stateSets[stateSets[k][i].i][j], stateSets[k][i].rule)) {
      auto sc = stateSets[stateSets[k][i].i][j];

      if (sc.rule.expr[sc.p].oneOrMore) {
        sc.rule.expr[sc.p].oneOrMore = false;
        sc.rule.expr[sc.p].arbitrary = true;
      }

      if (stateSets[k][i].rule.intermediate || sc.rule.alias ||
          sc.rule.expr[sc.p].deref) {
        for (auto &n : stateSets[k][i].node.children)
          sc.node.children.push_back(n);
      } else {
        sc.node.children.push_back(stateSets[k][i].node);
      }

      mergeAttributes(sc.rule, stateSets[k][i].rule);

      if (!sc.rule.expr[sc.p].arbitrary) sc.p += 1;

      stateSets[k].push_back(std::move(sc));
    }
}

auto parseEarley(Specification spec, Tokens tokens)
    -> Expected<std::vector<Node>> {
  auto t0 = std::chrono::high_resolution_clock::now();
  auto stateSets = StateSets(size(tokens) + 1);
  for (size_t i = 0; i != size(spec.rules); ++i)
    if (spec.rules[i].symbol == spec.rules[0].symbol)
      stateSets[0].push_back(
          State{0, 0, spec.rules[i], Node{spec.rules[i].symbol, {}}});

  std::map<std::string, std::vector<Rule>> rules;
  for (auto &rule : spec) rules[rule.symbol].push_back(rule);
  std::vector<bool> addedRules(size(spec.rules));

  for (size_t k = 0; k <= size(tokens); ++k) {
    for (size_t i = 0; i < size(stateSets[k]); ++i) {
      auto show = [&](auto &s) {
        if (i == 0) std::cout << "\n";
        std::cout << s.i << ' ' << s.rule.symbol << " ??? ";
        for (size_t j = 0; j < s.p; j++)
          std::cout << s.rule.expr[j].symbol << ' ';
        std::cout << u8"??? ";
        for (size_t j = s.p; j < size(s.rule.expr); j++)
          std::cout << s.rule.expr[j].symbol << ' ';
        std::cout << '\n';
      };
      if (0) show(stateSets[k][i]);

      if (!isComplete(stateSets[k][i])) {
        auto s = stateSets[k][i];
        const auto &next = s.rule.expr[s.p];

        // prediction
        auto isNonTerminal = predict(stateSets, k, i, next, rules, addedRules);

        // scanning
        if (k != size(tokens) && !isNonTerminal && tokens[k] == next.symbol)
          scan(stateSets, k, next, s);

        if (next.optional || next.arbitrary) {
          s.p += 1;
          stateSets[k].push_back(std::move(s));
        }

      } else {
        // completion
        // show(stateSets[k][i]);
        complete(stateSets, k, i);
        addedRules[stateSets[k][i].rule.idx] = false;
      }
    }

    addedRules.clear();
    addedRules.resize(size(spec.rules));
  }

  std::vector<Node> nodes;
  bool any = false;

  for (auto &s : stateSets.back())
    if (s.i == 0 && s.node.symbol == spec.rules.front().symbol) {
      if (isComplete(s) &&
          std::find(begin(nodes), end(nodes), s.node) == end(nodes))
        nodes.push_back(s.node);
      any = true;
    }

   auto t1 = std::chrono::high_resolution_clock::now();
   std::cout << std::chrono::duration<float>(t1 - t0).count() << "\n";

  if (size(nodes) == 0)
    return Error<>(any ? "top node is not complete" : "no top node is parsed");

  return nodes;
}

auto parse(const Specification &spec, Tokens tokens, ParserType parserType)
    -> Expected<std::vector<Node>> {
  if (0) {
    for (auto r : spec) {
      std::cout << r.symbol << " ::= ";
      for (auto e : r.expr) {
        std::cout << e.symbol;
        if (e.optional) std::cout << "?";
        if (e.arbitrary) std::cout << "*";
        if (e.oneOrMore) std::cout << "+";
        if (e.deref) std::cout << "&";
        std::cout << " ";
      }
      std::cout << "\n";
    }
    abort();
  }

  switch (parserType) {
    case ParserType::Earley:
      return parseEarley(spec, std::move(tokens));
    default:
      return Error<>("Invalid parser type");
  }
}

auto generateImpl(const Generator &generator, const Node &node)
    -> Expected<std::pair<AnnotatedPtr, Generator::Concept *>> {
  using R = Expected<std::pair<AnnotatedPtr, Generator::Concept *>>;

  auto build = [&](auto &build, Node node) -> R {
    auto modelOpt = generator.findModel(node.symbol);
    if (!modelOpt) return Error<>("Cannot find model: " + node.symbol);
    auto model = *modelOpt;

    std::vector<AnnotatedPtr> args;
    std::vector<Generator::Concept *> models;

    if (!model->useStringToConstruct)
      for (auto child : node.children) {
        if (auto ret = build(build, child)) {
          auto [arg, model] = *ret;
          args.push_back(arg);
          models.push_back(model);
        } else {
          return ret;
        }
      }

    ScopeGuard guard{[&]() {
      for (size_t i = 0; i < std::size(args); ++i)
        models[i]->destruct(args[i].ptr);
    }};

    if (model->useStringToConstruct && size(node.children) != 1)
      return Error<>(
          "Model binded with _UseString_ tag requires one child in "
          "corresponding node");

    if (auto ptr = !model->useStringToConstruct
                       ? model->construct(args)
                       : model->construct(node.children[0].symbol))
      return std::pair{*ptr, model};
    else
      return Error<>("Cannot construct type: " + node.symbol);
  };

  return build(build, node);
}

auto split(std::string_view text) {
  std::vector<std::string> parts;
  bool first = true;

  for (size_t i = 0; i < size(text); ++i) {
    bool nt = true;

    if (text[i] == '(')
      parts.push_back("(");
    else if (text[i] == ')')
      parts.push_back(")");
    else if (text[i] == '?')
      parts.push_back("?");
    else if (text[i] == '*')
      parts.push_back("*");
    else if (text[i] == '+')
      parts.push_back("+");
    else if (text[i] == '&')
      parts.push_back("&");
    else if (text[i] == ':')
      parts.push_back(":");
    else if (text[i] == '|')
      parts.push_back("|");
    else if (text[i] == '\\')
      parts.push_back("\\");
    else if (text[i] == '-' && i + 1 < size(text) && text[i + 1] == '>')
      (parts.push_back("->"), ++i);
    else if (text[i] == '[')
      parts.push_back("[");
    else if (text[i] == ']') parts.push_back("]");
    else if (text[i] == '{') parts.push_back("{");
    else if (text[i] == '}') parts.push_back("}");
    else if (text[i] == ',')
      parts.push_back(",");
    else if (text[i] == ' ')
      ;
    else {
      nt = false;
      if (first) parts.push_back("");
      parts.back().push_back(text[i]);
      first = false;
    }
    if (nt) first = true;
  }

  // for(auto p: parts) std::cout << p << " ";
  // std::cout << "\n";

  return parts;
}

auto parseSpec(std::string text) -> Specification {
  Specification spec;

  // std::set<std::string> templateRules;

  forEachLine(text, [&](auto line) {
    if (line[0] == '#') return;
    auto parts = split(line);
    if (size(parts) < 7) return;

    auto p0 = find(begin(parts), end(parts), "[");
    auto p1 = find(begin(parts), end(parts), "]");

    if (p0 != end(parts)) {
      auto name = *(p0 - 1);
      // templateRules.insert(name);

      std::map<std::string, std::string> map;
      std::pair<decltype(map)::iterator, bool> it;
      bool lhs = true;
      for (auto p = p0 + 1; p != p1; ++p) {
        if (lhs) {
          it = map.insert(std::pair{*p, ""});
          lhs = false;
        } else if (*p == "," && *(p - 1) != "\\")
          lhs = true;
        else if (*p != "->" && *p != "\\")
          it.first->second += *p + " ";
      }

      forEachLine(text, [&](auto line2) {
        if (line2.substr(0, line2.find(' ')) == name) {
          for (auto pair : map)
            line2 = std::regex_replace(
                line2, std::regex(pair.first),
                pair.second.substr(0, size(pair.second) - 1));
          line2.erase(0, std::min(line2.find("=="), line2.find(">=")) + 2);

          text += "\n";
          for (auto p = begin(parts); p != p0 - 1; ++p) text += *p + " ";
          text += line2;
          for (auto p = p1 + 1; p != end(parts); ++p) text += " " + *p;
        }
      });
    }
  });

  // std::cout << text << "\n";

  forEachLine(text, [&](auto line) {
    if (line[0] == '#') return;
    auto parts = split(line);

    auto p0 = find(begin(parts), end(parts), "{");
    auto p1 = find(begin(parts), end(parts), "}");

    if (size(parts) < 3) {
      std::cout << "invalid line: " << line << '\n';
      return;
    }

    if (find(begin(parts), end(parts), "[") != end(parts)) return;
    // if (templateRules.find(parts[0]) != end(templateRules)) return;

    spec.addSymbol(parts[0]);

    if (p0 != p1)
      for (auto p = p0 + 1; p != p1; ++p) spec.addAttributes(*p);

    auto it = std::min(find(begin(parts), end(parts), ">="),
                       find(begin(parts), end(parts), "=="));
    if (it == end(parts)) {
      std::cout << "Invalid line: " << line << "\n";
      return;
    }
    auto start = it - begin(parts);

    if (parts[start] == "==") spec.setIntermediate();

    bool isAttrib = false;

    for (size_t i = start + 1; i < size(parts); ++i) {
      if (parts[i] == "\\")
        spec >= parts[++i];
      else if (parts[i] == "|")
        spec.addAlternative();
      else if (parts[i] == "(")
        spec.addLeftParenthesis();
      else if (parts[i] == ")")
        spec.addRightParenthesis();
      else if(parts[i] == "{") isAttrib = true;
      else if(parts[i] == "}") isAttrib = false;
      else if (parts[i] == "?")
        spec.activeRule().expr.back().optional = true;
      else if (parts[i] == "*")
        spec.activeRule().expr.back().arbitrary = true;
      else if (parts[i] == "+")
        spec.activeRule().expr.back().oneOrMore = true;
      else if (parts[i] == "&")
        spec.activeRule().expr.back().deref = true;
      else if(isAttrib) 
        spec.activeRule().expr.back().attribs.push_back(parts[i]);
      else
        spec.addExpr(parts[i]);
    }
  });

  return spec;
}

}  // namespace tiny_bnf
