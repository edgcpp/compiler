//type:rp
//use_system_includes: true
//edg_header_pack: meta

/*
Output should be:
$ ./a.out
0 1 2
*/

#include <meta>

using namespace std::meta;
#include <stdio.h>

class TU_Ticket {
  template<int N> struct Helper {
    static constexpr int value = N;
  };

  template<int N>
  static consteval info helper_info() {
    return substitute(^^Helper, { reflect_constant(N) });
  }

  template<int N>
  static consteval int next_impl() {
    if constexpr (!is_complete_type(helper_info<N>())) {
      return extract<int>(
        static_data_members_of(helper_info<N>(),
                               access_context::unchecked())[0]);
    } else {
      return next_impl<N+1>();
    }
  }
public:
  static consteval int next() {
    return next_impl<0>();
  }
};

int x = TU_Ticket::next();  // x initialized to 0.
int y = TU_Ticket::next();  // y initialized to 1.
int z = TU_Ticket::next();  // z initialized to 2.

int main() {
  printf("%d %d %d\n", x, y, z);
}

