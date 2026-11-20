//options_all:--microsoft_v 1300 --c++11
namespace foo {
              using bar = int;
              namespace impl {}
}

namespace alias = foo::impl;

namespace alias {
              bar f();
}
