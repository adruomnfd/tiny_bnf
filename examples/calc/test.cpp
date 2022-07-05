#include "calc.h"

int main() {
  auto parser = buildParser();

#define CHECK_ANSWER(x)                                                                            \
  if (auto val = eval(#x, parser)) {                                                               \
    if (std::abs(*val - (x)) < 1e-4)                                                               \
      std::cout << #x << " = " << *val << '\n';                                                    \
    else                                                                                           \
      std::cout << "Answer to [" #x "] is not correct: " << *val << ", should be " << (x) << '\n'; \
  } else {                                                                                         \
    std::cout << "When processing [" #x "]: " << val.error() << '\n';                              \
    exit(1);                                                                                       \
  }

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
  CHECK_ANSWER((3 + (1.0 / 10) + 2));
}
