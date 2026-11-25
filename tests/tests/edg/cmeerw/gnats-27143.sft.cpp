//type:fp
//options:--c++20 --no_il_lower --il_display --sys_include=$EDG_BASE/include
//filter:awk -v RS='' -v ORS='\n\n' '!seen && /^func-scope variable@/ && /"marker"/{seen=1; next} seen' | edg-il-filter
//script:edg-first-stage-il
#include <coroutine>

struct R
{
  struct promise_type
  {
    promise_type();
    ~promise_type();

    auto get_return_object() { return R{}; }
    auto initial_suspend() noexcept { return std::suspend_always{}; }
    auto final_suspend() noexcept { return std::suspend_always{}; }
    void unhandled_exception() {}
    void return_void() {}
  };
};

void f(int marker)
{ }

R nontmpl(int p)
{
  int i;
  co_return;
}

template<typename T>
R tmpl(T q)
{
  T j;
  co_return;
}

template R tmpl<int>(int);

/* @filter
func-scope variable
  source_corresp/name, source_corresp/decl_position.seq,
  source_corresp/parent_scope, source_corresp/enclosing_routine,
  source_corresp/referenced, source_corresp/needed,
  source_corresp/is_local_to_function, type, storage_class, init_kind,
  is_parameter
 */
