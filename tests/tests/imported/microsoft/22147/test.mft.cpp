//options_all:--microsoft_version=1925 --ms_c++17 --set_flag coroutines
//type:fp
//source_files:coro_support.h
#include "coro_support.h"

struct B
{
    ~B() noexcept {}
    B& operator=(B&& other) noexcept { return *this; }
};

struct IAO : B
{
    IAO() {}
};

struct await_adapter
{
    bool await_ready() const { return false; }
    void await_suspend(std::experimental::coroutine_handle<> handle) const {}
    B await_resume() const { return {}; }
};

await_adapter operator co_await(const IAO& async)
{
    return {};
}

namespace std::experimental
{
    template <typename... Args>
    struct coroutine_traits<IAO, Args...>
    {
        struct promise_type final
        {
            IAO get_return_object() const noexcept { return{}; }
            std::experimental::suspend_never initial_suspend() const noexcept { return{}; }
            std::experimental::suspend_never final_suspend() noexcept { return{}; }
            void unhandled_exception() noexcept {}

            template <typename Expression>
            Expression&& await_transform(Expression&& expression) { return {}; }
        };
    };
}

IAO f()
{
    co_await IAO{};
}
