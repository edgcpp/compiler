//type:cp
//options_all:--ms_c++20 --microsoft_v 1940
auto f = [i = 1]() {
    auto g = [&i]<class F>(F f) requires requires (F f) { f(i); } {};
};
