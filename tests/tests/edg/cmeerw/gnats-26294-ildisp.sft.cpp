//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^(file-scope base-class@|func-scope constructor-init@)/' | grep -E -e '^(direct|type|base_class|derived_class):' -e '^(func|file)-scope ' -e '^$' | edg-enumerate-il-addrs

struct B1
{ };

template<unsigned>
struct B2
{ };

template<typename ... T>
struct C : B1, B2<sizeof ... (T)>
{
  C(T ...) : B1{}, B2<0 + sizeof ... (T)>{}
  { }
};
