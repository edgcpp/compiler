//options_all:--c++20 --no_il
template <typename TResult>
struct IAsyncOperation {};

namespace std::experimental {
    template <class = void>
    struct coroutine_handle;

    template <>
    struct coroutine_handle<void> {
        static coroutine_handle from_address(void *_Addr) noexcept;
    };

    template <typename _PromiseT>
    struct coroutine_handle : coroutine_handle<> {};

    template <typename _Ret, typename ..._Ts>
    struct coroutine_traits {};

    struct suspend_never {
        bool await_ready() noexcept { return true; }
        void await_suspend(coroutine_handle<>) noexcept {}
        void await_resume() noexcept {}
    };

    template <typename TResult, typename ...Args>
    struct coroutine_traits<IAsyncOperation<TResult>, Args...> {
        struct promise_type final {
            auto get_return_object() noexcept { return IAsyncOperation<TResult>{}; }
            suspend_never initial_suspend() noexcept { return {}; }
            suspend_never final_suspend() noexcept { return {}; }
            void unhandled_exception() noexcept {}

            void return_value(TResult &&value) noexcept;
            void return_value(TResult const &value) noexcept;
        };
    };
}

IAsyncOperation<int> _ShowMultiLinePasteWarningDialog() {
    co_return 0;
}
