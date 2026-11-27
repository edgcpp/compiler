//type:fp
//options:--c++11 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope class-type-supplement
  template_arg_list, partial_spec_template_arg_list,
  partial_spec_template_arg_list/type, assoc_template
 */
