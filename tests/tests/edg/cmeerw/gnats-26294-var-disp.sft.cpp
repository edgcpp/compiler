//type:fp
//options:--c++20:--c++20:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (expr-node@.*\nkind: +enk_variable|type@.*\nkind: +(tk_struct|template_arg_list)|variable@|variable-template-info@|name-qualifier@)/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(is_class|is_global_qualified_name|qualifier|qualifier.class_type|name|kind|variable|template_info|num_template_arguments|template_arg_list|orig_template_arg_list|typeref_type|is_template_id|position[.]seq|  name|  type|  explicitly_specified|  decl_position[.]seq):' -e '^ +(FALSE|TRUE)' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

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
