#ifndef TINY_BNF_H
#define TINY_BNF_H

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
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

  auto operator*() -> T & {
    return std::get<0>(value);
  }
  auto operator*() const -> const T & {
    return std::get<0>(value);
  }

  auto operator->() {
    return &std::get<0>(value);
  }
  auto operator->() const {
    return &std::get<0>(value);
  }

  auto error() const {
    return std::get<1>(value).value;
  }

 private:
  std::variant<T, Error<ErrorRepr>> value;
};

namespace detail {

struct Or {};

struct Leftparenthesis {};
struct RightParenthesis {};

struct Optional {
  std::string symbol;
};

struct Arbitrary {
  std::string symbol;
};

struct OneOrMore {
  std::string symbol;
};

template <typename T>
struct Deref {
  T ref;
};

}  // namespace detail

constexpr detail::Or OR;
constexpr detail::Leftparenthesis L;
constexpr detail::RightParenthesis R;

struct Expr {
  Expr(std::string symbol) : symbol(symbol) {
  }
  Expr(detail::Optional opt) : symbol(opt.symbol), optional(true) {
  }
  Expr(detail::Arbitrary arb) : symbol(arb.symbol), arbitrary(true) {
  }
  Expr(detail::OneOrMore oom) : symbol(oom.symbol), oneOrMore(true) {
  }
  template <typename T>
  Expr(detail::Deref<T> deref) : Expr(deref.ref) {
    deref = true;
  }

  std::string symbol;
  bool optional = false;
  bool arbitrary = false;
  bool oneOrMore = false;
  bool deref = false;
};

inline bool operator==(const Expr &a, const Expr &b) {
  return a.symbol == b.symbol && a.optional == b.optional && a.arbitrary == b.arbitrary && a.oneOrMore == b.oneOrMore;
}

struct Rule {
  std::string symbol;
  std::vector<Expr> expr;
  size_t idx = 0;
  bool intermediate = false;
  bool alias = false;
};

inline bool operator==(const Rule &a, const Rule &b) {
  return a.symbol == b.symbol && a.expr == b.expr;
}
inline bool operator!=(const Rule &a, const Rule &b) {
  return !(a == b);
}

inline auto opt(std::string symbol) {
  return detail::Optional{symbol};
}
inline auto arb(std::string symbol) {
  return detail::Arbitrary{symbol};
}
inline auto oom(std::string symbol) {
  return detail::OneOrMore{symbol};
}
template <typename T>
inline auto deref(T x) {
  return detail::Deref{x};
}

struct Specification {
  auto operator[](std::string symbol) -> Specification & {
    return addSymbol(symbol);
  }

  auto operator>=(std::string symbol) -> Specification & {
    return addExpr(symbol);
  }
  auto operator,(std::string symbol) -> Specification & {
    return addExpr(symbol);
  }
  auto operator|(std::string symbol) -> Specification & {
    addAlternative();
    return addExpr(symbol);
  }
  auto operator==(std::string symbol) -> Specification & {
    setIntermediate();
    return addExpr(symbol);
  }

  auto operator>=(detail::Optional symbol) -> Specification & {
    return addExpr(symbol);
  }
  auto operator,(detail::Optional symbol) -> Specification & {
    return addExpr(symbol);
  }
  auto operator|(detail::Optional symbol) -> Specification & {
    addAlternative();
    return addExpr(symbol);
  }
  auto operator==(detail::Optional symbol) -> Specification & {
    setIntermediate();
    return addExpr(symbol);
  }

  auto operator>=(detail::Arbitrary symbol) -> Specification & {
    return addExpr(symbol);
  }
  auto operator,(detail::Arbitrary symbol) -> Specification & {
    return addExpr(symbol);
  }
  auto operator|(detail::Arbitrary symbol) -> Specification & {
    addAlternative();
    return addExpr(symbol);
  }
  auto operator==(detail::Arbitrary symbol) -> Specification & {
    setIntermediate();
    return addExpr(symbol);
  }

  auto operator>=(detail::OneOrMore symbol) -> Specification & {
    return addExpr(symbol);
  }
  auto operator,(detail::OneOrMore symbol) -> Specification & {
    return addExpr(symbol);
  }
  auto operator|(detail::OneOrMore symbol) -> Specification & {
    addAlternative();
    return addExpr(symbol);
  }
  auto operator==(detail::OneOrMore symbol) -> Specification & {
    setIntermediate();
    return addExpr(symbol);
  }

  template <typename T>
  auto operator>=(detail::Deref<T> symbol) -> Specification & {
    return addExpr(symbol);
  }
  template <typename T>
  auto operator,(detail::Deref<T> symbol) -> Specification & {
    return addExpr(symbol);
  }
  template <typename T>
  auto operator|(detail::Deref<T> symbol) -> Specification & {
    addAlternative();
    return addExpr(symbol);
  }
  template <typename T>
  auto operator==(detail::Deref<T> symbol) -> Specification & {
    setIntermediate();
    return addExpr(symbol);
  }

  auto operator,(detail::Or) -> Specification & {
    return addAlternative();
  }

  auto addSymbol(std::string symbol, bool setAsActive = true) -> Specification & {
    if (setAsActive)
      p = size(rules);
    rules.push_back(Rule{std::move(symbol), {}, size(rules)});
    return *this;
  }

  template <typename T>
  auto addExpr(T symbol) -> Specification & {
    activeRule().expr.push_back(Expr(std::move(symbol)));
    return *this;
  }

  auto addAlternative() -> Specification & {
    rules.push_back(activeRule());
    p = size(rules) - 1;
    activeRule().expr.clear();
    activeRule().idx += 1;
    return *this;
  }

  auto addLeftParenthesis() -> Specification & {
    ps.push_back(p);
    auto scopeName = ".par#" + std::to_string(nParentheses++);
    addSymbol(scopeName);
    activeRule().intermediate = true;
    return *this;
  }
  auto addRightParenthesis() -> Specification & {
    if (size(ps) == 0)
      std::cout << "unmatched right parenthesis\n";
    rules[ps.back()].expr.push_back(activeRule().symbol);
    p = ps.back();
    ps.pop_back();
    return *this;
  }

  void setIntermediate() {
    activeRule().intermediate = true;
  }

  void setAlias() {
    activeRule().alias = true;
  }

  auto activeRule() -> Rule & {
    return rules[p];
  }

  auto begin() const {
    return std::begin(rules);
  }
  auto end() const {
    return std::end(rules);
  }

  std::vector<Rule> rules;
  size_t p = 0;
  std::vector<size_t> ps;
  size_t nParentheses = 0;
};

struct Terminals {
  auto operator[](std::string expr) -> auto & {
    return expr2Sym[expr] = expr;
  }

  std::map<std::string, std::string, std::less<>> expr2Sym;
};

Terminals autoTerminals(const Specification &spec);

using Tokens = std::vector<std::string>;

struct Node {
  std::string symbol;
  std::vector<Node> children;
};

template <typename F>
void traverse(Node &node, F f, int depth = 0, int w = 0) {
  f(node, depth, w);
  for (auto &n : node.children)
    traverse(n, f, depth + 1, w++);
}

inline bool operator==(const Node &a, const Node &b) {
  return a.symbol == b.symbol && a.children == b.children;
}

Expected<Tokens> tokenize(const Terminals &terminals, std::string_view input, bool delimit = false);

enum ParserType { Earley };

Expected<std::vector<Node>> parse(const Specification &spec, Tokens tokens, ParserType parserType = ParserType::Earley);

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
    virtual auto construct(std::vector<AnnotatedPtr> args) -> std::optional<AnnotatedPtr> = 0;
    virtual auto construct(std::string expr) -> std::optional<AnnotatedPtr> = 0;
    virtual void destruct(void *obj) const = 0;

    bool useStringToConstruct = false;
  };

  template <typename T, typename... Ctors>
  struct Model : Concept {
    using Concept::Concept;

    auto construct(std::vector<AnnotatedPtr> args) -> std::optional<AnnotatedPtr> override {
      if constexpr (sizeof...(Ctors) == 0)
        return AnnotatedPtr{new T(), typeid(T).hash_code()};
      else
        return match(args, Ctors{}...);
    }

    auto construct(std::string expr) -> std::optional<AnnotatedPtr> override {
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
    auto match(std::vector<AnnotatedPtr> args, Ctor<Args...>, Cs... ctors) -> std::optional<AnnotatedPtr> {
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

  auto findModel(std::string symbol) const -> std::optional<Concept *> {
    if (auto it = models.find(symbol); it != std::end(models))
      return it->second.get();
    else
      return std::nullopt;
  }

 private:
  std::map<std::string, std::unique_ptr<Concept>> models;
};

Expected<std::pair<AnnotatedPtr, Generator::Concept *>> generateImpl(const Generator &generator, const Node &node);

template <typename T>
auto generate(const Generator &generator, const Node &node) -> Expected<T> {
  if (auto ret = generateImpl(generator, node)) {
    auto [arg, model] = *ret;
    T object = *(T *)arg.ptr;
    model->destruct(arg.ptr);
    return object;
  } else {
    return Error<>(ret.error());
  }
}

auto parseSpec(std::string filename) -> Specification;

template <typename F>
void forEachLine(std::string text, F f) {
  std::stringstream ss(text);
  std::string line;
  while (std::getline(ss, line))
    if (size(line) && line.find_first_not_of(' ') != line.npos)
      f(line);
}

}  // namespace tiny_bnf

#endif  // TINY_BNF_H
