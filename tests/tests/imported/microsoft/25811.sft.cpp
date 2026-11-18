//type:cp
//options_all:--c++17
struct A {
    template <class F, typename Output, typename ...Args>
    bool Call(Output &output, Args &...args, F f = F()) const {
        return f(output, args...);
    }

    template <class F, typename Output, typename ...Args>
    bool Call2(Output &output, F f = F()) const {
        return f(output);
    }
};

struct B {
    template <class F, typename Output, typename ...Args>
    static bool Call(Output &output, Args &...args, F f = F()) {
        return f(output, args...);
    }

    template <class F, typename Output, typename ...Args>
    static bool Call2(Output &output, F f = F()) {
        return f(output);
    }
};

struct F {
    bool operator()(unsigned int &i) const {
        return (i = i % 2 ? i * 3 + 1 : i >> 1) != 1;
    }
};

int main() {
    unsigned int i = 100;

    A a;
    a.Call<F>(i);
    a.Call2<F>(i);

    B::Call<F>(i);
    B::Call2<F>(i);
}
