//type:fp
//options:--c++11 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

struct A
{
  static constexpr bool v = false;
};

template<typename T> struct B
{
  void g(int) noexcept(A::v);
};

struct D : B<bool>
{
  void f()
  {
    g(1);                       // shouldn't have a name-qualifier
  }
};

/* @filter
file-scope name-qualifier
  is_class, qualifier.class_type, name, previous_qualifier, position.seq
file-scope expr-node[kind==enk_routine]
  kind, type, routine, qualifier, position.seq
file-scope expr-node[kind==enk_variable]
  kind, type, variable, qualifier, position.seq
func-scope expr-node[kind==enk_routine]
  kind, type, routine, qualifier, position.seq
func-scope expr-node[kind==enk_variable]
  kind, type, variable, qualifier, position.seq
 */
