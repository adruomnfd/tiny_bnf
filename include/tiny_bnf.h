#ifndef TINY_BNF_H
#define TINY_BNF_H

#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
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

struct Rule {
  std::string symbol;
  std::vector<std::string> expr;
};
  
bool operator<(const Rule &l, const Rule &r) {
  return l.symbol == r.symbol ? l.expr < r.expr : l.symbol < r.symbol;
}

struct Specification {
  Specification &operator[](std::string symbol) {
    rules.push_back(Rule{std::move(symbol), {}});
    return *this;
  }
  Specification &operator<=(std::string symbol){
    rules.back().expr.push_back(std::move(symbol));
    return *this;
  }
  Specification &operator,(std::string symbol) {
    rules.back().expr.push_back(std::move(symbol));
    return *this;
  }
  Specification &operator|(std::string symbol) {
    cloneLast();
    rules.back().expr.push_back(std::move(symbol));
    return *this;
  }
  
  auto begin() const {
    return std::begin(rules);
  }
  auto end() const {
    return std::end(rules);
  }
  
  void cloneLast(){
    rules.push_back(Rule{rules.back().symbol, {}});
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
      ts[expr] = true;

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

Expected<Tokens> tokenize(const Terminals &terminals, std::string_view input) {
  Tokens tokens;
  const auto &map = terminals.expr_to_sym;

  auto n = accumulateN(size_t{1}, std::size(input), size_t{0}, [&](auto a, auto b) {
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
    return Error<>("Unused string: " + (std::string)input.substr(n));
}

Expected<Node> parseEarley(const Specification &spec, Tokens tokens) {
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
    return state.rule.expr[state.p] == c;
  };

  for (size_t k = 0; k <= size(tokens); ++k) {
    // scanning
    if (k != 0) {
      for (auto s : stateSets[k - 1])
        if (match(s, tokens[k - 1])) {
          s.p += 1;
          s.node.children.push_back(Node{tokens[k - 1], {}});
          stateSets[k].push_back(std::move(s));
        }
    }

    // completion
    for (size_t i = 0; i < size(stateSets[k]); ++i) {
      const auto s = stateSets[k][i];
      if (isComplete(s)) {
        for (auto ss : stateSets[s.i])
          if (match(ss, s.rule.symbol)) {
            ss.p += 1;
            ss.node.children.push_back(s.node);
            stateSets[k].push_back(std::move(ss));
          }
      }
    }

    // prediction
    std::set<Rule> record;
    for (size_t i = 0; i < size(stateSets[k]); ++i) {
      const auto s = stateSets[k][i];
      if (!isComplete(s)) {
        for (auto rule : spec.rules) {
          if (rule.symbol == s.rule.expr[s.p]) {
            if (record.find(rule) == record.end()) {
              record.insert(rule);
              stateSets[k].push_back(State{k, 0, rule, Node{rule.symbol, {}}});
            }
          }
        }
      }
    }
  }

  for (auto s : stateSets.back())
    if (s.node.symbol == spec.rules.front().symbol)
      return s.node;

  return Error<>("Failed to parse");
}

enum ParserType { Earley };

Expected<Node> parse(const Specification &spec, Tokens tokens, ParserType parserType = ParserType::Earley) {
  switch (parserType) {
    case ParserType::Earley: return parseEarley(spec, std::move(tokens));
    default: return Error<>("Invalid parser type");
  }
}

template <typename... Ts>
struct Ctor {};

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
    virtual ~Concept() = default;
    virtual std::optional<AnnotatedPtr> construct(std::vector<AnnotatedPtr> args) = 0;
    virtual void destruct(void *obj) const = 0;
  };

  template <typename T, typename... Ctors>
  struct Model : Concept {
    std::optional<AnnotatedPtr> construct(std::vector<AnnotatedPtr> args) override {
      if constexpr (sizeof...(Ctors) == 0)
        return AnnotatedPtr{new T(), typeid(T).hash_code()};
      else
        return match(args, Ctors{}...);
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
    models[name] = std::make_unique<Model<T, Ctors...>>();
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
    std::vector<AnnotatedPtr> args;
    std::vector<Generator::Concept *> models;

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

    if (auto model = generator.findModel(node.symbol)) {
      if (auto ptr = (*model)->construct(args))
        return std::pair{*ptr, *model};
      else
        return Error<>("Cannot construct type: " + node.symbol);
    } else {
      return Error<>("Cannot find symbol: " + node.symbol);
    }
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

}  // namespace tiny_bnf

#endif  // TINY_BNF_H
