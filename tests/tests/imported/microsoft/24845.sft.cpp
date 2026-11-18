//type:fp
//options_all:--c++20
namespace std {
    template <class R, class ...>
    struct coroutine_traits { using promise_type = typename R::promise_type; };

    template <class = void>
    struct coroutine_handle;
    template <>
    struct coroutine_handle<void> {};
    template <class>
    struct coroutine_handle {
        static coroutine_handle from_address(void *const) noexcept { return {}; }
        operator coroutine_handle<>() const noexcept;
    };

    struct suspend_never {
        bool await_ready() const noexcept;
        void await_suspend(coroutine_handle<>) const noexcept;
        void await_resume() const noexcept;
    };
}

struct IAsyncAction {
    struct promise_type final {
        IAsyncAction get_return_object() const noexcept;
        std::suspend_never initial_suspend() const noexcept;
        std::suspend_never final_suspend() noexcept;
        void unhandled_exception() noexcept;
        void return_void() const noexcept;
    };
};

struct await_adapter {
    await_adapter(const await_adapter &) = delete;

    bool await_ready() const noexcept;
    bool await_suspend(std::coroutine_handle<> handle);
    int await_resume() const;
};

struct IAsyncOperation { await_adapter operator co_await(); };

IAsyncAction Test(IAsyncOperation op) {
    co_await op;
}
