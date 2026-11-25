//type:fp
//options:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<typename T, typename U = T>
struct A
{
  struct B
  { };

  template<typename V>
  struct C
  {
    C(V);
  };

  enum E
  { };
};

template<typename T>
struct D
{ };

using INT1 = int;
using INT2 = int;

#if TEST_NUMBER == 1

A<int> a;

A<INT1> a1;                     // should show up in IL
A<INT1, INT1> a11;              // should show up in IL
A<INT2> a2;                     // should show up in IL
A<INT2, INT2> a22;              // should show up in IL

A<INT1> aa1;                    // should point to existing template argument list
A<INT2> aa2;                    // should point to existing template argument list

#elif TEST_NUMBER == 2

A<int>::B b;

A<INT1>::B b1;                  // should show up in IL
A<INT1, INT1>::B b11;           // should show up in IL
A<INT2>::B b2;                  // should show up in IL
A<INT2, INT2>::B b22;           // should show up in IL

A<INT1>::B bb1;                 // should point to existing template argument list
A<INT2>::B bb2;                 // should point to existing template argument list

#elif TEST_NUMBER == 3

A<int>::E e;

A<INT1>::E e1;                  // should show up in IL
A<INT2>::E e2;                  // should show up in IL

A<INT1>::E ee1;                 // should point to existing template argument list
A<INT2>::E ee2;                 // should point to existing template argument list

#elif TEST_NUMBER == 4

D<A<int>> d;
D<A<INT1>> d1;                  // should show up in IL
D<A<INT2>> d2;                  // should show up in IL

D<A<INT1>> dd1;                 // should point to existing template argument list
D<A<INT2>> dd2;                 // should point to existing template argument list

#elif TEST_NUMBER == 5

typename A<int>::B b;

typename A<INT1>::B b1;         // should show up in IL
typename A<INT1, INT1>::B b11;  // should show up in IL
typename A<INT2>::B b2;         // should show up in IL
typename A<INT2, INT2>::B b22;  // should show up in IL

typename A<INT1>::B bb1;     // should point to existing template argument list
typename A<INT2>::B bb2;     // should point to existing template argument list

#elif TEST_NUMBER == 6

A<int>::C c{0};
A<INT1>::C c1{1};
A<INT2>::C c2{2};
A<INT1>::C cc1{3};
A<INT2>::C cc2{4};

#elif TEST_NUMBER == 7

using DI = D<int>;
using DI1 = D<INT1>;
using DI2 = D<INT2>;

template<int I, typename T>
int T::*p = 0;

auto v = (p<1, D<int>>,
          p<2, DI>,
          p<3, DI1>,
          p<4, DI2>);

#elif TEST_NUMBER == 8

void f()
{
  struct B
  {
    int i;
  };

  using A = B;

  {
    struct B
    { };

    static int A::*p = &A::i;
  }
}

#elif TEST_NUMBER == 9

template<typename T>
using L = A<T>;

void f()
{
  int i;

  A<decltype(i)> a1;
  A<decltype(+i)> a2;

  L<decltype(i)> l1;
  L<decltype(+i)> l2;
}

#elif TEST_NUMBER == 10

struct A<int>::B b;

struct A<INT1>::B b1;         // should show up in IL
struct A<INT1, INT1>::B b11;  // should show up in IL
struct A<INT2>::B b2;         // should show up in IL
struct A<INT2, INT2>::B b22;  // should show up in IL

struct A<INT1>::B bb1;       // should point to existing template argument list
struct A<INT2>::B bb2;       // should point to existing template argument list

#endif

/* @filter
file-scope name-reference[is_global_qualified_name==TRUE]
  num_template_arguments, qualifier, is_template_id, is_global_qualified_name
file-scope name-qualifier
  qualifier.class_type, name, is_class
func-scope name-qualifier
  qualifier.class_type, name, is_class, is_global_qualified_name
file-scope variable
  source_corresp/name, source_corresp/decl_position.seq, type, declared_type
func-scope variable
  source_corresp/name, source_corresp/decl_position.seq, type, declared_type
file-scope class-type-supplement
  template_arg_list
func-scope class-type-supplement
  template_arg_list, orig_template_arg_list
file-scope type[kind==tk_struct]
  source_corresp/name, source_corresp/decl_position.seq, kind, extra_info
file-scope type[kind==template_arg_list]
  kind, typeref_type, template_arg_list
func-scope type[kind==template_arg_list]
  source_corresp/name, source_corresp/decl_position.seq, kind, type,
  declared_type, typeref_type, class_of_which_a_member,
  orig_class_of_which_a_member, extra_info, template_arg_list,
  orig_template_arg_list, qualifier
file-scope type[kind==name_qualifier]
  kind, typeref_type, qualifier
func-scope type[kind==name_qualifier]
  source_corresp/name, source_corresp/decl_position.seq, kind, type,
  declared_type, typeref_type, class_of_which_a_member,
  orig_class_of_which_a_member, extra_info, template_arg_list,
  orig_template_arg_list, qualifier
func-scope type[kind==tk_struct]
  source_corresp/name, source_corresp/decl_position.seq, kind, type,
  declared_type, typeref_type, class_of_which_a_member,
  orig_class_of_which_a_member, extra_info, template_arg_list,
  orig_template_arg_list, qualifier
file-scope type[kind==tk_ptr_to_member]
  kind, type, class_of_which_a_member, orig_class_of_which_a_member
func-scope type[kind==tk_ptr_to_member]
  source_corresp/name, source_corresp/decl_position.seq, kind, type,
  declared_type, typeref_type, class_of_which_a_member,
  orig_class_of_which_a_member, extra_info, template_arg_list,
  orig_template_arg_list, qualifier
file-scope type[kind==is_alias]
  source_corresp/name, source_corresp/decl_position.seq, kind, typeref_type
func-scope type[kind==is_alias]
  source_corresp/name, source_corresp/decl_position.seq, kind, type,
  declared_type, typeref_type, class_of_which_a_member,
  orig_class_of_which_a_member, extra_info, template_arg_list,
  orig_template_arg_list, qualifier
file-scope type[kind==is_template_alias]
  source_corresp/name, source_corresp/decl_position.seq, kind, typeref_type,
  template_arg_list, orig_template_arg_list
func-scope type[kind==is_template_alias]
  source_corresp/name, source_corresp/decl_position.seq, kind, type,
  declared_type, typeref_type, class_of_which_a_member,
  orig_class_of_which_a_member, extra_info, template_arg_list,
  orig_template_arg_list, qualifier
file-scope type[kind==is_decltype]
  kind, typeref_type
func-scope type[kind==is_decltype]
  source_corresp/name, source_corresp/decl_position.seq, kind, type,
  declared_type, typeref_type, class_of_which_a_member,
  orig_class_of_which_a_member, extra_info, template_arg_list,
  orig_template_arg_list, qualifier
 */
