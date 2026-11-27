//type:fp
//options:--c++20 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<typename T>
struct C
{
  using L = decltype([] (auto p) { return 0; });

  struct N
  {
    auto operator()(auto p) { return 0; }
  };
};

int i = C<int>::L()(1) + C<int>::N()(1);

/* @filter
file-scope template
  source_corresp/name, source_corresp/decl_position.seq, kind,
  template_decl, routine, prototype_template
 */
