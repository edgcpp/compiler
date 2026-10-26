//type:fp
//options:--c++17 --clang_version 230000:--c++17 --gnu_version 150000
//options_all:--il_display
//require:RECORD_UNRECOGNIZED_ATTRIBUTES 1
//script:./first_stage_il
//filter:awk '/^file-scope attribute@/{f=1}/^$/{if (f) print $0; f=0}f' | grep -E -e '^(name|is_pack_expansion):' -e '^$' | sed -e 's/@[0-9a-f]*//'
//remark:Unrecognized attribute with a pack expansion in its arguments

// An unrecognized attribute whose arguments referenced a pack was treated as
// a pack expansion.  Each attribute not followed by "..." must appear once per
// declaration and not be a pack expansion; attr_expanded must remain one.

struct mutex {};
struct adopt_lock_t {};

template <class... M>
struct scoped_lock {
  scoped_lock(M&... m) __attribute__((acquire_capability(m...))) {}
  scoped_lock(adopt_lock_t, M&... m)
      __attribute__((requires_capability(m...))) {}
  void varargs(int ...);
};

template <int I> mutex& get();

template <int... I>
void unlock() __attribute__((release_capability(get<I>()...))) {}

template <class... M>
void lock(mutex& a, M&... c)
    __attribute__((try_acquire_capability(true, a, c...))) {}

template <class... M>
void unexpanded(M&... m) __attribute__((attr_unexpanded(m))) {}

template <class... T>
[[attr_expanded(T)...]] void expanded() {}

void f(mutex& a, mutex& b) {
  scoped_lock<mutex, mutex> l1(a, b);
  scoped_lock<mutex, mutex> l2(adopt_lock_t(), a, b);
  scoped_lock<> l3;
  unlock<0, 1>();
  lock(a, b);
  lock(a);
  unexpanded(a, b);
  expanded<int, char>();
}
