//options_all:--c++20
struct empty {};

using foo = empty;
using bar = empty;

struct baz {
    [[no_unique_address]] foo a;
    [[no_unique_address]] bar b;
};

baz x;
