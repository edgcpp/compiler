//options_all:--microsoft_v 1938 --ms_c++latest
void f() {
    bool captured_variable = false;
    auto do_job = [&](this auto &&) {
        [&](auto &&) { return captured_variable; };
    };
}
