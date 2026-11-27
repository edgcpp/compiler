//type:fp
//options:--c++20:--c++20:--c++20:--c++20:--c++20 --gn 150100
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<typename T, typename U = T>
struct A
{
  struct N
  { };

  template<typename V>
  struct M
  { };
};

using INT1 = int;
using INT2 = int;

#if TEST_NUMBER == 1

struct D : A<int>
{ };

struct D1 : A<INT1>
{ };

struct D2 : A<INT2>
{ };

struct DD1 : A<INT1>
{ };

struct DD2 : A<INT2>
{ };

#elif TEST_NUMBER == 2

struct D : ::A<int>
{ };

struct D1 : ::A<INT1>
{ };

struct D2 : ::A<INT2>
{ };

struct DD1 : ::A<INT1>
{ };

struct DD2 : ::A<INT2>
{ };

#elif TEST_NUMBER == 3

template<typename T>
struct D1 : A<T>::N
{ };

template<typename T>
struct D2 : A<T>::template M<T>
{ };

template<typename T>
struct GD1 : ::A<T>::N
{ };

template<typename T>
struct GD2 : ::A<T>::template M<T>
{ };

#elif TEST_NUMBER == 4

template<typename T>
struct B : A<typename T::template N<typename T::type>>
{ };

#elif TEST_NUMBER ==5
// this one should be run in GCC mode

struct B
{ };

namespace ns
{
  template<typename> using A = B;
}

template<typename T>
struct C : ns::A<T>
{ };

#endif

/* @filter
file-scope name-qualifier
  qualifier.class_type, previous_qualifier, name, is_class
file-scope base-class
  type, orig_type
file-scope class-type-supplement
  proxy_of_type, template_arg_list
file-scope type[kind==tk_struct]
  source_corresp/name, source_corresp/decl_position.seq, kind, extra_info
file-scope type[kind==template_arg_list]
  kind, typeref_type, template_arg_list
file-scope type[kind==name_qualifier]
  kind, typeref_type, qualifier
file-scope type[kind==tk_class]
  source_corresp/name, source_corresp/decl_position.seq, kind, extra_info
file-scope type[kind==is_template_alias]
  source_corresp/name, source_corresp/decl_position.seq, kind, typeref_type,
  template_arg_list
 */
