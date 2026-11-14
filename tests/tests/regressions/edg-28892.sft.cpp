//type:fn
//options_all:--c++20
//options:--g++ --gnu_version=130300:--g++ --gnu_version=150200:--strict
// A pointer-to-integer reinterpret_cast requires sufficient destination width.
static_assert(sizeof(void*) > sizeof(unsigned char), "pointer width");
unsigned char narrow_object(void *p) {
  return reinterpret_cast<unsigned char>(p); // Must be an error in GNU mode.
}
unsigned char narrow_function(void (*p)()) {
  return reinterpret_cast<unsigned char>(p); // Must also be an error.
}
