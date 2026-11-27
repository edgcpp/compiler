//type:fp
//options:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope name-reference[is_global_qualified_name==TRUE]
  num_template_arguments, qualifier, is_global_qualified_name, is_template_id
file-scope name-qualifier
  qualifier.class_type, name, is_class
file-scope src-seq-secondary-decl
  declared_type, entity, decl_position.seq
file-scope class-type-supplement
  template_arg_list
file-scope routine[defined==TRUE]
  source_corresp/name, type, declared_type
file-scope type[kind==tk_struct]
  source_corresp/name, kind, extra_info
file-scope type[kind==template_arg_list]
  kind, typeref_type, template_arg_list
file-scope type[kind==name_qualifier]
  kind, typeref_type, qualifier
file-scope type[kind==is_alias]
  source_corresp/name, kind, typeref_type
file-scope type[this_class]
  kind, return_type, param_type_list, this_class, assoc_routine
 */
