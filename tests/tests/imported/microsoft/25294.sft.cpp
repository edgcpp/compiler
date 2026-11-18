//options_all:-w
//options:--ms_c++20:--c++20
int main() {
    bool flag = true;

    auto f1 = [](int a, int b) { return a < b; };
    auto f2 = [](int a, int b) { return a > b; };

    true ? f1 : f2;
}
