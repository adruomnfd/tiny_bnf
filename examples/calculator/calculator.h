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

struct Integer {
  Integer(Digit d) : d(d) {
  }
  Integer(Integer n, Digit d) : n(n), d(d) {
  }

  int eval() const {
    return (n ? n->eval() * 10 : 0) + ::eval(d);
  }

  Indirect<Integer> n;
  Digit d;
};

struct Dot {};

struct FloatingPoint {
  FloatingPoint(Integer n) : n(n) {
  }
  FloatingPoint(Integer n, Dot, Integer f) : n(n), f(f) {
  }

  float eval() const {
    float nf = f ? f->eval() : 0;
    while (nf >= 1.0f)
      nf /= 10;
    return n.eval() + nf;
  }

  Integer n;
  std::optional<Integer> f;
};

using Number = FloatingPoint;

struct LeftParenthesis {};
struct RightParenthesis {};

struct Add {};
struct Subtract {};
struct Multiply {};
struct Divide {};

struct Factor {
  Factor(LeftParenthesis, struct Expr, RightParenthesis);
  Factor(Number);

  float eval() const;

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

  float eval() const {
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

  float eval() const {
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
float Factor::eval() const {
  return expr ? expr->eval() : n->eval();
}

struct Stmt {
  Stmt(Expr expr) : expr(expr) {
  }

  float eval() const {
    return expr.eval();
  }

  Expr expr;
};

void debugTree(tiny_bnf::Node node, int depth = 0) {
  std::cout << std::string(depth, ' ') << node.symbol << '\n';
  for (auto child : node.children)
    debugTree(child, depth + 1);
}

auto buildParser() {
  tiny_bnf::Specification spec;

  spec["stmt"] >= "expr";

  spec["expr"] >= "term", "+", "expr";
  spec["expr"] >= "term", "-", "expr";
  spec["expr"] >= "term";

  spec["term"] >= "factor", "*", "term";
  spec["term"] >= "factor", "/", "term";
  spec["term"] >= "factor";

  spec["factor"] >= "(", "expr", ")";
  spec["factor"] >= "number";

  spec["number"] >= "integer" | "integer", ".", "integer";

  spec["integer"] >= "digit" | "integer", "digit";

  spec["digit"] >= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9";

  //
  tiny_bnf::Terminals terminals = autoTerminals(spec);
  terminals[" "] = "";

  //
  using tiny_bnf::Ctor;
  tiny_bnf::Generator gen;
  gen.bind<Stmt>("stmt", Ctor<Expr>{});
  gen.bind<Expr>("expr", Ctor<Term, Add, Expr>{}, Ctor<Term, Subtract, Expr>{}, Ctor<Term>{});
  gen.bind<Term>("term", Ctor<Factor, Multiply, Term>{}, Ctor<Factor, Divide, Term>{}, Ctor<Factor>{});
  gen.bind<Factor>("factor", Ctor<LeftParenthesis, Expr, RightParenthesis>{}, Ctor<Number>{});
  gen.bind<Integer>("integer", Ctor<Digit>{}, Ctor<Integer, Digit>{});
  gen.bind<FloatingPoint>("number", Ctor<Integer>{}, Ctor<Integer, Dot, Integer>{});
  gen.bind<Digit>("digit", Ctor<Zero>{}, Ctor<One>{}, Ctor<Two>{}, Ctor<Three>{}, Ctor<Four>{}, Ctor<Five>{},
                  Ctor<Six>{}, Ctor<Seven>{}, Ctor<Eight>{}, Ctor<Nine>{});
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
  gen.bind<Add>("+");
  gen.bind<Subtract>("-");
  gen.bind<Multiply>("*");
  gen.bind<Divide>("/");
  gen.bind<LeftParenthesis>("(");
  gen.bind<RightParenthesis>(")");
  gen.bind<Dot>(".");

  return std::make_tuple(terminals, spec, std::move(gen));
}

tiny_bnf::Expected<float> eval(std::string input,
                     const std::tuple<tiny_bnf::Terminals, tiny_bnf::Specification, tiny_bnf::Generator>& parser) {
  auto& [terminals, spec, gen] = parser;

  auto tokens = tiny_bnf::tokenize(terminals, input);
  if (!tokens)
    return tiny_bnf::Error<>("Failed to tokenize: " + tokens.error());

  auto tree = tiny_bnf::parse(spec, *tokens, tiny_bnf::ParserType::Earley);
  // debugTree(*tree);

  if (!tree)
    return tiny_bnf::Error<>("Failed to parse: " + tree.error());

  auto stmt = tiny_bnf::generate<Stmt>(gen, (*tree)[0]);
  if (!stmt)
    return tiny_bnf::Error<>("Failed to generate: " + stmt.error());

  return stmt->eval();
}