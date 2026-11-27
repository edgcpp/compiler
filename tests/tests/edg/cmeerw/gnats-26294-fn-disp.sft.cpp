//type:fp
//options:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<typename T, typename U = T>
int fn();

template<int I, int J = I>
int gn();

using INT1 = int;
using INT2 = int;

#if TEST_NUMBER == 1

int i =
  fn<int>() +
  fn<INT1>() +                  // should show up in IL
  fn<INT2>() +                  // should show up in IL
  fn<INT1>() +                  // should point to existing template args
  fn<INT2>();                   // should point to existing template args

#elif TEST_NUMBER == 2

int i =
  fn<int>() +
  fn<int, int>() +
  fn<INT1>() +                  // should show up in IL
  fn<INT2>() +                  // should show up in IL
  fn<INT1, INT1>() +            // should point to existing template args
  fn<INT2, INT2>();             // should point to existing template args

#elif TEST_NUMBER == 3

struct C
{
  template<typename T>
  static int fn();
};

int i =
  C::fn<int>() +
  C::fn<INT1>() +               // should show up in IL
  C::fn<INT2>() +               // should show up in IL
  C::fn<INT1>() +               // should point to existing template args
  C::fn<INT2>();                // should point to existing template args

#elif TEST_NUMBER == 4

template<typename U>
struct C
{
  template<typename T>
  static int fn();
};

int i =
  C<int>::fn<int>() +
  C<INT1>::fn<INT1>() +         // should show up in IL
  C<INT2>::fn<INT2>() +         // should show up in IL
  C<INT1>::fn<INT1>() +         // should point to existing template args
  C<INT2>::fn<INT2>();          // should point to existing template args

#elif TEST_NUMBER == 5

int i =
  gn<1>() +
  gn<1, 1>() +
  gn<2>() +
  gn<2, 2>();

#elif TEST_NUMBER == 6

void g()
{
  using INT1 = int;
  using INT2 = int;

  long l;

  int i =
    fn<int>() +
    fn<decltype(i)>() +
    fn<INT1>() +
    fn<INT2>() +
    fn<decltype(l)>() +
    fn<long>();
}

#elif TEST_NUMBER == 7

template<typename T>
void g()
{
  using type = T;
  int i = fn<type>();
}

#endif

/* @filter
file-scope name-reference[is_global_qualified_name==TRUE]
  num_template_arguments, qualifier, is_template_id, is_global_qualified_name
file-scope constant
  source_corresp/name, source_corresp/decl_position.seq, kind
func-scope constant
  kind, arg_list, con
file-scope expr-node[kind==enk_routine]
  kind, orig_template_arg_list, num_template_arguments, qualifier, routine,
  position.seq, is_global_qualified_name, is_template_id
func-scope expr-node[kind==enk_routine]
  kind, orig_template_arg_list, num_template_arguments, qualifier, routine,
  position.seq, is_global_qualified_name, is_template_id
file-scope expr-node[kind==enk_constant]
  kind, arg_list, num_template_arguments, qualifier, qualifier.class_type,
  name, constant, routine, position.seq, is_global_qualified_name,
  is_template_id
func-scope expr-node[kind==enk_constant]
  kind, orig_template_arg_list, num_template_arguments, qualifier, constant,
  position.seq, is_global_qualified_name, is_template_id
file-scope routine[is_template_function]
  source_corresp/name, source_corresp/decl_position.seq, template_arg_list
func-scope routine[is_template_function]
  source_corresp/name, source_corresp/decl_position.seq, template_arg_list,
  orig_template_arg_list
file-scope name-qualifier
  qualifier.class_type, name, is_class
func-scope name-qualifier
  qualifier.class_type, name, is_global_qualified_name, is_class
file-scope type[kind==is_alias]
  source_corresp/name, source_corresp/decl_position.seq, kind, typeref_type
file-scope type[kind==template_arg_list]
  kind, typeref_type, template_arg_list
func-scope type[kind==template_arg_list]
  source_corresp/name, source_corresp/decl_position.seq, kind, typeref_type,
  template_arg_list, orig_template_arg_list, qualifier
func-scope type[kind==is_alias]
  source_corresp/name, source_corresp/decl_position.seq, kind, typeref_type,
  template_arg_list, orig_template_arg_list, qualifier
file-scope type[kind==is_decltype]
  kind, typeref_type
func-scope type[kind==is_decltype]
  source_corresp/name, source_corresp/decl_position.seq, kind, typeref_type,
  template_arg_list, orig_template_arg_list, qualifier
 */
