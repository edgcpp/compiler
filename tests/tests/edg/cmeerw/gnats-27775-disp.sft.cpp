//type:fp
//options:--c++26 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

struct C
{
  char c;
  int i1, i2;
  long l;
};

template<typename T>
void f(T t, C c)
{
  static auto [ t0, ... tx, tl ] = t;
  auto [ c0, ... cx, cl ] = c;
}

template void f(C, C);

/* @filter
file-scope variable
  source_corresp/name, source_corresp/enclosing_routine, next, type,
  container, bindings, storage_class, is_pack, is_pack_element,
  init_kind, dynamic
func-scope variable
  source_corresp/name, source_corresp/enclosing_routine, next, type,
  container, bindings, storage_class, is_pack, is_pack_element,
  init_kind, dynamic
 */
