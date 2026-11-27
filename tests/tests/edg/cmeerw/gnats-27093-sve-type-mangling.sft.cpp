//type:fp
//options:--clang_version 180100
//options_all:--target linux_aarch64 -w --c++17 --il_display
//filter:edg-il-filter

namespace arm_sve_types
{
  void f(__SVInt8_t) { }
  void f(__SVInt16_t) { }
  void f(__SVInt32_t) { }
  void f(__SVInt64_t) { }
  void f(__SVUint8_t) { }
  void f(__SVUint16_t) { }
  void f(__SVUint32_t) { }
  void f(__SVUint64_t) { }
  void f(__SVFloat16_t) { }
  void f(__SVBfloat16_t) { }
  void f(__SVFloat32_t) { }
  void f(__SVFloat64_t) { }
  void f(__clang_svint8x2_t) { }
  void f(__clang_svint16x2_t) { }
  void f(__clang_svint32x2_t) { }
  void f(__clang_svint64x2_t) { }
  void f(__clang_svuint8x2_t) { }
  void f(__clang_svuint16x2_t) { }
  void f(__clang_svuint32x2_t) { }
  void f(__clang_svuint64x2_t) { }
  void f(__clang_svfloat16x2_t) { }
  void f(__clang_svfloat32x2_t) { }
  void f(__clang_svfloat64x2_t) { }
  void f(__clang_svint8x3_t) { }
  void f(__clang_svint16x3_t) { }
  void f(__clang_svint32x3_t) { }
  void f(__clang_svint64x3_t) { }
  void f(__clang_svuint8x3_t) { }
  void f(__clang_svuint16x3_t) { }
  void f(__clang_svuint32x3_t) { }
  void f(__clang_svuint64x3_t) { }
  void f(__clang_svfloat16x3_t) { }
  void f(__clang_svfloat32x3_t) { }
  void f(__clang_svfloat64x3_t) { }
  void f(__clang_svint8x4_t) { }
  void f(__clang_svint16x4_t) { }
  void f(__clang_svint32x4_t) { }
  void f(__clang_svint64x4_t) { }
  void f(__clang_svuint8x4_t) { }
  void f(__clang_svuint16x4_t) { }
  void f(__clang_svuint32x4_t) { }
  void f(__clang_svuint64x4_t) { }
  void f(__clang_svfloat16x4_t) { }
  void f(__clang_svfloat32x4_t) { }
  void f(__clang_svfloat64x4_t) { }
  void f(__clang_svbfloat16x2_t) { }
  void f(__clang_svbfloat16x3_t) { }
  void f(__clang_svbfloat16x4_t) { }

  void f(__SVBool_t) { }
  void f(__clang_svboolx2_t) { }
  void f(__clang_svboolx4_t) { }
  void f(__SVCount_t) { }
}

/* @filter
func-scope variable
  source_corresp/enclosing_routine, type
 */
