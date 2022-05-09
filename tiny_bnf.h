#ifndef TINY_BNF_H
#define TINY_BNF_H

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Test

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
  Expected(Error<ErrorRepr> error_) : error_(std::move(error_)) {
  }

  explicit operator bool() const {
    return (bool)value;
  }

  T &operator*() {
    return *value;
  }
  const T &operator*() const {
    return *value;
  }

  auto &operator->() {
    return value;
  }
  const auto &operator->() const {
    return value;
  }

  const auto &error() const {
    return error_->value;
  }

 private:
  std::optional<T> value;
  std::optional<Error<ErrorRepr>> error_;
};

struct Specification {
  struct Expression {
    std::vector<std::string> exprs;

    Expression &operator+=(std::string symbol) {
      exprs.push_back(std::move(symbol));
      return *this;
    }
    Expression &operator,(std::string symbol) {
      exprs.push_back(std::move(symbol));
      return *this;
    }

    auto begin() const {
      return std::begin(exprs);
    }
    auto end() const {
      return std::end(exprs);
    }
  };
  struct Rule {
    std::string symbol;
    Expression expression;
  };

  Expression &operator[](std::string symbol) {
    rules.push_back(Rule{std::move(symbol), {}});
    return rules.back().expression;
  }

  auto begin() const {
    return std::begin(rules);
  }
  auto end() const {
    return std::end(rules);
  }

  std::vector<Rule> rules;
};

template <typename... Ts>
void alternatives(Specification &spec, std::string symbol, Ts... exprs) {
  ((spec[symbol] += std::move(exprs)), ...);
}

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
    for (const auto &expr : rule.expression)
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

using Tokens = std::vector<Node>;

Expected<Tokens> tokenize(const Terminals &terminals, std::string_view input) {
  Tokens tokens;
  const auto &map = terminals.expr_to_sym;

  size_t pos = 0;
  while (pos++ < std::size(input)) {
    std::string_view expr = input.substr(0, pos);
    if (auto it = map.find(expr); it != std::end(map)) {
      if (it->second != "")
        tokens.push_back({it->second, {}});
      input = input.substr(pos);
      pos = 0;
    }
  }

  if (std::size(input))
    return Error<>("Unused string: " + (std::string)input);

  return tokens;
}

template <typename ItA, typename ItB, typename Pred>
bool match(ItA a_first, ItA a_last, ItB b_first, ItB b_last, Pred pred) {
  if (a_last - a_first != b_last - b_first)
    return false;

  for (; a_first != a_last; ++a_first, ++b_first)
    if (!pred(*a_first, *b_first))
      return false;

  return true;
}

template <typename Container, typename It, typename T>
It replace(Container &container, It first, It last, T value) {
  auto n = first - std::begin(container);
  container.erase(first, last);
  return container.insert(std::begin(container) + n, std::move(value));
}

Expected<Node> parse(const Specification &spec, Tokens tokens, void (*debug)(std::string) = nullptr) {
  for (auto it = std::begin(spec); it != std::end(spec); ++it) {
    for (size_t n = 1; n <= std::size(tokens); ++n) {
      for (size_t i = 0; i <= std::size(tokens) - n; ++i) {
        auto first = std::begin(tokens) + i;
        if (match(first, first + n, std::begin(it->expression), std::end(it->expression),
                  [](auto token, auto expr) { return token.symbol == expr; })) {
          Node node{it->symbol, {}};
          for (auto token = first; token != first + n; ++token)
            node.children.push_back(*token);
          replace(tokens, first, first + n, node);
          if (debug) {
            for (auto token : tokens)
              debug(token.symbol + ' ');
            debug("\n");
          }
          return parse(spec, std::move(tokens));
        }
      }
    }
  }

  if (std::size(tokens) == 1)
    return tokens[0];
  else {
    std::string error = "Unused tokens: ";
    for (auto &token : tokens)
      error += token.symbol;
    return Error{error};
  }
}

template <typename F>
void traverse(Node node, F f, int depth = 0) {
  f(node.symbol, depth);
  for (auto &child : node.children)
    traverse(child, f, depth + 1);
}

template <typename... Ts>
struct Ctor {};

template <int I, typename T, typename... Ts>
struct NthType : NthType<I - 1, Ts...> {};
template <typename T, typename... Ts>
struct NthType<0, T, Ts...> {
  using type = T;
};

struct Generator {
  struct Concept {
    virtual ~Concept() = default;
    virtual std::optional<std::pair<void *, size_t>> construct(std::vector<void *> args,
                                                               std::vector<size_t> typeids) = 0;
    virtual void destruct(void *obj) const = 0;
  };
  template <typename T, typename... Ctors>
  struct Model : Concept {
    std::optional<std::pair<void *, size_t>> construct(std::vector<void *> args, std::vector<size_t> typeids) override {
      if constexpr (sizeof...(Ctors) == 0)
        return std::pair{new T(), typeid(T).hash_code()};
      else
        return match(args, typeids, Ctors{}...);
    }

    void destruct(void *obj) const override {
      delete (T *)obj;
    }

    template <typename... Args, int... I>
    static auto integerSeqHelper(std::vector<void *> args, std::integer_sequence<int, I...>) {
      return std::pair{new T((*(typename NthType<I, Args...>::type *)args[I])...), typeid(T).hash_code()};
    }

    template <typename... Args, typename... Cs>
    std::optional<std::pair<void *, size_t>> match(std::vector<void *> args, std::vector<size_t> typeids, Ctor<Args...>,
                                                   Cs... ctors) {
      int i = 0;
      if (std::size(args) == sizeof...(Args) && ((typeids[i++] == typeid(Args).hash_code()) && ...)) {
        return integerSeqHelper<Args...>(args, std::make_integer_sequence<int, sizeof...(Args)>{});
      }
      if constexpr (sizeof...(Cs) == 0)
        return std::nullopt;
      else
        return match(args, typeids, ctors...);
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
  using R = Expected<std::pair<std::pair<void *, size_t>, Generator::Concept *>>;
  auto build = [&](auto &build, Node node) -> R {
    std::vector<void *> args;
    std::vector<size_t> typeids;
    std::vector<Generator::Concept *> models;
    for (auto child : node.children) {
      if (auto ret = build(build, child)) {
        auto [ptr, model] = *ret;
        args.push_back(ptr.first);
        typeids.push_back(ptr.second);
        models.push_back(model);
      } else {
        return ret;
      }
    }

    ScopeGuard guard{[&]() {
      for (size_t i = 0; i < std::size(args); ++i)
        models[i]->destruct(args[i]);
    }};

    if (auto model = generator.findModel(node.symbol)) {
      if (auto ptr = (*model)->construct(args, typeids))
        return std::pair{*ptr, *model};
      else
        return Error<>("Cannot construct type: " + node.symbol);
    } else {
      return Error<>("Cannot find symbol: " + node.symbol);
    }
  };

  if (auto ret = build(build, node)) {
    auto [ptr, model] = *ret;
    T object = *(T *)ptr.first;
    model->destruct(ptr.first);
    return object;
  } else {
    return Error<>(ret.error());
  }
}

}  // namespace tiny_bnf

#endif  // TINY_BNF_H