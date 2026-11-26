//options_all:--c++11 --microsoft
struct Foo {
              enum Bar { X = 0, Y = 1 };
              Bar b : 1;
};

int main() {
              Foo f;
              f = { Foo::Bar::X };
}
