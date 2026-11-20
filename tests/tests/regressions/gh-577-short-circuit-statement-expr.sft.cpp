//type: cp
//options:--clang --clang_version=220100:--g++ --gnu_version=150200

bool builtin_operators(bool b) {
  // With no overloads declared yet, the scanner knows from the constant
  // left operands that both initializers skip the right operand.
  bool left_false = false && ({ int value = b; value; });
  bool left_true = true || ({ int value = b; value; });
  // The final statement expression executes when b is true.
  return (left_false == left_true) !=
         (b && ({ int value = b; value; }));
}

// These declarations make the scanner consider overloading when it sees
// either operator name.
struct operators {
  bool operator&&(int);
  bool operator||(int);
};

struct wrapper {
  bool has_value();
};

void test() {
  // Both expressions use built-in operators.  The calls to has_value keep
  // the right operands nonconstant, so the compiler short-circuits during
  // binary folding.  The C++ generator still needs both local declarations.
  (void)(0 && ({
    wrapper value;
    value;
  }).has_value());
  (void)(1 || ({
    wrapper value;
    value;
  }).has_value());
}

bool nested_short_circuit(int i) {
  // The parameter keeps the right operand of && nonconstant.  The right
  // operand of || folds on its own.  The enclosing comparison folds both
  // results and the C++ generator still needs the declarations in both blocks.
  return (false && ({
    int value = i;
    value;
  })) == (true || ({
    struct local {
      bool value;
    };
    local item{false};
    item.value;
  }));
}
