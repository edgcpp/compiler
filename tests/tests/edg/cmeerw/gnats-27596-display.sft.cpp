//type:fp
//options:--c++20 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

template<typename> concept X = true;

template<typename T> struct C
{
  template<typename U>
  void f(T, U) requires X<T>;

  void g(T, int) requires X<T>;
};

template<typename U>
void h(U) requires X<U>;

template<>
void h(int);

template<>
void h(long)
{ }

template<> template<typename U>
void C<int>::f(int, U) requires X<int>;

template<> template<typename U>
void C<long>::f(long, U) requires X<long>
{ }

template<>
void C<int>::g(int, int);

template<>
void C<long>::g(long, int)
{ }

/* @filter
file-scope requires-clause
  constraint, requires_pos.seq
file-scope routine
  source_corresp/name, source_corresp/decl_position.seq, type, defined,
  is_prototype_instantiation, is_template_function, is_specialized,
  trailing_requires_clause, assoc_template
 */
