//type:fp
//options:-w --c++14 --clang_version 190100 --no_il_lower --il_display
//filter:awk '/^file-scope (constant|expr-node)@/{f=1}/^$/{if (f) print $0; f=0}f' | grep -E -e '^(  name|type|orig_lvalue_type|kind|routine|constant|qualifier|special_kind|is_qualified_name|operation[.][a-z]+|position[.][a-z]+|):' -e '^file-scope ' -e '^$' | sed -e 's/@[0-9a-f]*//g'

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
