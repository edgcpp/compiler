//type:fp
//options:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (name-qualifier@|src-seq-secondary-decl@|class-type-supplement@|type@.*\nkind: +(tk_struct|template_arg_list|name_qualifier|is_alias)|type@.*\nthis_class:|routine@.*\nfunction_def_number: *[1-9])/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  name|  type|  explicitly_specified|decl_position[.]seq|is_class|qualifier|qualifier[.]class_type|assoc_routine|extra_info|kind|entity|type|declared_type|return_type|param_type_list|this_class|typeref_type|template_arg_list|orig_template_arg_list|is_global_qualified_name):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

#if TEST_NUMBER == 1

template<typename T>
struct C
{ };

struct B
{
  struct D
  {
    using type = int;
  };

  using I = D::type;
  C<I> f(int *);
};

C<B::I> B::f(int *)
{ return { }; }

#elif TEST_NUMBER == 2

template<typename>
struct B
{ };

template<typename>
struct P
{ };

template<typename T>
struct C
{
  using A = T;
  using type = B<typename A::type>;

  P<type> f(int);
};

template<typename T>
P<typename C<T>::type> C<T>::f(int)
{
  return { };
}

#elif TEST_NUMBER == 3

using CINT = int;
using DINT = int;

struct C
{
  using INT = int;
  INT cf();
};

template<typename T>
struct D
{
  using INT = int;
  INT df();
};

CINT C::cf()
{
  return { };
}

template<typename T>
DINT D<T>::df()
{
  return { };
}

#endif
