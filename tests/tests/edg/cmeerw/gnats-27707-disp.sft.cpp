//type:fp
//options:--c++20 --no_il_lower --il_display
//filter:awk '/^file-scope template@/{f=1}/^$/{if (f) print $0; f=0}f' | grep -E -e '^(kind|template_decl|routine|prototype_template|  name|  decl_position.seq):' -e '^file-scope ' -e '^$' | sed -e 's/@[0-9a-f]*//'

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
