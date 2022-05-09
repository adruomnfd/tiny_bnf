#include <iostream>

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

struct Digit {
  Digit(Zero) : n(0) {
  }
  Digit(One) : n(1) {
  }
  Digit(Two) : n(2) {
  }
  Digit(Three) : n(3) {
  }
  Digit(Four) : n(4) {
  }
  Digit(Five) : n(5) {
  }
  Digit(Six) : n(6) {
  }
  Digit(Seven) : n(7) {
  }
  Digit(Eight) : n(8) {
  }
  Digit(Nine) : n(9) {
  }

  int eval() const {
    return n;
  }

  int n = 0;
};

struct Number {
  Number(Number n, Digit d) : n(n), d(d) {
  }
  Number(Digit d) : d(d) {
  }

  int eval() const {
    return n ? n->eval() * 10 + d.eval() : d.eval();
  }

  Indirect<Number> n;
  Digit d;
};

struct LeftParenthesis {};
struct RightParenthesis {};

struct Add {};
struct Subtract {};
struct Multiply {};
struct Divide {};

struct Factor {
  Factor(LeftParenthesis, struct Term, RightParenthesis);
  Factor(Number);

  int eval() const;

  std::optional<Number> n;
  Indirect<struct Term> term;
};

struct Term {
  Term(Term term, Multiply, Factor factor) : term(term), op(Op::Mul), factor(factor) {
  }
  Term(Term term, Divide, Factor factor) : term(term), op(Op::Div), factor(factor) {
  }
  Term(Term term, Add, Term term2) : term(term), op(Op::Add), term2(term2) {
  }
  Term(Term term, Subtract, Term term2) : term(term), op(Op::Sub), term2(term2) {
  }
  Term(Factor factor) : op(Op::Factor), factor(factor) {
  }

  int eval() const {
    switch (op) {
      case Op::Add: return term->eval() + term2->eval();
      case Op::Sub: return term->eval() - term2->eval();
      case Op::Mul: return term->eval() * factor->eval();
      case Op::Div: return term->eval() / factor->eval();
      case Op::Factor: return factor->eval();
    }
  }

  Indirect<Term> term;
  enum class Op { Add, Sub, Mul, Div, Factor } op;
  Indirect<Term> term2;
  std::optional<Factor> factor;
};

Factor::Factor(LeftParenthesis, Term t, RightParenthesis) : term(t) {
}
Factor::Factor(Number n) : n(n) {
}
int Factor::eval() const {
  return term ? term->eval() : n->eval();
}

struct Expr {
  Expr(Term term) : term(term) {
  }

  int eval() const {
    return term.eval();
  }

  Term term;
};

#define CHECK_ANSWER(x)                                                                            \
  if (auto val = eval(#x)) {                                                                       \
    if (*val == (x))                                                                               \
      std::cout << #x << " = " << *val << '\n';                                                    \
    else                                                                                           \
      std::cout << "Answer to [" #x "] is not correct: " << *val << ", should be " << (x) << '\n'; \
  } else {                                                                                         \
    std::cout << "When processing [" #x "]: " << val.error() << '\n';                                \
    abort();                                                                                       \
  }

int main() {
  //
  tiny_bnf::Specification spec;
  tiny_bnf::alternatives(spec, "digit", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9");
  spec["number"] += "number", "digit";
  spec["number"] += "digit";

  spec["factor"] += "(", "term", ")";
  spec["factor"] += "number";

  spec["term"] += "term", "*", "factor";
  spec["term"] += "term", "/", "factor";

  spec["term"] += "term", "+", "term";
  spec["term"] += "term", "-", "term";
  spec["term"] += "factor";

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
  gen.bind<Factor>("factor", Ctor<LeftParenthesis, Term, RightParenthesis>{}, Ctor<Number>{});
  gen.bind<Term>("term", Ctor<Term, Multiply, Factor>{}, Ctor<Term, Divide, Factor>{}, Ctor<Term, Add, Term>{},
                 Ctor<Term, Subtract, Term>{}, Ctor<Factor>{});
  gen.bind<Expr>("expr", Ctor<Term>{});

  //
  auto eval = [&](auto input) -> tiny_bnf::Expected<int> {
    auto tokens = tiny_bnf::tokenize(terminals, input);
    if (!tokens)
      return tiny_bnf::Error<>("Failed to tokenize: " + tokens.error());

    auto tree = tiny_bnf::parse(spec, *tokens);
    if (!tree)
      return tiny_bnf::Error<>("Failed to parse: " + tree.error());

    auto expr = tiny_bnf::generate<Expr>(gen, *tree);
    if (!expr)
      return tiny_bnf::Error<>("Failed to generate: " + expr.error());

    return expr->eval();
  };

  CHECK_ANSWER(7 * 2);
  CHECK_ANSWER(1 * 2 + 3);
  CHECK_ANSWER(3 + 1 * 2);
  CHECK_ANSWER(6 * 2);
  CHECK_ANSWER(6 * (3 + 2));
  CHECK_ANSWER((3 + 2) * 6);
  CHECK_ANSWER((3 + 2) * (1 + 2));
  CHECK_ANSWER((3 + 2) * 7 + 5);
  CHECK_ANSWER(5 * (3 + 2) * 7);
  CHECK_ANSWER(5 * (3 + 2 * 12) * 7);
  CHECK_ANSWER((3 + (1 / 10) + 2));
}