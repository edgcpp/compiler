//type:fp
//options_all:--c++20
namespace std {
    template <class T>
    struct coroutine_traits { using promise_type = typename T::promise_type; };

    template <class = void>
    struct coroutine_handle;
    template <>
    struct coroutine_handle<void> { static coroutine_handle from_address(void *); };
    template <class P>
    struct coroutine_handle : coroutine_handle<> {};

    struct suspend_never {
        bool await_ready() const noexcept { return true; }
        void await_suspend(std::coroutine_handle<>) const noexcept {}
        void await_resume() const noexcept {}
    };
}

template <typename T>
struct my_generator {
    struct promise_type {
        my_generator<T> get_return_object() { return {}; }
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_never final_suspend() noexcept { return {}; }
        void unhandled_exception() {}
        auto yield_value(T value) {
            struct awaiter {
                bool await_ready() const noexcept { return true; }
                void await_suspend(std::coroutine_handle<>) const noexcept {}
                int await_resume() const noexcept { return 0; }
            };
            return awaiter{};
        }
        void return_void() {}
    };
};

my_generator<int> f() {
    co_yield 3, co_yield 4;
}
