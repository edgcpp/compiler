//type:fp
//options:--c++20:--c++20:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (name-qualifier@|variable@|routine@.*\nis_template_function:|class-type-supplement@|type@.*\nkind: +(tk_struct|template_arg_list|name_qualifier)|expr-node@.*\nkind: +(enk_variable|enk_routine))/' | awk '/^[a-z_]+:$/ { getline n; if (n ~ /^   /) { gsub(/  +/, "", n); print $0 " " n; } else { print $0; print n; } next; }1' | grep -E -e '^(  name|  type|  explicitly_specified|  decl_position[.]seq|is_class|position[.]seq|qualifier|previous_qualifier|qualifier[.]class_type|extra_info|kind|type|declared_type|typeref_type|template_arg_list|orig_template_arg_list):' -e '^file-scope ' -e '^ +(FALSE|TRUE)' -e '^$' | edg-enumerate-il-addrs

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
