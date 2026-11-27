//type:fp
//options:--target linux_x86_64:--target linux_i686:--target linux_aarch64:--target linux_armv7
//options_all:-w --c++11 --clang_version 200100 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

namespace check_types
{
  void f(float f, float __attribute__((__vector_size__(16))) vf,
      int i, int __attribute__((__vector_size__(16))) vi)
  {
    {
      auto rf = __builtin_elementwise_abs(vf);
      decltype(rf) *pf = &vf;
      auto ri = __builtin_elementwise_abs(vi);
      decltype(ri) *pi = &vi;
    }
    {
      auto rf = __builtin_elementwise_acos(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto ri = __builtin_elementwise_add_sat(vi, vi);
      decltype(ri) *pi = &vi;
    }
    {
      auto rf = __builtin_elementwise_asin(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_atan(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_atan2(vf, vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto ri = __builtin_elementwise_bitreverse(vi);
      decltype(ri) *pi = &vi;
    }
    {
      auto rf = __builtin_elementwise_canonicalize(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_ceil(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_copysign(vf, vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_cos(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_cosh(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_exp(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_exp2(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_floor(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_fma(vf, vf, vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_fmod(vf, vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_log(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_log10(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_log2(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto ri = __builtin_elementwise_max(vi, vi);
      decltype(ri) *pi = &vi;
      auto rf = __builtin_elementwise_max(vf, vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_maximum(vf, vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto ri = __builtin_elementwise_min(vi, vi);
      decltype(ri) *pi = &vi;
      auto rf = __builtin_elementwise_min(vf, vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_minimum(vf, vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_nearbyint(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto ri = __builtin_elementwise_popcount(vi);
      decltype(ri) *pi = &vi;
    }
    {
      auto rf = __builtin_elementwise_pow(vf, vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_rint(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_round(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_roundeven(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_sin(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_sinh(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_sqrt(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto ri = __builtin_elementwise_sub_sat(vi, vi);
      decltype(ri) *pi = &vi;
    }
    {
      auto rf = __builtin_elementwise_tan(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_tanh(vf);
      decltype(rf) *pf = &vf;
    }
    {
      auto rf = __builtin_elementwise_trunc(vf);
      decltype(rf) *pf = &vf;
    }

    {
      auto ri = __builtin_reduce_max(vi);
      decltype(ri) *pi = &i;
      auto rf = __builtin_reduce_max(vf);
      decltype(rf) *pf = &f;
    }

    {
      auto ri = __builtin_reduce_and(vi);
      decltype(ri) *pi = &i;
    }
    {
      auto ri = __builtin_reduce_or(vi);
      decltype(ri) *pi = &i;
    }
    {
      auto ri = __builtin_reduce_xor(vi);
      decltype(ri) *pi = &i;
    }
    {
      auto ri = __builtin_reduce_add(vi);
      decltype(ri) *pi = &i;
    }
    {
      auto ri = __builtin_reduce_mul(vi);
      decltype(ri) *pi = &i;
    }
    {
      auto rf = __builtin_reduce_maximum(vf);
      decltype(rf) *pf = &f;
    }
    {
      auto ri = __builtin_reduce_min(vi);
      decltype(ri) *pi = &i;
      auto rf = __builtin_reduce_min(vf);
      decltype(rf) *pf = &f;
    }
    {
      auto rf = __builtin_reduce_minimum(vf);
      decltype(rf) *pf = &f;
    }
  }
}

/* @filter
file-scope routine[builtin_function_kind]
  source_corresp/name, type
 */
