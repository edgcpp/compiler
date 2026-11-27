//type:fp
//options:--c++20:--ms_c++20 --microsoft_version 1938
//options_all:--no_il_lower --il_display --sys_include=$EDG_BASE/include
//filter:edg-il-filter
//script:edg-first-stage-il

// just make sure that we always create a final_suspend_label and that the
// implicitly added goto statement only sets the object lifetime for
// non-prototype instantiations

#include <coroutine>

namespace minimal
{
  //#include <coroutine>
  template<typename T>
  std::task<void> f(T) {
    co_return;
  }
}

struct C
{
  ~C();
};

template<typename T>
void tmpl_fn()
{
  C c;
  goto done;
done:
  return;
}

void non_tmpl_fn()
{
  C c;
  goto done;
done:
  return;
}

template<typename T>
std::task<int> non_dpdt_params()
{
  C c;
  co_return 1;
}

template<typename T>
std::task<int> dpdt_params(T)
{
  C c;
  co_return 1;
}

std::task<int> non_tmpl()
{
  C c;
  co_return 1;
}

/* @filter
func-scope statement[kind==stmk_coroutine]
  kind, position.seq, body_generated, final_suspend_label,
  get_return_object_call
func-scope statement[kind==stmk_coroutine_return]
  kind, position.seq
func-scope statement[kind==stmk_label]
  kind, position.seq, lifetime, label
func-scope statement[kind==stmk_goto]
  kind, position.seq, lifetime, label
 */
