//type:fp
//use_system_includes: true
//edg_header_pack: exp_meta
//require:DO_IL_LOWERING 1

/*
std::meta::source_location_of is a constant expression for every reflection
([meta.reflection.names] in P2996R13 has no "Constant When" clause for it).
For a value, a type other than a class or enumeration type, the global
namespace, or a data member description, the result is source_location{};
for an entity, it is the location of a declaration.
*/

#include <experimental/meta>
#include <source_location>

using namespace std::meta;

consteval bool is_empty_location(std::source_location l) {
  return l.line() == 0 && l.column() == 0 && l.file_name()[0] == '\0' &&
         l.function_name()[0] == '\0';
}

// No source location.
static_assert(is_empty_location(source_location_of(^^int)));
static_assert(is_empty_location(source_location_of(^^int*)));
static_assert(is_empty_location(source_location_of(^^const int&)));
static_assert(is_empty_location(source_location_of(reflect_constant(42))));
static_assert(is_empty_location(source_location_of(^^::)));
static_assert(is_empty_location(
                source_location_of(data_member_spec(^^int, {.name = "x"}))));
static_assert(is_empty_location(source_location_of(info{})));

// Entities: the line of the declaration.
struct S { int m; };
enum E { e1 };
using Alias = int;
void f();
namespace N {}
constexpr int line_S = __LINE__ - 5;

static_assert(source_location_of(^^S).line() == line_S);
static_assert(source_location_of(^^const S).line() == line_S);
static_assert(source_location_of(^^S::m).line() == line_S);
static_assert(source_location_of(^^E).line() == line_S + 1);
static_assert(source_location_of(^^e1).line() == line_S + 1);
static_assert(source_location_of(^^Alias).line() == line_S + 2);
static_assert(source_location_of(^^f).line() == line_S + 3);
static_assert(source_location_of(^^N).line() == line_S + 4);
static_assert(!is_empty_location(source_location_of(^^S)));
