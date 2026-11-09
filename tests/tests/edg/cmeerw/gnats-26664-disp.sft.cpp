//type:fp
//options:--c++20 --no_il_lower --il_display
//filter:awk '/^(func-scope lambda-capture|file-scope field)@/{f=1; print $0; next}/^$/{if (f) print $0; f=0}f' | grep -E -e '^(  name|captured.init_capture_field|captured.initializer|closure_field|is_pack_expansion|is_init_capture|is_indirect_init_capture|is_captured_pack_element|offset|type):' -e '^(file-scope|func-scope) ' -e '^$' | sed -e 's/@[0-9a-f]*//'

namespace captures
{
  template <typename ... T> constexpr int nested(T ... p) {
    return [...c=p] {
      return [c...] {
        return (c + ...);
      }();
    }();
  }

  constexpr int v = nested(1, 2L);
}
