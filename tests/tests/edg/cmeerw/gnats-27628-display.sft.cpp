//type:fp
//options:-w --c++14 --clang_version 190100 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

int v;

int i = (__builtin_operator_delete((void*)v),
         v);

void *p = (__builtin_operator_new(4));

template<typename T>
int j = (__builtin_operator_delete((T *)v),
         v);

template<typename T>
void *q = (__builtin_operator_new(T(v)));

int k = (__sync_add_and_fetch(&v, v), __atomic_load_n(&v, v));

template<typename T, T &r>
int l = (__sync_add_and_fetch(&v, T(v)), __atomic_load_n(&v, T(v)),
         __sync_add_and_fetch(&r, T(v)), __atomic_load_n(&r, T(v)));

/* @filter
file-scope constant
  source_corresp/name, position.seq, position.column, type, kind,
  is_qualified_name
file-scope expr-node
  position.seq, position.column, type, orig_lvalue_type, kind, routine,
  constant, qualifier, special_kind, operation.kind
 */
