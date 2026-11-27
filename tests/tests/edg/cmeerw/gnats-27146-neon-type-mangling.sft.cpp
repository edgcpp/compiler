//type:fp
//options:--gn 130200 --target linux_aarch64:--gn 130200 --target linux_armv7:--clang_version 180100 --target linux_aarch64:--clang_version 180100 --target linux_armv7
//options_all:-w --c++17 --il_display
//filter:edg-il-filter

namespace arm_neon_types
{
#if !defined(__clang__)
#if _LP64
  void f(__Bfloat16x4_t v)
  { }
  void f(__Bfloat16x8_t v)
  { }
  void f(__Float16x4_t v)
  { }
  void f(__Float16x8_t v)
  { }
  void f(__Float32x2_t v)
  { }
  void f(__Float32x4_t v)
  { }
  void f(__Float64x1_t v)
  { }
  void f(__Float64x2_t v)
  { }

  void f(__Int8x8_t v)
  { }
  void f(__Int8x16_t v)
  { }
  void f(__Int16x4_t v)
  { }
  void f(__Int16x8_t v)
  { }
  void f(__Int32x2_t v)
  { }
  void f(__Int32x4_t v)
  { }
  void f(__Int64x1_t v)
  { }
  void f(__Int64x2_t v)
  { }

  void f(__Uint8x8_t v)
  { }
  void f(__Uint8x16_t v)
  { }
  void f(__Poly8x8_t v)
  { }
  void f(__Poly8x16_t v)
  { }
  void f(__Uint16x4_t v)
  { }
  void f(__Uint16x8_t v)
  { }
  void f(__Poly16x4_t v)
  { }
  void f(__Poly16x8_t v)
  { }
  void f(__Uint32x2_t v)
  { }
  void f(__Uint32x4_t v)
  { }
  void f(__Uint64x1_t v)
  { }
  void f(__Uint64x2_t v)
  { }
  void f(__Poly64x1_t v)
  { }
  void f(__Poly64x2_t v)
  { }

  void f(__Poly8_t v)
  { }
  void f(__Poly16_t v)
  { }
  void f(__Poly64_t v)
  { }
  void f(__Poly128_t v)
  { }
#else
  void f(__simd64_bfloat16_t v)
  { }
  void f(__simd128_bfloat16_t v)
  { }
  void f(__simd64_float16_t v)
  { }
  void f(__simd128_float16_t v)
  { }
  void f(__simd64_float32_t v)
  { }
  void f(__simd128_float32_t v)
  { }

  void f(__simd64_int8_t v)
  { }
  void f(__simd128_int8_t v)
  { }
  void f(__simd64_int16_t v)
  { }
  void f(__simd128_int16_t v)
  { }
  void f(__simd64_int32_t v)
  { }
  void f(__simd128_int32_t v)
  { }
  void f(__simd128_int64_t v)
  { }

  void f(__simd64_uint8_t v)
  { }
  void f(__simd128_uint8_t v)
  { }
  void f(__simd64_poly8_t v)
  { }
  void f(__simd128_poly8_t v)
  { }
  void f(__simd64_uint16_t v)
  { }
  void f(__simd128_uint16_t v)
  { }
  void f(__simd64_poly16_t v)
  { }
  void f(__simd128_poly16_t v)
  { }
  void f(__simd64_uint32_t v)
  { }
  void f(__simd128_uint32_t v)
  { }
  void f(__simd128_uint64_t v)
  { }
#endif
#else
  void f(__attribute__((neon_vector_type(4))) __bf16 v)
  { }
  void f(__attribute__((neon_vector_type(8))) __bf16 v)
  { }
  void f(__attribute__((neon_vector_type(4))) __fp16 v)
  { }
  void f(__attribute__((neon_vector_type(8))) __fp16 v)
  { }
  void f(__attribute__((neon_vector_type(2))) float v)
  { }
  void f(__attribute__((neon_vector_type(4))) float v)
  { }
#if _LP64
  void f(__attribute__((neon_vector_type(1))) double v)
  { }
  void f(__attribute__((neon_vector_type(2))) double v)
  { }
#endif

  void f(__attribute__((neon_vector_type(8))) signed char v)
  { }
  void f(__attribute__((neon_vector_type(16))) signed char v)
  { }
  void f(__attribute__((neon_vector_type(4))) short v)
  { }
  void f(__attribute__((neon_vector_type(8))) short v)
  { }
  void f(__attribute__((neon_vector_type(2))) int v)
  { }
  void f(__attribute__((neon_vector_type(4))) int v)
  { }

#if _LP64
  void f(__attribute__((neon_vector_type(1))) long v)
  { }
  void f(__attribute__((neon_vector_type(2))) long v)
  { }
  void f2(__attribute__((neon_vector_type(1))) long long v)
  { }
  void f2(__attribute__((neon_vector_type(2))) long long v)
  { }
#else
  void f(__attribute__((neon_vector_type(1))) long long v)
  { }
  void f(__attribute__((neon_vector_type(2))) long long v)
  { }
  void f2(__attribute__((neon_vector_type(2))) long v)
  { }
  void f2(__attribute__((neon_vector_type(4))) long v)
  { }
#endif

  void f(__attribute__((neon_vector_type(8))) unsigned char v)
  { }
  void f(__attribute__((neon_vector_type(16))) unsigned char v)
  { }
#if _LP64
  void f(__attribute__((neon_polyvector_type(8))) unsigned char v)
  { }
  void f(__attribute__((neon_polyvector_type(16))) unsigned char v)
  { }
#else
  void f(__attribute__((neon_polyvector_type(8))) signed char v)
  { }
  void f(__attribute__((neon_polyvector_type(16))) signed char v)
  { }
#endif
  void f(__attribute__((neon_vector_type(4))) unsigned short v)
  { }
  void f(__attribute__((neon_vector_type(8))) unsigned short v)
  { }
#if _LP64
  void f(__attribute__((neon_polyvector_type(4))) unsigned short v)
  { }
  void f(__attribute__((neon_polyvector_type(8))) unsigned short v)
  { }
#else
  void f(__attribute__((neon_polyvector_type(4))) short v)
  { }
  void f(__attribute__((neon_polyvector_type(8))) short v)
  { }
#endif
  void f(__attribute__((neon_vector_type(2))) unsigned int v)
  { }
  void f(__attribute__((neon_vector_type(4))) unsigned int v)
  { }
#if _LP64
  void f(__attribute__((neon_vector_type(1))) unsigned long v)
  { }
  void f(__attribute__((neon_vector_type(2))) unsigned long v)
  { }
  void f(__attribute__((neon_polyvector_type(1))) unsigned long v)
  { }
  void f(__attribute__((neon_polyvector_type(2))) unsigned long v)
  { }

  void f2(__attribute__((neon_vector_type(1))) unsigned long long v)
  { }
  void f2(__attribute__((neon_vector_type(2))) unsigned long long v)
  { }
  void f2(__attribute__((neon_polyvector_type(1))) unsigned long long v)
  { }
  void f2(__attribute__((neon_polyvector_type(2))) unsigned long long v)
  { }
#else
  void f(__attribute__((neon_vector_type(1))) unsigned long long v)
  { }
  void f(__attribute__((neon_vector_type(2))) unsigned long long v)
  { }
  void f(__attribute__((neon_polyvector_type(1))) long long v)
  { }
  void f(__attribute__((neon_polyvector_type(2))) long long v)
  { }
#endif

  using SHORT = short;
  void g(__attribute__((neon_vector_type(4))) SHORT v)
  { }
#endif
}

/* @filter
func-scope variable
  source_corresp/name, source_corresp/enclosing_routine, type
 */
