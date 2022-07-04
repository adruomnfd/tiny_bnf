#include <tiny_bnf.h>

#include <algorithm>
#include <iostream>
#include <map>

namespace tiny_bnf {

template <typename It, typename T, typename U, typename F>
auto accumulateN(It it, T n, U init, F f) {
  for (T i = 0; i < n; ++i)
    init = f(init, it++);
  return init;
}

inline auto isWord(char c) -> bool {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-';
}

auto autoTerminals(const Specification &spec) -> Terminals {
  Terminals terminals;
  std::map<std::string, bool> ts;

  for (const auto &rule : spec)
    for (const auto &expr : rule.expr)
      ts[expr.symbol] = true;

  for (const auto &rule : spec)
    ts[rule.symbol] = false;

  for (const auto &terminal : ts)
    if (terminal.second)
      terminals[terminal.first];

  return terminals;
}

auto tokenize(const Terminals &terminals, std::string_view input, bool delimit) -> Expected<Tokens> {
  Tokens tokens;
  const auto &map = terminals.expr2Sym;

  auto n = accumulateN(size_t{1}, std::size(input), size_t{0}, [&](auto a, auto b) {
    if (!delimit || b == size(input) || !isWord(input[b]) || !isWord(input[a]))
      if (auto it = map.find(input.substr(a, b - a)); it != std::end(map)) {
        if (it->second != "")
          tokens.push_back(it->second);
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
  for (auto &r : n.children)
    traverseNode(r, f);
}

struct State {
  size_t i = 0;
  size_t p = 0;
  Rule rule;
  Node node;
  size_t start = 0;
};
using StateSets = std::vector<std::vector<State>>;

auto isComplete(const State &state) {
  return state.p == size(state.rule.expr);
}

auto match(const State &state, const std::string &c) {
  if (isComplete(state))
    return false;
  return state.rule.expr[state.p].symbol == c;
}

auto predict(StateSets &stateSets, size_t k, size_t i, const Expr &next,
             const std::map<std::string, std::vector<Rule>> &rules, std::vector<bool> &addedRules) {
  auto it = rules.find(next.symbol);
  if (it == end(rules))
    return false;
  for (auto &rule : it->second)
    if (!addedRules[rule.idx]) {
      stateSets[k].push_back(State{k, 0, rule, {rule.symbol, {}}, i});
      addedRules[rule.idx] = true;
    }
  return true;
}

auto scan(StateSets &stateSets, size_t k, const Expr &next, const State &s) {
  auto s2 = s;
  if (!s2.rule.expr[s2.p].arbitrary)
    s2.p += 1;
  s2.node.children.push_back(Node{next.symbol, {}});
  stateSets[k + 1].push_back(s2);
}

auto complete(StateSets &stateSets, size_t k, const State &s) {
  auto sz = size(stateSets[s.i]);
  for (size_t j = s.start; j < sz; ++j)
    if (match(stateSets[s.i][j], s.rule.symbol)) {
      auto sc = stateSets[s.i][j];

      if (sc.rule.expr[sc.p].oneOrMore) {
        sc.rule.expr[sc.p].oneOrMore = false;
        sc.rule.expr[sc.p].arbitrary = true;
      }

      if (s.rule.intermediate || sc.rule.alias || sc.rule.expr[sc.p].deref) {
        for (auto n : s.node.children)
          sc.node.children.push_back(n);
      } else {
        sc.node.children.push_back(s.node);
      }

      if (!sc.rule.expr[sc.p].arbitrary)
        sc.p += 1;

      stateSets[k].push_back(sc);
    }
}

auto parseEarley(Specification spec, Tokens tokens) -> Expected<std::vector<Node>> {
  auto stateSets = StateSets(size(tokens) + 1);
  for (size_t i = 0; i != size(spec.rules) && spec.rules[i].symbol == spec.rules[0].symbol; ++i)
    stateSets[0].push_back(State{0, 0, spec.rules[i], Node{spec.rules[i].symbol, {}}});

  std::map<std::string, std::vector<Rule>> rules;
  for (auto &rule : spec)
    rules[rule.symbol].push_back(rule);

  for (size_t k = 0; k <= size(tokens); ++k) {
    std::vector<bool> addedRules(size(spec.rules));

    for (size_t i = 0; i < size(stateSets[k]); ++i) {
      auto s = stateSets[k][i];
      if (0) {
        if (i == 0)
          std::cout << "\n";
        std::cout << s.i << ' ' << s.rule.symbol << " → ";
        for (size_t j = 0; j < s.p; j++)
          std::cout << s.rule.expr[j].symbol << ' ';
        std::cout << u8"• ";
        for (size_t j = s.p; j < size(s.rule.expr); j++)
          std::cout << s.rule.expr[j].symbol << ' ';
        std::cout << '\n';
      }

      if (!isComplete(s)) {
        const auto &next = s.rule.expr[s.p];

        // prediction
        auto isNonTerminal = predict(stateSets, k, i, next, rules, addedRules);

        // scanning
        if (k != size(tokens) && !isNonTerminal && tokens[k] == next.symbol)
          scan(stateSets, k, next, s);

        if (next.optional || next.arbitrary) {
          s.p += 1;
          stateSets[k].push_back(s);
        }

      } else {
        // completion
        complete(stateSets, k, s);
        addedRules[s.rule.idx] = false;
      }
    }
  }

  std::vector<Node> nodes;

  for (auto &s : stateSets.back())
    if (s.i == 0 && s.node.symbol == spec.rules.front().symbol)
      if (isComplete(s) && std::find(begin(nodes), end(nodes), s.node) == end(nodes))
        nodes.push_back(s.node);

  if (size(nodes) == 0)
    return Error<>("Unable to parse input");

  return nodes;
}

auto parse(const Specification &spec, Tokens tokens, ParserType parserType) -> Expected<std::vector<Node>> {
  if (0)
    for (auto r : spec) {
      std::cout << r.symbol << " ::= ";
      for (auto e : r.expr) {
        std::cout << e.symbol;
        if (e.optional)
          std::cout << "?";
        if (e.arbitrary)
          std::cout << "*";
        if (e.oneOrMore)
          std::cout << "+";
        if (e.deref)
          std::cout << "&";
        std::cout << " ";
      }
      std::cout << r.idx << '\n';
    }

  switch (parserType) {
    case ParserType::Earley: return parseEarley(spec, std::move(tokens));
    default: return Error<>("Invalid parser type");
  }
}

auto generateImpl(const Generator &generator, const Node &node)
    -> Expected<std::pair<AnnotatedPtr, Generator::Concept *>> {
  using R = Expected<std::pair<AnnotatedPtr, Generator::Concept *>>;

  auto build = [&](auto &build, Node node) -> R {
    auto modelOpt = generator.findModel(node.symbol);
    if (!modelOpt)
      return Error<>("Cannot find model: " + node.symbol);
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
      return Error<>("Model binded with _UseString_ tag requires one child in corresponding node");

    if (auto ptr = !model->useStringToConstruct ? model->construct(args) : model->construct(node.children[0].symbol))
      return std::pair{*ptr, model};
    else
      return Error<>("Cannot construct type: " + node.symbol);
  };

  return build(build, node);
}

auto splitBySpaceAndParenthesis(std::string_view text) {
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
    else if (text[i] == ' ')
      ;
    else {
      nt = false;
      if (first)
        parts.push_back("");
      parts.back().push_back(text[i]);
      first = false;
    }
    if (nt)
      first = true;
  }

  return parts;
}

auto parseSpec(std::string text) -> Specification {
  Specification spec;

  forEachLine(text, [&](std::string line) {
    if (line[0] == '#')
      return;
    auto parts = splitBySpaceAndParenthesis(line);

    if (size(parts) < 3 || (parts[1] != ">=" && parts[1] != "==")) {
      std::cout << "invalid line: " << line << '\n';
      return;
    }

    spec.addSymbol(parts[0]);
    if (parts[1] == ">=" || parts[1] == "==") {
      if (parts[1] == "==")
        spec.setIntermediate();

      for (size_t i = 2; i < size(parts); ++i) {
        if (parts[i] == "|")
          spec.addAlternative();
        else if (parts[i] == "(")
          spec.addLeftParenthesis();
        else if (parts[i] == ")")
          spec.addRightParenthesis();
        else if (parts[i] == "?")
          spec.activeRule().expr.back().optional = true;
        else if (parts[i] == "*")
          spec.activeRule().expr.back().arbitrary = true;
        else if (parts[i] == "+")
          spec.activeRule().expr.back().oneOrMore = true;
        else if (parts[i] == "&")
          spec.activeRule().expr.back().deref = true;
        else
          spec >= parts[i];
      }
    }
  });

  return spec;
}

}  // namespace tiny_bnf
