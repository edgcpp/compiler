namespace std {
  namespace experimental {

    template <typename _Ret, typename... _Ts>
    struct coroutine_traits
    {
        using promise_type = typename _Ret::promise_type;
    };

    template <typename _PromiseT = void>
    struct coroutine_handle;

    template <>
    struct coroutine_handle<void>
    {
        void resume() const {}
    };

    template <typename _PromiseT>
    struct coroutine_handle : coroutine_handle<>
    {};

    template <typename _Ret, typename... _Ts>
    struct _Resumable_helper_traits
    {
        using _Traits = coroutine_traits<_Ret, _Ts...>;
        using _PromiseT = typename _Traits::promise_type;

        static _PromiseT * _Promise_from_frame(void*) noexcept { return nullptr; }
        static coroutine_handle<_PromiseT> _Handle_from_frame(void*) noexcept { return {}; }
        template <typename... _Us>
        static void * _Alloc(size_t, _Us&&...) { return nullptr; }
        static void _Free(size_t, void*) {}
        static void _ConstructPromise(void*, void*, int) {}
        static void _DestructPromise(void*) {}
    };

    template <typename T>
    struct generator {
      struct promise_type {
        auto get_return_object() { return generator{}; }
        auto initial_suspend() { return true; }
        auto final_suspend() { return true; }
        auto yield_value(T const&) {
          struct awaiter {
            bool await_ready() { return false; }
            void await_suspend(coroutine_handle<>) {}
            T await_resume() { return {}; }
          };
          return awaiter{};
        }
      };
      ~generator() {}
    };

    template <typename T>
    struct task {
      struct promise_type {
        auto get_return_object() { return task<T>{}; }
        auto initial_suspend() { return true; }
        auto final_suspend() { return true; }
        void return_value(T const&) {}
      };
      ~task() {}
    };

    template <>
    struct task<void> {
      struct promise_type {
        auto get_return_object() { return task<void>{}; }
        auto initial_suspend() { return true; }
        auto final_suspend() { return true; }
        void return_void() {}
      };
      ~task() {}
    };

    struct suspend_never {
      bool await_ready() noexcept { return true; }
      void await_suspend(coroutine_handle<>) noexcept {}
      void await_resume() noexcept {}
    };
  }
}
