//type:fp
//options:--c++20:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
file-scope template
  source_corresp/name, kind
file-scope expr-node
  kind, type, constant, operation.kind, position.seq, concept_id.args
 */
