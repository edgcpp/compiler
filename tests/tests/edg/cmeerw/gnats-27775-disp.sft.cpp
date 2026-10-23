//type:fp
//options:--c++26 --no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^(file|func)-scope variable@/' | grep -E -e '^(next|type|dynamic|container|is_pack|is_pack_element|init_kind|bindings|storage_class|  name|  enclosing_routine):' -e '^ +(file|func)-scope variable@' -e '^(file|func)-scope ' -e '^$' | edg-enumerate-il-addrs

struct C
{
  char c;
  int i1, i2;
  long l;
};

template<typename T>
void f(T t, C c)
{
  static auto [ t0, ... tx, tl ] = t;
  auto [ c0, ... cx, cl ] = c;
}

template void f(C, C);
