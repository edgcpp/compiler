//type:fp
//options:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^(file|func)-scope (constant@|expr-node@.*\nkind: +enk_(routine|constant)|type@.*\nkind: +(is_alias|is_decltype|template_arg_list)|routine@.*\nis_template_function:|name-qualifier@)/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(is_class|qualifier.class_type|position[.]seq|name|kind|routine|constant|con|qualifier|num_template_arguments|arg_list|template_arg_list|orig_template_arg_list|typeref_type|  name|  type|  constant|  explicitly_specified|  decl_position[.]seq|is_template_id|is_global_qualified_name):' -e '^ +(FALSE|TRUE)' -e '^(file|func)-scope ' -e '^$' | edg-enumerate-il-addrs

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
