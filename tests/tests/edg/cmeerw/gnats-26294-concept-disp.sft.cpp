//type:fp
//options:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (template@|expr-node@)/' | grep -E -e '^(  name|  type|  explicitly_specified|position[.]seq|concept_id[.]args|operation[.]kind|constant|kind|type):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

template<typename T = int, typename U = T>
concept C = true;

using INT1 = int;
using INT2 = int;

#if TEST_NUMBER == 1

bool b = C<> &&
         C<int> &&
         C<INT1> &&
         C<INT1, INT1> &&
         C<INT2> &&
         C<INT2, INT2>;

#elif TEST_NUMBER ==2

void f(C<int> auto v)
{ }

void g(C<INT1> auto v)
{ }

#endif
