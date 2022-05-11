#include <iostream>
#include <variant>

#include "tiny_bnf.h"

template <typename T>
struct Indirect {
  Indirect() = default;
  Indirect(T x) : x(std::make_shared<T>(x)) {
  }

  auto& operator*() const {
    return *x;
  }
  auto operator->() const {
    return x;
  }
  explicit operator bool() const {
    return (bool)x;
  }

  std::shared_ptr<T> x;
};

template <typename... Ts>
struct Overloaded : Ts... {
  Overloaded(Ts... xs) : Ts(xs)... {
  }
  using Ts::operator()...;
};

struct Zero {};
struct One {};
struct Two {};
struct Three {};
struct Four {};
struct Five {};
struct Six {};
struct Seven {};
struct Eight {};
struct Nine {};

using Digit = std::variant<Zero, One, Two, Three, Four, Five, Six, Seven, Eight, Nine>;
int eval(Digit d) {
  return std::visit(
      Overloaded{[](Zero) { return 0; }, [](One) { return 1; }, [](Two) { return 2; }, [](Three) { return 3; },
                 [](Four) { return 4; }, [](Five) { return 5; }, [](Six) { return 6; }, [](Seven) { return 7; },
                 [](Eight) { return 8; }, [](Nine) { return 9; }},
      d);
}

struct Number {
  Number(Digit d, Number n) : d(d), n(n) {
  }
  Number(Digit d) : d(d) {
  }

  int eval() const {
    return n ? n->helper(d) : ::eval(d);
  }
  
  int helper(int d) const {
    return n ? 10 * n->helper(d) + n->helper(this->d) : 10 * d + this->d;
  }

  Digit d;
  Indirect<Number> n;
};

struct LeftParenthesis {};
struct RightParenthesis {};

struct Add {};
struct Subtract {};
struct Multiply {};
struct Divide {};

struct Factor {
  Factor(LeftParenthesis, struct Expr, RightParenthesis);
  Factor(Number);

  int eval() const;

  std::optional<Number> n;
  Indirect<struct Expr> expr;
};

struct Term {
  Term(Factor factor, Multiply, Term term) : op(Op::Mul), factor(factor), term(term) {
  }
  Term(Factor factor, Divide, Term term) : op(Op::Div), factor(factor), term(term) {
  }
  Term(Factor factor) : op(Op::Identity), factor(factor) {
  }

  int eval() const {
    switch (op) {
      case Op::Mul: return factor->eval() * term->eval();
      case Op::Div: return factor->eval() / term->eval();
      default: return factor->eval();
    }
  }

  enum class Op { Mul, Div, Identity } op;
  std::optional<Factor> factor;
  Indirect<Term> term;
};

struct Expr {
  Expr(Term term, Add, Expr expr) : op(Op::Add), term(term), expr(expr) {
  }
  Expr(Term term, Subtract, Expr expr) : op(Op::Sub), term(term), expr(expr) {
  }
  Expr(Term term) : op(Op::Identity), term(term) {
  }

  int eval() const {
    switch (op) {
      case Op::Add: return term->eval() + expr->eval();
      case Op::Sub: return term->eval() - expr->eval();
      default: return term->eval();
    }
  }

  enum class Op { Add, Sub, Identity } op;
  std::optional<Term> term;
  Indirect<Expr> expr;
};

Factor::Factor(LeftParenthesis, Expr expr, RightParenthesis) : expr(expr) {
}
Factor::Factor(Number n) : n(n) {
}
int Factor::eval() const {
  return expr ? expr->eval() : n->eval();
}

#define CHECK_ANSWER(x)                                                                            \
  if (auto val = eval(#x)) {                                                                       \
    if (*val == (x))                                                                               \
      std::cout << #x << " = " << *val << '\n';                                                    \
    else                                                                                           \
      std::cout << "Answer to [" #x "] is not correct: " << *val << ", should be " << (x) << '\n'; \
  } else {                                                                                         \
    std::cout << "When processing [" #x "]: " << val.error() << '\n';                              \
    exit(1);                                                                                       \
  }

int main() {
  //
  tiny_bnf::Specification spec;
  tiny_bnf::alternatives(spec, "digit", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9");
  spec["number"] += "digit", "number";
  spec["number"] += "digit";

  spec["factor"] += "(", "expr", ")";
  spec["factor"] += "number";

  spec["term"] += "factor", "*", "term";
  spec["term"] += "factor", "/", "term";
  spec["term"] += "factor";

  spec["expr"] += "term", "+", "expr";
  spec["expr"] += "term", "-", "expr";
  spec["expr"] += "term";

  //
  tiny_bnf::Terminals terminals = autoTerminals(spec);
  terminals[" "] = "";

  //
  using tiny_bnf::Ctor;
  tiny_bnf::Generator gen;
  gen.bind<Zero>("0");
  gen.bind<One>("1");
  gen.bind<Two>("2");
  gen.bind<Three>("3");
  gen.bind<Four>("4");
  gen.bind<Five>("5");
  gen.bind<Six>("6");
  gen.bind<Seven>("7");
  gen.bind<Eight>("8");
  gen.bind<Nine>("9");
  gen.bind<Digit>("digit", Ctor<Zero>{}, Ctor<One>{}, Ctor<Two>{}, Ctor<Three>{}, Ctor<Four>{}, Ctor<Five>{},
                  Ctor<Six>{}, Ctor<Seven>{}, Ctor<Eight>{}, Ctor<Nine>{});
  gen.bind<Number>("number", Ctor<Number, Digit>{}, Ctor<Digit>{});
  gen.bind<Add>("+");
  gen.bind<Subtract>("-");
  gen.bind<Multiply>("*");
  gen.bind<Divide>("/");
  gen.bind<LeftParenthesis>("(");
  gen.bind<RightParenthesis>(")");
  gen.bind<Factor>("factor", Ctor<LeftParenthesis, Expr, RightParenthesis>{}, Ctor<Number>{});
  gen.bind<Term>("term", Ctor<Factor, Multiply, Term>{}, Ctor<Factor, Divide, Term>{}, Ctor<Factor>{});
  gen.bind<Expr>("expr", Ctor<Term, Add, Expr>{}, Ctor<Term, Subtract, Expr>{}, Ctor<Term>{});

  //
  auto eval = [&](auto input) -> tiny_bnf::Expected<int> {
    auto tokens = tiny_bnf::tokenize(terminals, input);
    if (!tokens)
      return tiny_bnf::Error<>("Failed to tokenize: " + tokens.error());

    auto tree = tiny_bnf::parseTopdown(spec, *tokens);
    if (!tree)
      return tiny_bnf::Error<>("Failed to parse: " + tree.error());

    auto expr = tiny_bnf::generate<Expr>(gen, *tree);
    if (!expr)
      return tiny_bnf::Error<>("Failed to generate: " + expr.error());

    return expr->eval();
  };

  CHECK_ANSWER(7);
  CHECK_ANSWER(7 + 2);
  CHECK_ANSWER(7 * 2);
  CHECK_ANSWER(1 + 2 + 3);
  CHECK_ANSWER((1 + 2));
  CHECK_ANSWER(3 + 1 * 2);
  CHECK_ANSWER(6 * 2);
  CHECK_ANSWER(6 * (3 + 2));
  CHECK_ANSWER(6 * (3 * 2));
  CHECK_ANSWER(3 * 2 * 6);
  CHECK_ANSWER((3 * 2) * 6);
  CHECK_ANSWER((3 + 2) * 6);
  CHECK_ANSWER((3 + 2) * (1 + 2));
  CHECK_ANSWER((3 + 2) * 7 + 5);
  CHECK_ANSWER(5 * (3 + 2) * 7);
  CHECK_ANSWER(5 * (3 + 2 * 12) * 7);
  CHECK_ANSWER((3 + (1 / 10) + 2));
}
