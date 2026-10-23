//type:fp
//options:--c++20:--ms_c++20 --microsoft_version 1938
//options_all:--no_il_lower --il_display
//filter:awk '/^func-scope statement@/{f=1; next}/^$/{if (f) print "|"; f=0}f{print $0 " "}' | grep -E -e '^(position.seq|kind|body_generated|final_suspend_label|label|lifetime|get_return_object_call):' -e '^[|]$' | sed -e 's/@[0-9a-f]*[ :]/@ /' -e 's/^  *//' -e 's/:  */: /' -e 's/ func-scope / /' | tr -d '\n' | tr '|' '\n' | grep -E 'kind: stmk_(coroutine|coroutine_return|label|goto) '

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
