//type:fp
//options:--c++11 --no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope class-type-supplement@/' | grep -E -e '^(  type|  explicitly_specified|template_arg_list|partial_spec_template_arg_list|assoc_template):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

template<typename>
struct C;

template<typename ... As>
struct C<int(As ...)>
{
  template<typename U>
  using A = int;

  template<typename U, typename = typename C<int(As ...)>::template A<U>>
  struct V;

  template<typename U, typename V = V<U>>
  void f(U);
};

C<int(int)> c;
