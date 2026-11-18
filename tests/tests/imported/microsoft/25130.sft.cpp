//options_all:--ms_c++14 --ms_std_preprocessor
void foo(int x) {}

#define CALL( x, ... ) foo(x, ## __VA_ARGS__)

int main() {
    CALL(1);
}
