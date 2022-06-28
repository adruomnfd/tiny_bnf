#ifndef TINY_BNF_H
#define TINY_BNF_H

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace tiny_bnf {

template <typename F>
struct ScopeGuard {
  ScopeGuard(F f) : f(std::move(f)) {
  }
  ~ScopeGuard() {
    f();
  }
  F f;
};

template <typename T = std::string>
struct Error {
  Error() = default;
  Error(T value) : value(std::move(value)) {
  }

  T value;
};

template <typename T, typename ErrorRepr = std::string>
struct Expected {
  Expected(T value) : value(std::move(value)) {
  }
  Expected(Error<ErrorRepr> error) : value(std::move(error)) {
  }

  explicit operator bool() const {
    return value.index() == 0;
  }

  T &operator*() {
    return std::get<0>(value);
  }
  const T &operator*() const {
    return std::get<0>(value);
  }

  auto operator->() {
    return &std::get<0>(value);
  }
  auto operator->() const {
    return &std::get<0>(value);
  }

  const auto &error() const {
    return std::get<1>(value).value;
  }

 private:
  std::variant<T, Error<ErrorRepr>> value;
};

namespace detail {

struct Or {};

struct Optional {
  std::string symbol;
};

struct Arbitrary {
  std::string symbol;
};

}  // namespace detail

constexpr detail::Or OR;

struct Expr {
  Expr() = default;
  Expr(std::string symbol) : symbol(symbol), optional(false) {
  }
  Expr(detail::Optional opt) : symbol(opt.symbol), optional(true) {
  }

  Expr(detail::Arbitrary arb) : symbol(arb.symbol), arbitrary(true) {
  }

  std::string symbol;
  bool optional = false;
  bool arbitrary = false;
};

struct Rule {
  std::string symbol;
  std::vector<Expr> expr;
  bool intermediate = false;
};

bool operator==(const Expr &l, const Expr &r) {
  return l.symbol < r.symbol;
}
bool operator==(const Rule &l, const Rule &r) {
  return l.symbol == r.symbol && l.expr == r.expr;
}

auto opt(std::string symbol) {
  return detail::Optional{symbol};
}
auto arb(std::string symbol) {
  return detail::Arbitrary{symbol};
}

struct Specification {
  Specification &operator[](std::string symbol) {
    rules.push_back(Rule{std::move(symbol), {}});
    return *this;
  }

  Specification &operator==(std::string symbol) {
    rules.back().intermediate = true;
    exprAdd(symbol);
    return *this;
  }
  Specification &operator==(detail::Optional symbol) {
    rules.back().intermediate = true;
    exprAdd(symbol);
    return *this;
  }
  Specification &operator==(detail::Arbitrary symbol) {
    rules.back().intermediate = true;
    exprAdd(symbol);
    return *this;
  }

  Specification &operator>=(std::string symbol) {
    exprAdd(symbol);
    return *this;
  }
  Specification &operator,(std::string symbol) {
    exprAdd(symbol);
    return *this;
  }
  Specification &operator|(std::string symbol) {
    cloneLast();
    exprAdd(symbol);
    return *this;
  }

  Specification &operator>=(detail::Optional symbol) {
    exprAdd(symbol);
    return *this;
  }
  Specification &operator,(detail::Optional symbol) {
    exprAdd(symbol);
    return *this;
  }
  Specification &operator|(detail::Optional symbol) {
    cloneLast();
    exprAdd(symbol);
    return *this;
  }

  Specification &operator>=(detail::Arbitrary symbol) {
    exprAdd(symbol);
    return *this;
  }
  Specification &operator,(detail::Arbitrary symbol) {
    exprAdd(symbol);
    return *this;
  }
  Specification &operator|(detail::Arbitrary symbol) {
    cloneLast();
    exprAdd(symbol);
    return *this;
  }

  template <typename T>
  void exprAdd(T symbol) {
    rules.back().expr.push_back(Expr(std::move(symbol)));
  }

  Specification &operator,(detail::Or) {
    cloneLast();
    return *this;
  }

  void setIntermediate() {
    rules.back().intermediate = true;
  }

  auto begin() const {
    return std::begin(rules);
  }
  auto end() const {
    return std::end(rules);
  }

  void cloneLast() {
    rules.push_back(Rule{rules.back().symbol, {}, rules.back().intermediate});
  }

  std::vector<Rule> rules;
};

struct Terminals {
  std::string &operator[](std::string expr) {
    return expr_to_sym[expr] = expr;
  }

  std::map<std::string, std::string, std::less<>> expr_to_sym;
};

Terminals autoTerminals(const Specification &spec) {
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

struct Node {
  std::string symbol;
  std::vector<Node> children;
};
using Tokens = std::vector<std::string>;

template <typename It, typename T, typename U, typename F>
auto accumulateN(It it, T n, U init, F f) {
  for (T i = 0; i < n; ++i)
    init = f(init, it++);
  return init;
}

inline bool isWordConstituent(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-';
}

Expected<Tokens> tokenize(const Terminals &terminals, std::string_view input, bool delimit = false) {
  Tokens tokens;
  const auto &map = terminals.expr_to_sym;

  auto n = accumulateN(size_t{1}, std::size(input), size_t{0}, [&](auto a, auto b) {
    bool cond = !delimit || b == size(input) || !isWordConstituent(input[b]) || !isWordConstituent(input[a]);
    if (auto it = map.find(input.substr(a, b - a)); cond && it != std::end(map)) {
      if (it->second != "")
        tokens.push_back(it->second);
      return b;
    }
    return a;
  });

  if (n >= std::size(input))
    return tokens;
  else
    return Error<>("Unused string: " + (std::string)input.substr(n));
}

Expected<std::vector<Node>> parseEarley(const Specification &spec, Tokens tokens) {
  struct State {
    size_t i = 0;
    size_t p = 0;
    Rule rule;
    Node node;
  };

  auto stateSets = std::vector<std::vector<State>>(size(tokens) + 1);
  stateSets[0].push_back(State{0, 0, spec.rules.front(), Node{spec.rules.front().symbol, {}}});

  auto isComplete = [&](const auto &state) { return state.p == size(state.rule.expr); };
  auto match = [&](const auto &state, const auto &c) {
    if (isComplete(state))
      return false;
    return state.rule.expr[state.p].symbol == c;
  };

  for (size_t k = 0; k <= size(tokens); ++k) {
    std::set<size_t> predRules;

    for (size_t i = 0; i < size(stateSets[k]); ++i) {
      auto s = stateSets[k][i];
      if (!isComplete(s)) {
        auto next = s.rule.expr[s.p];

        // prediction
        size_t ct = 0, fc = 0;
        for (auto &rule : spec) {
          if (rule.symbol == next.symbol && predRules.find(ct) == end(predRules)) {
            stateSets[k].push_back(State{k, 0, rule, {rule.symbol, {}}});
            predRules.insert(ct);
            ++fc;
          }
          ++ct;
        }

        // scanning
        if (fc == 0 && tokens[k] == next.symbol) {
          auto s2 = s;
          s2.p += 1;
          s2.node.children.push_back(Node{next.symbol, {}});
          stateSets[k + 1].push_back(s2);
        }

        // TODO
        if (next.optional || next.arbitrary) {
          auto s2 = s;
          s2.p += 1;
          stateSets[k].push_back(s2);
        }

      } else {
        // completion
        auto sz = size(stateSets[s.i]);
        for (size_t j = 0; j < sz; ++j)
          if (match(stateSets[s.i][j], s.rule.symbol)) {
            auto sc = stateSets[s.i][j];
            if (!sc.rule.expr[sc.p].arbitrary)
              sc.p += 1;

            if (s.rule.intermediate) {
              for (auto n : s.node.children)
                sc.node.children.push_back(n);
            } else {
              sc.node.children.push_back(s.node);
            }

            stateSets[k].push_back(sc);
          }
      }
    }
  }

  int npossibilities = 0;
  for (auto s : stateSets.back())
    if (s.node.symbol == spec.rules.front().symbol)
      if (s.p == size(s.rule.expr))
        ++npossibilities;

  if (npossibilities == 0)
    return Error<>("Unable to parse input");

  std::vector<Node> nodes;

  for (auto s : stateSets.back())
    if (s.node.symbol == spec.rules.front().symbol)
      if (s.p == size(s.rule.expr))
        nodes.push_back(s.node);

  return nodes;
}

enum ParserType { Earley };

Expected<std::vector<Node>> parse(const Specification &spec, Tokens tokens, ParserType parserType = ParserType::Earley) {
  switch (parserType) {
    case ParserType::Earley: return parseEarley(spec, std::move(tokens));
    default: return Error<>("Invalid parser type");
  }
}

template <typename... Ts>
struct Ctor {};

struct UseString {};

template <typename... Ts>
struct CheckUseString : std::false_type {};
template <>
struct CheckUseString<UseString> : std::true_type {};

template <int I, typename T, typename... Ts>
struct NthType : NthType<I - 1, Ts...> {};
template <typename T, typename... Ts>
struct NthType<0, T, Ts...> {
  using type = T;
};

struct AnnotatedPtr {
  void *ptr = nullptr;
  size_t id = 0;
};

struct Generator {
  struct Concept {
    Concept(bool useString) : useStringToConstruct(useString) {
    }

    virtual ~Concept() = default;
    virtual std::optional<AnnotatedPtr> construct(std::vector<AnnotatedPtr> args) = 0;
    virtual std::optional<AnnotatedPtr> construct(std::string expr) = 0;
    virtual void destruct(void *obj) const = 0;
    bool useStringToConstruct = false;
  };

  template <typename T, typename... Ctors>
  struct Model : Concept {
    using Concept::Concept;

    std::optional<AnnotatedPtr> construct(std::vector<AnnotatedPtr> args) override {
      if constexpr (sizeof...(Ctors) == 0)
        return AnnotatedPtr{new T(), typeid(T).hash_code()};
      else
        return match(args, Ctors{}...);
    }
    std::optional<AnnotatedPtr> construct(std::string expr) override {
      if constexpr (CheckUseString<Ctors...>::value)
        return new T(expr);
      else
        return std::nullopt;
    }

    void destruct(void *obj) const override {
      delete (T *)obj;
    }

    template <typename... Args, int... I>
    static auto integerSeqHelper(std::vector<AnnotatedPtr> args, std::integer_sequence<int, I...>) {
      return AnnotatedPtr{new T((*(typename NthType<I, Args...>::type *)args[I].ptr)...), typeid(T).hash_code()};
    }

    template <typename... Args, typename... Cs>
    std::optional<AnnotatedPtr> match(std::vector<AnnotatedPtr> args, Ctor<Args...>, Cs... ctors) {
      if (int i = 0; std::size(args) == sizeof...(Args) && ((args[i++].id == typeid(Args).hash_code()) && ...))
        return integerSeqHelper<Args...>(args, std::make_integer_sequence<int, sizeof...(Args)>{});
      if constexpr (sizeof...(Cs) == 0)
        return std::nullopt;
      else
        return match(args, ctors...);
    }
  };

  template <typename T, typename... Ctors>
  void bind(std::string name, Ctors...) {
    models[name] = std::make_unique<Model<T, Ctors...>>(false);
  }

  template <typename T>
  void bind(std::string name, UseString) {
    models[name] = std::make_unique<Model<T>>(true);
  }

  std::optional<Concept *> findModel(std::string symbol) const {
    if (auto it = models.find(symbol); it != std::end(models))
      return it->second.get();
    else
      return std::nullopt;
  }

 private:
  std::map<std::string, std::unique_ptr<Concept>> models;
};

template <typename T>
Expected<T> generate(const Generator &generator, const Node &node) {
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

  if (auto ret = build(build, node)) {
    auto [arg, model] = *ret;
    T object = *(T *)arg.ptr;
    model->destruct(arg.ptr);
    return object;
  } else {
    return Error<>(ret.error());
  }
}

// Utilities
std::string readFile(std::string filename) {
  std::ifstream file(filename);
  std::stringstream ss;
  if (!file.is_open())
    std::cout << "cannot open: " << filename << '\n';
  ss << file.rdbuf();
  return ss.str();
}

template <typename F>
void forEachLine(std::string lines, F f) {
  std::istringstream ss(lines);
  std::string line;
  while (std::getline(ss, line)) {
    if (size(line))
      f(line);
  }
}

void importSpec(Specification &spec, std::string sym, std::string lines) {
  forEachLine(lines, [&](auto line) { spec[sym] >= line; });
}

auto parseSpec(std::string filename) {
  auto dir = filename.substr(0, filename.find_last_of('/')) + '/';

  Specification spec;
  Terminals terminals;
  forEachLine(readFile(filename), [&](std::string line) {
    if (line == "autoTerminals") {
      terminals = autoTerminals(spec);
    } else if (line == "ignoreSpace") {
      terminals[" "] = "";
    } else if (line.substr(0, 6) == "import") {
      std::vector<std::string> parts;
      std::stringstream ss(line);
      std::string part;
      while (ss >> part)
        parts.push_back(part);
      if (size(parts) == 3)
        importSpec(spec, parts[1], readFile(dir + parts[2]));
      else if (size(parts) == 4) {
        importSpec(spec, parts[1], readFile(dir + parts[2]));
        spec.setIntermediate();
      } else
        std::cout << "import error\n";
    } else {
      std::vector<std::string> parts;
      std::stringstream ss(line);
      std::string part;
      while (ss >> part)
        parts.push_back(part);
      if (size(parts) < 3)
        std::cout << "rule error\n";
      spec[parts[0]];
      if (parts[1] == ">=" || parts[1] == "==") {
        for (size_t i = 2; i < size(parts); i++) {
          if (parts[i].size() > 1 && parts[i].back() == '*')
            spec >= arb(parts[i].substr(0, size(parts[i]) - 1));
          else if (parts[i].size() > 1 && parts[i].back() == '?')
            spec >= opt(parts[i].substr(0, size(parts[i]) - 1));
          else if (parts[i] == "|")
            spec.cloneLast();
          else
            spec >= parts[i];
        }
        if (parts[1] == "==")
          spec.setIntermediate();
      } else {
        std::cout << "part[1] error\n";
      }
    }
  });

  return std::make_tuple(spec, terminals);
}

}  // namespace tiny_bnf

#endif  // TINY_BNF_H
