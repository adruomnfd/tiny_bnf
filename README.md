**work in progress**

### Minimal example
```c++
#include <tiny_bnf.h>

#include <iostream>

struct Digit {
  Digit(std::string str) : val(std::stoi(str)) {}
  int val;
};

struct Number {
  Number(Digit d) : val(d.val) {}
  Number(Number n, Digit d) : val(n.val * 10 + d.val) {}
  int val;
};

int main() {
  namespace bnf = tiny_bnf;

  // Specify the grammar
  // which is(in bnf):
  //   number ::= digit | number digit
  //   digit  ::= 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9
  auto spec = bnf::Specification();
  spec["number"] >= "digit" | "number", "digit";
  spec["digit"] >= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9";

  // deduce the terminal symbols automatically 
  // by adding symbols that aren't present on LHS
  auto terminals = bnf::autoTerminals(spec);

  // tokenize and parse input
  auto tokens = bnf::tokenize(terminals, "31415926");
  auto tree = bnf::parse(spec, *tokens);
  
  // generate Number from parsed tree using bindings specified.
  auto gen = bnf::Generator();
  gen.bind<Digit>("digit", bnf::UseString{});
  gen.bind<Number>("number", bnf::Ctor<Digit>{}, bnf::Ctor<Number, Digit>{});
  auto number = bnf::generate<Number>(gen, (*tree)[0]);
  
  // print the resulting number
  std::cout << number->val << "\n";
}
```