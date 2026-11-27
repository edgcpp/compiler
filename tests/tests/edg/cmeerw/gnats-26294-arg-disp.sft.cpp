//type:fp
//options:--c++20:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

#if TEST_NUMBER == 1

template<typename T1 = int, typename T2 = T1, typename T3 = T2>
struct A
{ };

template<typename ... T>
struct B
{ };

A<> a0;
A<int> a1;
A<int, int> a2;
A<int, int, int> a3;

B<> b0;
B<int> b1;
B<int, int> b2;
B<int, int, int> b3;

#elif TEST_NUMBER == 2

template<typename T1 = int, typename T2 = T1, typename T3 = T2>
int a = 0;

template<typename ... T>
int b = 0;

auto v = a<> +
         a<int> +
         a<int, int> +
         a<int, int, int> +
         b<> +
         b<int> +
         b<int, int> +
         b<int, int, int>;

#elif TEST_NUMBER == 3

template<typename T1 = int, typename T2 = T1, typename T3 = T2>
int a();

template<typename ... T>
int b();

auto v = a<>() +
         a<int>() +
         a<int, int>() +
         a<int, int, int>() +
         b<>() +
         b<int>() +
         b<int, int>() +
         b<int, int, int>();

#elif TEST_NUMBER == 4

template<typename T = void>
struct C
{
  static int f();
  static int i;
};

auto v = C<void>::f() +
         C<void>::i +
         C<>::f() +
         C<>::i;

#endif

/* @filter
file-scope name-qualifier
  qualifier.class_type, previous_qualifier, name, is_class
file-scope variable
  source_corresp/name, source_corresp/decl_position.seq, type, declared_type
file-scope routine[is_template_function]
  source_corresp/name, source_corresp/decl_position.seq, type, declared_type,
  template_arg_list
file-scope class-type-supplement
  template_arg_list
file-scope type[kind==tk_struct]
  source_corresp/name, source_corresp/decl_position.seq, kind, extra_info
file-scope type[kind==template_arg_list]
  kind, typeref_type, template_arg_list
file-scope type[kind==name_qualifier]
  source_corresp/name, source_corresp/decl_position.seq, kind, declared_type,
  typeref_type, extra_info, template_arg_list, orig_template_arg_list,
  qualifier, previous_qualifier
file-scope expr-node[kind==enk_variable]
  kind, type, orig_template_arg_list, qualifier, position.seq
file-scope expr-node[kind==enk_routine]
  kind, type, qualifier, position.seq
 */
