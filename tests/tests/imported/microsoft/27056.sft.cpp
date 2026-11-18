//type:cp
//options_all:--ms_c++latest --microsoft_v 1940 -w
enum class BasicFormatArgType { NoType, CustomType };

struct Monostate {};

struct Handle {
  Handle(int, int) {}
};

template <typename Context> struct BasicFormatArg {
  BasicFormatArg() = default;

  BasicFormatArg(int x, int y)
      : ActiveState(BasicFormatArgType::CustomType), CustomState(x, y) {}

  BasicFormatArgType ActiveState = BasicFormatArgType::NoType;

  union {
    Monostate NoState = Monostate{};
    Handle CustomState;
  };
};

int main() {
  BasicFormatArg<double> arg{};
  (void)arg;
}
