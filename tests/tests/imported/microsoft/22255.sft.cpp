//type:fp
//options_all:--microsoft_version 1926 --c++20
namespace std {
              template <class R, class ...Ts>
              struct coroutine_traits {
                             using promise_type = typename R::promise_type;
              };

              template <class P = void>
              struct coroutine_handle;

              template <>
              struct coroutine_handle<void> {};

              template <class P>
              struct coroutine_handle : coroutine_handle<> {};

              struct suspend_always {
                             bool await_ready() noexcept;
                             void await_suspend(coroutine_handle<>) noexcept;
                             void await_resume() noexcept;
              };
}

struct promise_base {
              void *operator new(size_t);
              void operator delete(void *, size_t);
};

template <typename T>
struct awaitable {
              struct promise_type : promise_base {
                             awaitable get_return_object();
                             std::suspend_always initial_suspend() noexcept;
                             void return_void();
                             void unhandled_exception();
                             std::suspend_always final_suspend() noexcept;
              };
};

awaitable<void> server() {
              co_return;
}
