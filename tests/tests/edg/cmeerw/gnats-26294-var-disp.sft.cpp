//type:fp
//options:--c++20:--c++20:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<typename T, typename U = T>
int var = 1;

using INT1 = int;
using INT2 = int;

#if TEST_NUMBER == 1

int i =
  var<int> +
  var<INT1> +                   // should show up in IL
  var<INT2> +                   // should show up in IL
  var<INT1> +                   // should point to existing template args
  var<INT2>;                    // should point to existing template args

#elif TEST_NUMBER == 2

int i =
  var<int> +
  var<int, int> +
  var<INT1> +                   // should show up in IL
  var<INT2> +                   // should show up in IL
  var<INT1, INT1> +             // should point to existing template args
  var<INT2, INT2>;              // should point to existing template args

#elif TEST_NUMBER == 3

struct C
{
  template<typename T>
  static inline int var = 1;
};

int i =
  C::var<int> +
  C::var<INT1> +                // should show up in IL
  C::var<INT2> +                // should show up in IL
  C::var<INT1> +                // should point to existing template args
  C::var<INT2>;                 // should point to existing template args

#elif TEST_NUMBER == 4

template<typename U>
struct C
{
  template<typename T>
  static inline int var = 1;
};

int i =
  C<int>::var<int> +
  C<INT1>::var<INT1> +          // should show up in IL
  C<INT2>::var<INT2> +          // should show up in IL
  C<INT1>::var<INT1> +          // should point to existing template args
  C<INT2>::var<INT2>;           // should point to existing template args

#elif TEST_NUMBER == 5

template<typename U>
struct C
{
  template<typename V>
  struct N
  {
    template<typename T>
    static inline int var = 1;
  };
};

int i =
  C<int>::N<int>::var<int> +
  C<INT1>::N<INT1>::var<INT1> + // should show up in IL
  C<INT2>::N<INT2>::var<INT2> + // should show up in IL
  C<INT1>::N<INT1>::var<INT1> + // should point to existing template args
  C<INT2>::N<INT2>::var<INT2>;  // should point to existing template args

#endif

/* @filter
file-scope name-reference[is_global_qualified_name==TRUE]
  num_template_arguments, qualifier, is_template_id, is_global_qualified_name
file-scope expr-node[kind==enk_variable]
  kind, orig_template_arg_list, num_template_arguments, qualifier, variable,
  position.seq, is_template_id, is_global_qualified_name
file-scope type[kind==tk_struct]
  source_corresp/name, source_corresp/decl_position.seq, kind
file-scope type[kind==template_arg_list]
  kind, typeref_type, template_arg_list
file-scope variable
  source_corresp/name, source_corresp/decl_position.seq, template_info
file-scope variable-template-info
  template_arg_list
file-scope name-qualifier
  qualifier.class_type, name, is_class
 */
