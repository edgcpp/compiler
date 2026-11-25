//type:fp
//options:--c++20 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

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

/* @filter
func-scope lambda-capture
  closure_field, captured.init_capture_field, captured.initializer,
  is_init_capture, is_pack_expansion, is_indirect_init_capture
file-scope field
  source_corresp/name, type, offset, is_captured_pack_element,
  is_init_capture
 */
