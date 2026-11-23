#!/usr/bin/env python3
# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions. See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
"""Materializes the 216 cross-backend differential torture cases (12 x 18)."""

from __future__ import annotations

import json
import os
import re
import textwrap

NL = os.linesep
C_NL = chr(92) + "n"


def write_case(path: str, src: str) -> None:
    """Writes dedented source text to path, creating parent directories."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    body = textwrap.dedent(src).lstrip()
    body = re.sub(
        r'(printf\("[^"]*?)\r?\n\s*"',
        lambda m: m.group(1) + C_NL + '"',
        body,
    )
    with open(path, "w", encoding="utf-8") as f:
        f.write(body)
        if not body.endswith(NL):
            f.write(NL)


def build_cat01_int_bit(root: str) -> None:
    """Category 1: Integer & Bitwise Torture (18 cases)."""
    d = os.path.join(root, "cat01_int_bit")
    write_case(
        f"{d}/c01_int_widths.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      int8_t a = -120; uint8_t b = 250;
      int16_t c = -30000; uint16_t d = 60000;
      int32_t e = -2000000000; uint32_t f = 4000000000U;
      int64_t g = -9000000000000000000LL;
      uint64_t h = 18000000000000000000ULL;
      printf("%d %u %d %u %d %u %lld %llu
", a, b, c, d, e, f,
             (long long)(g / 7), (unsigned long long)(h / 13));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c02_int128_arith.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      unsigned __int128 u = ((unsigned __int128)0x123456789ABCDEF0ULL << 64)
                            | 0xFEDCBA9876543210ULL;
      unsigned __int128 q = u / 97U;
      unsigned __int128 r = u % 97U;
      __int128 s = -(__int128)(u >> 5);
      __int128 sq = s / -53;
      printf("%llx:%llx:%llu:%llx
",
             (unsigned long long)(q >> 64), (unsigned long long)q,
             (unsigned long long)r, (unsigned long long)sq);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c03_shifts_signed_unsigned.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      int64_t s = -0x123456789ABCLL;
      uint64_t u = 0xFEDCBA9876543210ULL;
      uint64_t acc = 0;
      for (int k = 1; k < 63; k += 5) {
        acc ^= (uint64_t)(s >> k) + (u >> k) + (u << (64 - k));
      }
      printf("%llx
", (unsigned long long)acc);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c04_bitfields_signed_unsigned.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    struct BF {
      signed int a:5;
      unsigned int b:7;
      signed int c:11;
      unsigned int :0;
      signed int d:9;
      unsigned long long e:37;
    };
    int main(void) {
      struct BF x = {-13, 109, -900, -211, 0x1ABCDEF123ULL};
      x.a += 4; x.b ^= 0x3AU; x.c -= 23; x.d += 17; x.e += 99ULL;
      printf("%d %u %d %d %llu
", x.a, x.b, x.c, x.d,
             (unsigned long long)x.e);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c05_packed_bitfields.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    struct __attribute__((packed)) PBF {
      uint8_t tag;
      uint32_t f1:13;
      int32_t f2:17;
      uint64_t f3:35;
    };
    int main(void) {
      struct PBF p = {7, 5001, -40000, 0x56789ABCDULL};
      p.f1 += 19; p.f2 += 1234; p.f3 ^= 0x111111111ULL;
      printf("%u %u %d %llu %zu
", p.tag, p.f1, p.f2,
             (unsigned long long)p.f3, sizeof(struct PBF));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c06_unsigned_wraparound.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      uint8_t u8 = 250U; u8 += 19U;
      uint16_t u16 = 65530U; u16 *= 37U;
      uint32_t u32 = 0xFFFFFFF0U; u32 = u32 * 2654435761U + 12345U;
      uint64_t u64 = 0xFFFFFFFFFFFFFF00ULL; u64 = u64 * 11400714819323198485ULL;
      printf("%u %u %u %llx
", u8, u16, u32, (unsigned long long)u64);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c07_bit_clz_ctz.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      uint32_t v = 0x001A8000U;
      uint64_t w = 0x00000F0012300000ULL;
      printf("%d %d %d %d
", __builtin_clz(v), __builtin_ctz(v),
             __builtin_clzll(w), __builtin_ctzll(w));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c08_bit_popcount_parity_ffs.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      uint32_t v = 0xA5A5F00DU;
      uint64_t w = 0x123456789ABCDEF0ULL;
      printf("%d %d %d %d %d %d
", __builtin_popcount(v),
             __builtin_parity(v), __builtin_ffs((int)v),
             __builtin_popcountll(w), __builtin_parityll(w),
             __builtin_ffsll((long long)w));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c09_bswap_widths.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      uint16_t a = 0x1234U;
      uint32_t b = 0x12345678U;
      uint64_t c = 0x0123456789ABCDEFULL;
      printf("%x %x %llx
", __builtin_bswap16(a), __builtin_bswap32(b),
             (unsigned long long)__builtin_bswap64(c));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c10_enum_underlying.cpp",
        r"""
    #include <cstdint>
    #include <cstdio>
    enum class Small : int8_t { Neg = -100, Zero = 0, Pos = 110 };
    enum class Wide : uint64_t { Big = 0xFEDCBA9876543210ULL };
    int main() {
      Small s = Small::Neg;
      Wide w = Wide::Big;
      std::printf("%d %llx
", static_cast<int>(s),
                  static_cast<unsigned long long>(w));
      return 0;
    }
    """,
    )
    for k in range(11, 19):
        mult = (13 + k * 7) * 100003
        modv = 997 + k * 31
        bias = k * 17
        write_case(
            f"{d}/c{k:02d}_divmod_matrix_{k}.c",
            rf"""
      #include <stdint.h>
      #include <stdio.h>
      int main(void) {{
        int64_t acc_q = 0, acc_r = 0;
        for (int64_t i = -250; i <= 250; ++i) {{
          int64_t n = i * {mult}LL + {bias};
          int64_t dv = (i % 2 == 0) ? {modv}LL : -{modv}LL;
          acc_q += n / dv;
          acc_r += n % dv;
        }}
        printf("%lld:%lld
", (long long)acc_q, (long long)acc_r);
        return 0;
      }}
      """,
        )


def build_cat02_fp_complex(root: str) -> None:
    """Category 2: Floating-Point & Complex Torture (18 cases)."""
    d = os.path.join(root, "cat02_fp_complex")
    write_case(
        f"{d}/c01_fp_basic_casts.c",
        r"""
    #include <math.h>
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      float f = 123.75f; double db = -987654.125; long double ld = 42.5L;
      int64_t i1 = (int64_t)db; uint64_t u1 = (uint64_t)(f * 1000.0f);
      printf("%lld %llu %.3f %.3f
", (long long)i1,
             (unsigned long long)u1, (double)(f + (float)ld), db * 2.0);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c02_ieee_nan_inf_signbit.c",
        r"""
    #include <math.h>
    #include <stdio.h>
    int main(void) {
      volatile double z = 0.0;
      double pinf = 1.0 / z, ninf = -1.0 / z, nanv = z / z;
      double nzero = copysign(0.0, -1.0);
      printf("%d %d %d %d %d %d
", isinf(pinf) != 0, isinf(ninf) != 0,
             isnan(nanv) != 0, signbit(nzero) != 0, !(nanv == nanv),
             nanv != nanv);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c03_c99_complex_double.c",
        r"""
    #include <complex.h>
    #include <stdio.h>
    int main(void) {
      double _Complex a = 3.0 + 4.0 * I;
      double _Complex b = 1.0 - 2.0 * I;
      double _Complex s = a + b, p = a * b, q = a / b;
      printf("%.2f %.2f | %.2f %.2f | %.2f %.2f
",
             creal(s), cimag(s), creal(p), cimag(p), creal(q), cimag(q));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c04_c99_complex_float.c",
        r"""
    #include <complex.h>
    #include <stdio.h>
    int main(void) {
      float _Complex a = 2.5f + 1.5f * I;
      float _Complex b = -0.5f + 2.0f * I;
      float _Complex prod = a * b;
      printf("%.2f %.2f
", (double)crealf(prod), (double)cimagf(prod));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c05_std_complex_cxx.cpp",
        r"""
    #include <complex>
    #include <cstdio>
    int main() {
      std::complex<double> x(5.0, -12.0), y(3.0, 4.0);
      auto z = (x * y) + (x / y);
      std::printf("%.2f %.2f %.2f
", z.real(), z.imag(), std::abs(x));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c06_math_intrinsics.c",
        r"""
    #include <math.h>
    #include <stdio.h>
    int main(void) {
      double x = 16.0, y = -3.75;
      printf("%.2f %.2f %.2f %.2f %.2f
",
             sqrt(x), fabs(y), floor(y), ceil(y), trunc(y));
      return 0;
    }
    """,
    )
    for k in range(7, 19):
        a = 1.25 + 0.5 * (k - 6)
        b = -0.75 * (k - 5)
        write_case(
            f"{d}/c{k:02d}_horner_poly_{k}.c",
            rf"""
      #include <math.h>
      #include <stdio.h>
      int main(void) {{
        double x = {a:.4f}, c = {b:.4f}, acc = 0.0;
        for (int i = 1; i <= 16; ++i) {{
          acc = acc * 0.5 + (x * (double)i - c) / (double)(i + 1);
        }}
        printf("%.6f
", acc);
        return 0;
      }}
      """,
        )


def build_cat03_abi_agg(root: str) -> None:
    """Category 3: Aggregate & Calling Convention Torture (18 cases)."""
    d = os.path.join(root, "cat03_abi_agg")
    sizes = [1, 3, 5, 7, 8, 9, 12, 15, 16, 24, 32, 48]
    for idx, sz in enumerate(sizes, 1):
        write_case(
            f"{d}/c{idx:02d}_struct_byval_sz{sz}.c",
            rf"""
      #include <stdint.h>
      #include <stdio.h>
      typedef struct {{ unsigned char b[{sz}]; }} S;
      __attribute__((noinline)) S transform(int seed, S in, S in2) {{
        S out;
        for (int i = 0; i < {sz}; ++i) {{
          out.b[i] = (unsigned char)(in.b[i] + in2.b[i] * 3 + seed + i);
        }}
        return out;
      }}
      int main(void) {{
        S a, b;
        for (int i = 0; i < {sz}; ++i) {{
          a.b[i] = (unsigned char)(i + 1);
          b.b[i] = (unsigned char)(2 * i + 3);
        }}
        S r = transform(11, a, b);
        unsigned sum = 0;
        for (int i = 0; i < {sz}; ++i) sum = sum * 31U + r.b[i];
        printf("%u
", sum);
        return 0;
      }}
      """,
        )
    write_case(
        f"{d}/c13_hfa_float4.c",
        r"""
    #include <stdio.h>
    typedef struct { float x, y, z, w; } Vec4f;
    __attribute__((noinline)) Vec4f lerp4(Vec4f a, Vec4f b, float t) {
      Vec4f r = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                 a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
      return r;
    }
    int main(void) {
      Vec4f u = {1.0f, 2.0f, 3.0f, 4.0f}, v = {5.0f, 10.0f, 15.0f, 20.0f};
      Vec4f w = lerp4(u, v, 0.25f);
      printf("%.2f %.2f %.2f %.2f
", (double)w.x, (double)w.y,
             (double)w.z, (double)w.w);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c14_hfa_double3.c",
        r"""
    #include <stdio.h>
    typedef struct { double x, y, z; } Vec3d;
    __attribute__((noinline)) Vec3d cross3(Vec3d a, Vec3d b) {
      Vec3d r = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
                 a.x * b.y - a.y * b.x};
      return r;
    }
    int main(void) {
      Vec3d a = {2.0, 3.0, 4.0}, b = {5.0, 6.0, 7.0};
      Vec3d c = cross3(a, b);
      printf("%.1f %.1f %.1f
", c.x, c.y, c.z);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c15_many_mixed_params_spill.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    __attribute__((noinline)) double mix12(
        int a1, double d1, int a2, double d2, int a3, double d3,
        int a4, double d4, int a5, double d5, int a6, double d6,
        int a7, int a8, int a9, int a10, double d7, double d8,
        double d9, double d10) {
      long isum = a1 + a2*2 + a3*3 + a4*4 + a5*5 + a6*6 + a7*7 + a8*8
                  + a9*9 + a10*10;
      double fsum = d1 + d2*2 + d3*3 + d4*4 + d5*5 + d6*6 + d7*7 + d8*8
                    + d9*9 + d10*10;
      return (double)isum + fsum;
    }
    int main(void) {
      double r = mix12(1,0.5,2,1.5,3,2.5,4,3.5,5,4.5,6,5.5,7,8,9,10,
                       6.5,7.5,8.5,9.5);
      printf("%.2f
", r);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c16_variadic_ints_doubles.c",
        r"""
    #include <stdarg.h>
    #include <stdio.h>
    __attribute__((noinline)) double vsum(int n, ...) {
      va_list ap, ap2;
      va_start(ap, n);
      va_copy(ap2, ap);
      double total = 0.0;
      for (int i = 0; i < n; ++i) {
        int iv = va_arg(ap, int);
        double dv = va_arg(ap, double);
        total += (double)iv + dv;
      }
      int first_i = va_arg(ap2, int);
      va_end(ap2);
      va_end(ap);
      return total + (double)first_i;
    }
    int main(void) {
      double r = vsum(6, 10, 0.25, 20, 0.5, 30, 0.75, 40, 1.0, 50, 1.25,
                      60, 1.5);
      printf("%.2f
", r);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c17_union_nested_agg.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    typedef union { uint64_t u64; struct { uint32_t lo, hi; } p; } U;
    typedef struct { int tag; U payload; } Packet;
    __attribute__((noinline)) Packet flip(Packet in) {
      uint32_t t = in.payload.p.lo;
      in.payload.p.lo = in.payload.p.hi;
      in.payload.p.hi = t;
      in.tag += 1;
      return in;
    }
    int main(void) {
      Packet pk; pk.tag = 4; pk.payload.p.lo = 0x11223344U;
      pk.payload.p.hi = 0x55667788U;
      Packet q = flip(pk);
      printf("%d %x %x
", q.tag, q.payload.p.lo, q.payload.p.hi);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c18_nontrivial_cxx_return_byval.cpp",
        r"""
    #include <cstdio>
    #include <string>
    struct Box {
      int id; std::string label;
      Box(int i, const char* s) : id(i), label(s) {}
      Box(const Box& o) : id(o.id + 100), label(o.label + "_cp") {}
      ~Box() {}
    };
    __attribute__((noinline)) Box make_box(Box in) {
      return Box(in.id + 1, in.label.c_str());
    }
    int main() {
      Box b0(5, "item");
      Box b1 = make_box(b0);
      std::printf("%d:%s
", b1.id, b1.label.c_str());
      return 0;
    }
    """,
    )


def build_cat04_control_flow(root: str) -> None:
    """Category 4: Control Flow Torture (18 cases)."""
    d = os.path.join(root, "cat04_control_flow")
    write_case(
        f"{d}/c01_dense_switch.c",
        r"""
    #include <stdio.h>
    int step(int x) {
      switch (x) {
        case 0: return 3; case 1: return 7; case 2: return 11;
        case 3: return 19; case 4: return 23; case 5: return 29;
        default: return -1;
      }
    }
    int main(void) {
      int s = 0; for (int i = -1; i <= 7; ++i) s = s * 5 + step(i);
      printf("%d
", s);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c02_sparse_switch_fallthrough.c",
        r"""
    #include <stdio.h>
    int classify(int v) {
      int r = 0;
      switch (v) {
        case -1000: r += 10; /* fallthrough */
        case 0: r += 5; break;
        case 4096: return 77;
        case 1000000: return 99;
        default: return v + 1;
      }
      return r;
    }
    int main(void) {
      printf("%d %d %d %d %d
", classify(-1000), classify(0),
             classify(4096), classify(1000000), classify(12));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c03_computed_goto_vm.c",
        r"""
    #include <stdio.h>
    int run_vm(const int *ops) {
      static const void * const tbl[] = {&&L_ADD, &&L_MUL, &&L_SUB, &&L_HALT};
      int pc = 0, acc = 5;
      goto *tbl[ops[pc++]];
    L_ADD: acc += ops[pc++]; goto *tbl[ops[pc++]];
    L_MUL: acc *= ops[pc++]; goto *tbl[ops[pc++]];
    L_SUB: acc -= ops[pc++]; goto *tbl[ops[pc++]];
    L_HALT: return acc;
    }
    int main(void) {
      const int prog[] = {0, 7, 1, 3, 2, 10, 3};
      printf("%d
", run_vm(prog));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c04_stmt_expr_nested.c",
        r"""
    #include <stdio.h>
    int main(void) {
      int base = 10;
      int res = ({ int a = base * 2; int b = ({ int t = a + 3; t * 2; }); a + b; });
      printf("%d
", res);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c05_goto_raii_cleanup.cpp",
        r"""
    #include <cstdio>
    static int log_v = 0;
    struct Guard {
      int id;
      Guard(int i):id(i){ log_v = log_v*10 + id; }
      ~Guard(){ log_v = log_v*10 + (id + 5); }
    };
    int main() {
      int n = 0;
      { Guard g1(1);
        { Guard g2(2);
          if (++n == 1) goto done;
        }
      }
    done:
      std::printf("%d
", log_v);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c06_ternary_lvalue_shortcircuit.cpp",
        r"""
    #include <cstdio>
    static int trace = 0;
    bool bump(int id, bool ret) { trace = trace * 10 + id; return ret; }
    int main() {
      int x = 10, y = 20;
      (true ? x : y) += 5;
      (false ? x : y) += 9;
      bool a = bump(1, false) && bump(2, true);
      bool b = bump(3, true) || bump(4, true);
      bool c = bump(5, true) && bump(6, true);
      std::printf("%d %d %d%d%d %d
", x, y, a, b, c, trace);
      return 0;
    }
    """,
    )
    for k in range(7, 19):
        lim = 8 + k
        write_case(
            f"{d}/c{k:02d}_nested_loops_break_continue_{k}.c",
            rf"""
      #include <stdio.h>
      int main(void) {{
        int total = 0, i = 0;
        while (i < {lim}) {{
          ++i;
          if (i % 3 == 0) continue;
          int j = 0;
          do {{
            ++j;
            if (i + j > {lim + 3}) break;
            total += i * 7 + j;
          }} while (j < {k});
        }}
        printf("%d
", total);
        return 0;
      }}
      """,
        )


def build_cat05_obj_model(root: str) -> None:
    """Category 5: C++ Object Model Torture (18 cases)."""
    d = os.path.join(root, "cat05_obj_model")
    write_case(
        f"{d}/c01_single_virtual_dispatch.cpp",
        r"""
    #include <cstdio>
    struct Base { virtual int eval(int x) const { return x + 1; }
                  virtual ~Base() = default; };
    struct Derived : Base { int eval(int x) const override { return x * 3; } };
    int run(const Base& b, int v) { return b.eval(v); }
    int main() {
      Base b; Derived d;
      std::printf("%d %d
", run(b, 5), run(d, 5));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c02_multiple_inheritance_this_adj.cpp",
        r"""
    #include <cstdio>
    struct A { int a = 10; virtual int fa() { return a; } virtual ~A()=default; };
    struct B { int b = 25; virtual int fb() { return b; } virtual ~B()=default; };
    struct C : A, B {
      int c = 7;
      int fa() override { return a + c; }
      int fb() override { return b + c * 2; }
    };
    int call_b(B* p) { return p->fb(); }
    int main() {
      C obj; A* pa = &obj; B* pb = &obj;
      std::printf("%d %d
", pa->fa(), call_b(pb));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c03_diamond_virtual_inheritance.cpp",
        r"""
    #include <cstdio>
    static int order = 0;
    struct V { int v; V(int x):v(x){ order = order*10 + 1; }
      virtual int get() const { return v; }
      virtual ~V(){ order = order*10 + 9; } };
    struct L : virtual V { int l; L():V(99),l(10){ order = order*10 + 2; }
      ~L(){ order = order*10 + 8; } };
    struct R : virtual V { int r; R():V(88),r(20){ order = order*10 + 3; }
      ~R(){ order = order*10 + 7; } };
    struct D : L, R { D():V(42){ order = order*10 + 4; }
      int get() const override { return v + l + r; }
      ~D(){ order = order*10 + 6; } };
    int main() {
      int val = 0;
      { D d; V* pv = &d; val = pv->get(); }
      std::printf("%d:%d
", val, order);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c04_covariant_return.cpp",
        r"""
    #include <cstdio>
    struct Left { int pad = 111; virtual ~Left() = default; };
    struct Base { int b = 22; virtual Base* self() { return this; }
                  virtual ~Base() = default; };
    struct Sub : Left, Base { int s = 33;
      Sub* self() override { return this; } };
    int main() {
      Sub x; Base* bp = &x; Base* r1 = bp->self();
      Sub* r2 = x.self();
      std::printf("%d %d %d
", r1->b, r2->s, r2->pad);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c05_deleting_destructor_d0.cpp",
        r"""
    #include <cstdio>
    static int dtor_trace = 0;
    struct Base { virtual ~Base() { dtor_trace = dtor_trace * 10 + 2; } };
    struct Child : Base { int x = 5;
      ~Child() override { dtor_trace = dtor_trace * 10 + 1; } };
    int main() {
      Base* p = new Child(); delete p;
      std::printf("%d
", dtor_trace);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c06_array_new_delete_cookie.cpp",
        r"""
    #include <cstdio>
    static int events = 0;
    struct Elem { int id;
      Elem() : id(++events) {}
      ~Elem() { events = events * 10 + id; } };
    int main() {
      Elem* arr = new Elem[3]; delete[] arr;
      std::printf("%d
", events);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c07_ptr_to_data_member.cpp",
        r"""
    #include <cstdio>
    struct S { int a; int b; int c; };
    int read_field(const S& s, int S::* pm) { return pm ? s.*pm : -1; }
    int main() {
      S s{11, 22, 33};
      int S::* p1 = &S::b;
      int S::* pnull = nullptr;
      std::printf("%d %d %d
", read_field(s, p1), read_field(s, pnull),
                  p1 != pnull);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c08_ptr_to_member_functions.cpp",
        r"""
    #include <cstdio>
    struct Base { int k = 4;
      int nonvirt(int x) const { return x + k; }
      virtual int virt(int x) const { return x * 2; }
      virtual ~Base() = default; };
    struct Der : Base {
      int virt(int x) const override { return x * 5 + k; } };
    int invoke(const Base* b, int (Base::*pmf)(int) const, int v) {
      return (b->*pmf)(v);
    }
    int main() {
      Der d;
      std::printf("%d %d
", invoke(&d, &Base::nonvirt, 10),
                  invoke(&d, &Base::virt, 10));
      return 0;
    }
    """,
    )
    for k in range(9, 19):
        bias = k * 3
        write_case(
            f"{d}/c{k:02d}_polymorphic_chain_{k}.cpp",
            rf"""
      #include <cstdio>
      struct L0 {{ virtual int compute(int x) const {{ return x + {bias}; }}
                   virtual ~L0() = default; }};
      struct L1 : L0 {{ int compute(int x) const override {{
        return L0::compute(x) * 2; }} }};
      struct L2 : L1 {{ int compute(int x) const override {{
        return L1::compute(x) - {k}; }} }};
      int main() {{
        L0 a; L1 b; L2 c; const L0* arr[3] = {{&a, &b, &c}};
        int sum = 0; for (int i = 0; i < 3; ++i) sum += arr[i]->compute(i + 4);
        std::printf("%d
", sum);
        return 0;
      }}
      """,
        )


def build_cat06_eh_cleanup(root: str) -> None:
    """Category 6: Exception Handling & Cleanup Torture (18 cases)."""
    d = os.path.join(root, "cat06_eh_cleanup")
    write_case(
        f"{d}/c01_throw_catch_int.cpp",
        r"""
    #include <cstdio>
    int risky(int x) { if (x < 0) throw 42; return x * 2; }
    int main() {
      int res = 0;
      try { res += risky(5); res += risky(-1); }
      catch (int e) { res += e; }
      std::printf("%d
", res);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c02_raii_lifo_unwind_multiframe.cpp",
        r"""
    #include <cstdio>
    static int log_seq = 0;
    struct Scoped { int id; Scoped(int i):id(i){ log_seq = log_seq*10 + id; }
      ~Scoped(){ log_seq = log_seq*10 + (id + 4); } };
    void inner() { Scoped s3(3); throw 99; }
    void middle() { Scoped s2(2); inner(); }
    int main() {
      try { Scoped s1(1); middle(); }
      catch (int v) { log_seq = log_seq * 100 + v; }
      std::printf("%d
", log_seq);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c03_catch_base_ref.cpp",
        r"""
    #include <cstdio>
    struct ErrBase { int code; ErrBase(int c):code(c){}
      virtual int val() const { return code; } virtual ~ErrBase()=default; };
    struct ErrSub : ErrBase { ErrSub(int c):ErrBase(c){}
      int val() const override { return code + 100; } };
    int main() {
      int got = 0;
      try { throw ErrSub(23); }
      catch (const ErrBase& e) { got = e.val(); }
      std::printf("%d
", got);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c04_rethrow_and_catch_all.cpp",
        r"""
    #include <cstdio>
    static int steps = 0;
    void inner() {
      try { throw 7; }
      catch (...) { steps = steps * 10 + 1; throw; }
    }
    int main() {
      try { inner(); }
      catch (int x) { steps = steps * 10 + x; }
      std::printf("%d
", steps);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c05_partial_ctor_subobject_unwind.cpp",
        r"""
    #include <cstdio>
    static int dtors = 0;
    struct SubA { SubA(){ dtors = dtors*10 + 1; }
                  ~SubA(){ dtors = dtors*10 + 8; } };
    struct SubB { SubB(){ dtors = dtors*10 + 2; }
                  ~SubB(){ dtors = dtors*10 + 7; } };
    struct Whole { SubA a; SubB b;
      Whole() { dtors = dtors*10 + 3; throw 55; }
      ~Whole() { dtors = dtors*10 + 9; } };
    int main() {
      try { Whole w; } catch (int e) { dtors = dtors*100 + e; }
      std::printf("%d
", dtors);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c06_nested_try_in_catch.cpp",
        r"""
    #include <cstdio>
    int main() {
      int total = 0;
      try {
        throw 10;
      } catch (int a) {
        total += a;
        try { throw 25; } catch (int b) { total += b; }
        total += 7;
      }
      std::printf("%d
", total);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c07_std_runtime_error_hierarchy.cpp",
        r"""
    #include <cstdio>
    #include <stdexcept>
    void fail(int mode) {
      if (mode == 1) throw std::out_of_range("oor");
      throw std::runtime_error("rt");
    }
    int main() {
      int score = 0;
      for (int m = 1; m <= 2; ++m) {
        try { fail(m); }
        catch (const std::logic_error&) { score += 10; }
        catch (const std::exception&) { score += 20; }
      }
      std::printf("%d
", score);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c08_catch_pointer_upcast.cpp",
        r"""
    #include <cstdio>
    struct B { int b = 14; virtual ~B() = default; };
    struct D : B { int d = 28; };
    int main() {
      D obj; int got = 0;
      try { D* p = &obj; throw p; }
      catch (const B* bp) { got = bp->b; }
      std::printf("%d
", got);
      return 0;
    }
    """,
    )
    for k in range(9, 19):
        code = k * 11
        write_case(
            f"{d}/c{k:02d}_unwind_loop_cleanup_{k}.cpp",
            rf"""
      #include <cstdio>
      static int tally = 0;
      struct Item {{ int v; Item(int x):v(x){{}} ~Item(){{ tally += v; }} }};
      int main() {{
        for (int i = 1; i <= 5; ++i) {{
          try {{
            Item a(i);
            if (i == 3) throw {code};
            tally += 100;
          }} catch (int e) {{
            tally += e;
          }}
        }}
        std::printf("%d
", tally);
        return 0;
      }}
      """,
        )


def build_cat07_rtti_dyncast(root: str) -> None:
    """Category 7: RTTI & Dynamic Cast Torture (18 cases)."""
    d = os.path.join(root, "cat07_rtti_dyncast")
    write_case(
        f"{d}/c01_typeid_polymorphic.cpp",
        r"""
    #include <cstdio>
    #include <typeinfo>
    struct Base { virtual ~Base() = default; };
    struct A : Base {}; struct B : Base {};
    int main() {
      A a; B b; Base* p1 = &a; Base* p2 = &b;
      std::printf("%d %d %d
", typeid(*p1) == typeid(A),
                  typeid(*p2) == typeid(B), typeid(*p1) != typeid(*p2));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c02_dyncast_downcast_and_fail.cpp",
        r"""
    #include <cstdio>
    struct Base { virtual ~Base() = default; int x = 1; };
    struct D1 : Base { int y = 42; };
    struct D2 : Base { int z = 99; };
    int main() {
      D1 d1; D2 d2; Base* b1 = &d1; Base* b2 = &d2;
      D1* ok = dynamic_cast<D1*>(b1);
      D1* no = dynamic_cast<D1*>(b2);
      std::printf("%d %d
", ok ? ok->y : -1, no == nullptr);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c03_dyncast_sibling_crosscast.cpp",
        r"""
    #include <cstdio>
    struct Left { int l = 15; virtual ~Left() = default; };
    struct Right { int r = 27; virtual ~Right() = default; };
    struct Both : Left, Right { int m = 3; };
    int main() {
      Both obj; Left* lp = &obj;
      Right* rp = dynamic_cast<Right*>(lp);
      void* vp = dynamic_cast<void*>(rp);
      std::printf("%d %d
", rp ? rp->r : -1, vp == static_cast<void*>(&obj));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c04_dyncast_diamond_virtual.cpp",
        r"""
    #include <cstdio>
    struct V { int v = 9; virtual ~V() = default; };
    struct A : virtual V { int a = 10; };
    struct B : virtual V { int b = 20; };
    struct Join : A, B { int j = 30; };
    int main() {
      Join j; V* vp = &j; A* ap = dynamic_cast<A*>(vp);
      B* bp = dynamic_cast<B*>(ap);
      std::printf("%d %d
", ap ? ap->a : -1, bp ? bp->b : -1);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c05_dyncast_ref_bad_cast.cpp",
        r"""
    #include <cstdio>
    #include <typeinfo>
    struct Base { virtual ~Base() = default; };
    struct D1 : Base { int v = 7; };
    struct D2 : Base { int w = 8; };
    int main() {
      D1 d1; Base& br = d1; int res = 0;
      res += dynamic_cast<D1&>(br).v;
      try { (void)dynamic_cast<D2&>(br); }
      catch (const std::bad_cast&) { res += 100; }
      std::printf("%d
", res);
      return 0;
    }
    """,
    )
    for k in range(6, 19):
        tag = k * 5
        write_case(
            f"{d}/c{k:02d}_rtti_dispatch_table_{k}.cpp",
            rf"""
      #include <cstdio>
      #include <typeinfo>
      struct Root {{ virtual ~Root() = default; }};
      struct KindA : Root {{ int a = {tag}; }};
      struct KindB : Root {{ int b = {tag + 3}; }};
      struct KindAB : KindA, KindB {{}};
      int inspect(Root* p) {{
        if (auto* a = dynamic_cast<KindA*>(p)) return a->a;
        if (auto* b = dynamic_cast<KindB*>(p)) return b->b;
        return -1;
      }}
      int main() {{
        KindA ka; KindB kb; KindAB kab;
        std::printf("%d %d %d
", inspect(&ka), inspect(&kb),
                    dynamic_cast<KindB*>(static_cast<KindA*>(&kab))->b);
        return 0;
      }}
      """,
        )


def build_cat08_templates_lambdas(root: str) -> None:
    """Category 8: Templates, Lambdas & Constexpr Torture (18 cases)."""
    d = os.path.join(root, "cat08_templates_lambdas")
    write_case(
        f"{d}/c01_variadic_fold_exprs.cpp",
        r"""
    #include <cstdio>
    template <typename... Ts> constexpr auto sum_all(Ts... xs) {
      return (0 + ... + xs);
    }
    template <typename... Ts> int comma_fold(Ts... xs) {
      int acc = 1; ((acc = acc * 3 + xs), ...); return acc;
    }
    int main() {
      std::printf("%d %d
", sum_all(1, 2, 3, 4, 5), comma_fold(2, 4, 6));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c02_concepts_requires_overload.cpp",
        r"""
    #include <concepts>
    #include <cstdio>
    template <std::integral T> int score(T v) { return static_cast<int>(v * 2); }
    template <std::floating_point T> int score(T v) {
      return static_cast<int>(v * 10.0);
    }
    int main() {
      std::printf("%d %d
", score(21), score(3.5));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c03_generic_init_mutable_lambdas.cpp",
        r"""
    #include <cstdio>
    int main() {
      int ext = 10;
      auto gen = [acc = 5, &ext](auto step) mutable {
        acc += static_cast<int>(step);
        ext += acc;
        return acc;
      };
      int a = gen(3), b = gen(4.0);
      int (*fp)(int) = +[](int x) { return x * x + 1; };
      std::printf("%d %d %d %d
", a, b, ext, fp(6));
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c04_if_constexpr_structured_bindings.cpp",
        r"""
    #include <cstdio>
    #include <tuple>
    #include <type_traits>
    template <typename T> int norm(T x) {
      if constexpr (std::is_pointer_v<T>) return *x + 100;
      else return x + 1;
    }
    struct Pair { int first; int second; };
    int main() {
      int v = 9; Pair p{14, 28}; auto [x, y] = p;
      std::printf("%d %d %d
", norm(v), norm(&v), x + y);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c05_spaceship_three_way.cpp",
        r"""
    #include <compare>
    #include <cstdio>
    struct Key { int hi; int lo;
      auto operator<=>(const Key&) const = default; };
    int main() {
      Key a{1, 5}, b{1, 9}, c{1, 5};
      std::printf("%d %d %d
", a < b, b > c, a == c);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c06_constexpr_sieve.cpp",
        r"""
    #include <array>
    #include <cstdio>
    constexpr int count_primes_upto_50() {
      std::array<bool, 51> comp{};
      int cnt = 0;
      for (int i = 2; i <= 50; ++i) {
        if (!comp[i]) {
          ++cnt;
          for (int j = i * i; j <= 50; j += i) comp[j] = true;
        }
      }
      return cnt;
    }
    int main() {
      constexpr int n = count_primes_upto_50();
      std::printf("%d
", n);
      return 0;
    }
    """,
    )
    for k in range(7, 19):
        n = 4 + (k % 7)
        write_case(
            f"{d}/c{k:02d}_template_metaprog_{k}.cpp",
            rf"""
      #include <cstdio>
      template <int N, int K> struct Binom {{
        static constexpr int value = Binom<N-1, K-1>::value + Binom<N-1, K>::value;
      }};
      template <int N> struct Binom<N, 0> {{ static constexpr int value = 1; }};
      template <int N> struct Binom<N, N> {{ static constexpr int value = 1; }};
      int main() {{
        auto f = [bias = {k}](auto v) {{ return v * 2 + bias; }};
        std::printf("%d
", f(Binom<{n + 4}, {n // 2 + 1}>::value));
        return 0;
      }}
      """,
        )


def build_cat09_coroutines(root: str) -> None:
    """Category 9: C++20 Coroutines Torture (18 cases)."""
    d = os.path.join(root, "cat09_coroutines")
    gen_hdr = r"""
    #include <coroutine>
    #include <cstdio>
    #include <exception>
    struct IntGen {
      struct promise_type {
        int cur = 0;
        IntGen get_return_object() {
          return IntGen{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(int v) noexcept { cur = v; return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { std::terminate(); }
      };
      std::coroutine_handle<promise_type> h;
      explicit IntGen(std::coroutine_handle<promise_type> hh) : h(hh) {}
      ~IntGen() { if (h) h.destroy(); }
      bool next(int* out) {
        h.resume();
        if (h.done()) return false;
        *out = h.promise().cur;
        return true;
      }
    };
  """
    write_case(
        f"{d}/c01_coro_fibonacci_yield.cpp",
        gen_hdr
        + r"""
    IntGen fib(int count) {
      int a = 1, b = 1;
      for (int i = 0; i < count; ++i) {
        co_yield a;
        int nxt = a + b; a = b; b = nxt;
      }
    }
    int main() {
      IntGen g = fib(8); int v = 0, sum = 0;
      while (g.next(&v)) sum = sum * 3 + v;
      std::printf("%d
", sum);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c02_coro_return_value_task.cpp",
        r"""
    #include <coroutine>
    #include <cstdio>
    #include <exception>
    struct Task {
      struct promise_type {
        int result = 0;
        Task get_return_object() {
          return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_never initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(int v) noexcept { result = v; }
        void unhandled_exception() { std::terminate(); }
      };
      std::coroutine_handle<promise_type> h;
      ~Task() { if (h) h.destroy(); }
    };
    struct DoubleAwait {
      int x;
      bool await_ready() const noexcept { return false; }
      bool await_suspend(std::coroutine_handle<>) const noexcept { return false; }
      int await_resume() const noexcept { return x * 2; }
    };
    Task compute(int base) {
      int a = co_await DoubleAwait{base};
      int b = co_await DoubleAwait{a + 3};
      co_return a + b;
    }
    int main() {
      Task t = compute(10);
      std::printf("%d
", t.h.promise().result);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c03_coro_raii_frame_destroy.cpp",
        gen_hdr
        + r"""
    static int log_events = 0;
    struct FrameGuard { int id;
      FrameGuard(int i):id(i){ log_events = log_events*10 + id; }
      ~FrameGuard(){ log_events = log_events*10 + (id + 5); } };
    IntGen guarded_seq() {
      FrameGuard g1(1);
      co_yield 10;
      FrameGuard g2(2);
      co_yield 20;
    }
    int main() {
      { IntGen g = guarded_seq(); int x = 0; g.next(&x); }
      std::printf("%d
", log_events);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c04_coro_unhandled_exception.cpp",
        r"""
    #include <coroutine>
    #include <cstdio>
    #include <exception>
    struct CatchTask {
      struct promise_type {
        int caught = 0;
        CatchTask get_return_object() {
          return CatchTask{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() noexcept {
          try { std::rethrow_exception(std::current_exception()); }
          catch (int v) { caught = v; }
        }
      };
      std::coroutine_handle<promise_type> h;
      ~CatchTask() { if (h) h.destroy(); }
    };
    CatchTask boom(int v) { if (v > 0) throw v; co_return; }
    int main() {
      CatchTask ct = boom(77); ct.h.resume();
      std::printf("%d:%d
", ct.h.done(), ct.h.promise().caught);
      return 0;
    }
    """,
    )
    for k in range(5, 19):
        step = k + 2
        write_case(
            f"{d}/c{k:02d}_coro_stepper_{k}.cpp",
            gen_hdr
            + rf"""
      IntGen seq_{k}(int start, int n) {{
        int cur = start;
        for (int i = 0; i < n; ++i) {{
          co_yield cur;
          cur = (cur * 3 + {step}) % 1009;
        }}
      }}
      int main() {{
        IntGen g = seq_{k}({k}, 7);
        int x = 0, total = 0;
        while (g.next(&x)) total += x;
        std::printf("%d
", total);
        return 0;
      }}
      """,
        )


def build_cat10_atomics_tls(root: str) -> None:
    """Category 10: Atomics & Threading Torture (18 cases)."""
    d = os.path.join(root, "cat10_atomics_tls")
    write_case(
        f"{d}/c01_atomic_builtins_rmw.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      int32_t v = 10;
      int32_t old1 = __atomic_fetch_add(&v, 5, __ATOMIC_SEQ_CST);
      int32_t new2 = __atomic_add_fetch(&v, 7, __ATOMIC_ACQ_REL);
      int32_t old3 = __atomic_exchange_n(&v, 100, __ATOMIC_SEQ_CST);
      int32_t exp = 100;
      int ok = __atomic_compare_exchange_n(&v, &exp, 250, 0,
                                           __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
      int32_t fin = __atomic_load_n(&v, __ATOMIC_ACQUIRE);
      printf("%d %d %d %d %d
", old1, new2, old3, ok, fin);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c02_atomic_bitwise_64.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      uint64_t x = 0xF0F0ULL;
      __atomic_fetch_or(&x, 0x0F00ULL, __ATOMIC_SEQ_CST);
      __atomic_fetch_and(&x, 0xFF33ULL, __ATOMIC_SEQ_CST);
      __atomic_fetch_xor(&x, 0x1111ULL, __ATOMIC_SEQ_CST);
      uint64_t r = __atomic_sub_fetch(&x, 0x10ULL, __ATOMIC_SEQ_CST);
      printf("%llx
", (unsigned long long)r);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c03_std_atomic_multithread.cpp",
        r"""
    #include <atomic>
    #include <cstdio>
    #include <thread>
    #include <vector>
    int main() {
      std::atomic<int> counter{0};
      std::vector<std::thread> ts;
      for (int t = 0; t < 4; ++t) {
        ts.emplace_back([&counter]() {
          for (int i = 0; i < 500; ++i) counter.fetch_add(1, std::memory_order_relaxed);
        });
      }
      for (auto& th : ts) th.join();
      std::printf("%d
", counter.load());
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c04_thread_local_isolation.cpp",
        r"""
    #include <atomic>
    #include <cstdio>
    #include <thread>
    thread_local int tls_val = 7;
    int main() {
      std::atomic<int> sum{0};
      std::thread t1([&]() { tls_val += 10; sum.fetch_add(tls_val); });
      std::thread t2([&]() { tls_val += 20; sum.fetch_add(tls_val); });
      t1.join(); t2.join();
      std::printf("%d:%d
", tls_val, sum.load());
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c05_mutex_lock_guard_threads.cpp",
        r"""
    #include <cstdio>
    #include <mutex>
    #include <thread>
    #include <vector>
    int main() {
      std::mutex mu; int shared_total = 0;
      std::vector<std::thread> pool;
      for (int w = 1; w <= 4; ++w) {
        pool.emplace_back([&, w]() {
          for (int k = 0; k < 100; ++k) {
            std::lock_guard<std::mutex> lk(mu);
            shared_total += w;
          }
        });
      }
      for (auto& t : pool) t.join();
      std::printf("%d
", shared_total);
      return 0;
    }
    """,
    )
    for k in range(6, 19):
        init_v = k * 9
        write_case(
            f"{d}/c{k:02d}_cas_retry_loop_{k}.cpp",
            rf"""
      #include <atomic>
      #include <cstdio>
      int main() {{
        std::atomic<int> state{{{init_v}}};
        for (int i = 1; i <= 10; ++i) {{
          int cur = state.load();
          while (!state.compare_exchange_weak(cur, cur + i * {k})) {{}}
        }}
        std::printf("%d
", state.load());
        return 0;
      }}
      """,
        )


def build_cat11_gnu_ext(root: str) -> None:
    """Category 11: GNU/Clang Extensions Torture (18 cases)."""
    d = os.path.join(root, "cat11_gnu_ext")
    write_case(
        f"{d}/c01_attr_cleanup_lifo.c",
        r"""
    #include <stdio.h>
    static int logv = 0;
    static void clean_int(int *p) { logv = logv * 10 + *p; }
    int run(int flag) {
      __attribute__((cleanup(clean_int))) int a = 1;
      {
        __attribute__((cleanup(clean_int))) int b = 2;
        if (flag) return 9;
      }
      __attribute__((cleanup(clean_int))) int c = 3;
      return 5;
    }
    int main(void) {
      int r1 = run(1), r2 = run(0);
      printf("%d %d %d
", r1, r2, logv);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c02_attr_constructor_destructor.c",
        r"""
    #include <stdio.h>
    static int stage = 1;
    __attribute__((constructor)) static void init_fn(void) { stage = stage * 10 + 7; }
    int main(void) {
      printf("%d
", stage);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c03_attr_packed_aligned.c",
        r"""
    #include <stddef.h>
    #include <stdint.h>
    #include <stdio.h>
    struct __attribute__((packed)) PackedRec { char c; uint64_t q; uint16_t s; };
    struct __attribute__((aligned(64))) AlignedBuf { int x; };
    int main(void) {
      struct PackedRec r = {'A', 0x0102030405060708ULL, 0x1234U};
      struct AlignedBuf b = {42};
      printf("%zu %zu %zu %d %llx
", sizeof(r), _Alignof(struct PackedRec),
             _Alignof(struct AlignedBuf), b.x + r.c,
             (unsigned long long)r.q + r.s);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c04_gnu_vector_arith_cmp.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    typedef int32_t v4i __attribute__((vector_size(16)));
    int main(void) {
      v4i a = {1, 20, 3, 40};
      v4i b = {5, 10, 3, 50};
      v4i s = a + b;
      v4i eq = (a == b);
      v4i lt = (a < b);
      printf("%d %d %d %d | %d %d %d %d | %d %d %d %d
",
             s[0], s[1], s[2], s[3], eq[0], eq[1], eq[2], eq[3],
             lt[0], lt[1], lt[2], lt[3]);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c05_vla_sizeof_and_loop_stack.c",
        r"""
    #include <stdio.h>
    int vla_sum(int n) {
      int arr[n];
      for (int i = 0; i < n; ++i) arr[i] = (i + 1) * 3;
      int s = (int)(sizeof(arr) / sizeof(arr[0]));
      for (int i = 0; i < n; ++i) s += arr[i];
      return s;
    }
    int main(void) {
      int acc = 0;
      for (int k = 1; k <= 12; ++k) acc += vla_sum(k);
      printf("%d
", acc);
      return 0;
    }
    """,
    )
    write_case(
        f"{d}/c06_builtin_choose_types_compat.c",
        r"""
    #include <stdint.h>
    #include <stdio.h>
    int main(void) {
      int a = __builtin_choose_expr(
          __builtin_types_compatible_p(int, int32_t), 111, 222);
      int b = __builtin_choose_expr(
          __builtin_types_compatible_p(int, double), 111, 222);
      long c = __builtin_expect(a + b, 333);
      printf("%ld
", c);
      return 0;
    }
    """,
    )
    for k in range(7, 19):
        c0, c1, c2, c3 = k, k * 2, k * 3, k * 4
        write_case(
            f"{d}/c{k:02d}_gnu_vec_pipeline_{k}.c",
            rf"""
      #include <stdint.h>
      #include <stdio.h>
      typedef int32_t v4i __attribute__((vector_size(16)));
      int main(void) {{
        v4i x = {{{c0}, {c1}, {c2}, {c3}}};
        v4i y = {{4, 3, 2, 1}};
        v4i z = x * y - x;
        z[2] += 17;
        printf("%d:%d:%d:%d
", z[0], z[1], z[2], z[3]);
        return 0;
      }}
      """,
        )


def build_cat12_cross_abi(root: str) -> None:
    """Category 12: Cross-Backend Binary ABI Link Compatibility (18 manifests)."""
    base = os.path.join(root, "cat12_cross_abi")
    manifest_json = json.dumps(
        {
            "units": [
                {"src": "a.c", "backend": "llvm"},
                {"src": "b.c", "backend": "gcc"},
            ]
        },
        indent=2,
    )
    manifest_cxx_json = json.dumps(
        {
            "units": [
                {"src": "a.cpp", "backend": "llvm"},
                {"src": "b.cpp", "backend": "gcc"},
            ]
        },
        indent=2,
    )

    m01 = os.path.join(base, "x01_c_structs_hfa")
    write_case(
        f"{m01}/a.c",
        r"""
    #include <stdio.h>
    typedef struct { long a, b, c; } Big3;
    typedef struct { float x, y, z; } Hfa3;
    Big3 make_big3(long s);
    Hfa3 scale_hfa3(Hfa3 in, float f);
    int main(void) {
      Big3 b = make_big3(10);
      Hfa3 h = scale_hfa3((Hfa3){1.5f, 2.5f, 3.5f}, 2.0f);
      printf("%ld %.1f
", b.a + b.b + b.c, (double)(h.x + h.y + h.z));
      return 0;
    }
    """,
    )
    write_case(
        f"{m01}/b.c",
        r"""
    typedef struct { long a, b, c; } Big3;
    typedef struct { float x, y, z; } Hfa3;
    Big3 make_big3(long s) { Big3 r = {s, s * 2, s * 3}; return r; }
    Hfa3 scale_hfa3(Hfa3 in, float f) {
      Hfa3 r = {in.x * f, in.y * f, in.z * f}; return r;
    }
    """,
    )
    write_case(f"{m01}/case.manifest.json", manifest_json)

    m02 = os.path.join(base, "x02_cxx_stl_and_eh")
    write_case(
        f"{m02}/a.cpp",
        r"""
    #include <cstdio>
    #include <stdexcept>
    #include <string>
    #include <vector>
    std::string build_greeting(int n);
    std::vector<int> build_seq(int n);
    int main() {
      std::string s = build_greeting(5);
      auto v = build_seq(4);
      int sum = 0; for (int x : v) sum += x;
      try { (void)build_greeting(-1); }
      catch (const std::exception& e) {
        std::printf("%s|%d|%s
", s.c_str(), sum, e.what());
      }
      return 0;
    }
    """,
    )
    write_case(
        f"{m02}/b.cpp",
        r"""
    #include <stdexcept>
    #include <string>
    #include <vector>
    std::string build_greeting(int n) {
      if (n < 0) throw std::runtime_error("bad_n");
      return "hi_" + std::to_string(n);
    }
    std::vector<int> build_seq(int n) {
      std::vector<int> out;
      for (int i = 1; i <= n; ++i) out.push_back(i * 10);
      return out;
    }
    """,
    )
    write_case(f"{m02}/case.manifest.json", manifest_cxx_json)

    m03 = os.path.join(base, "x03_cross_vtable_rtti")
    write_case(
        f"{m03}/a.cpp",
        r"""
    #include <cstdio>
    struct Shape {
      virtual int area() const = 0;
      virtual ~Shape() = default;
    };
    struct Rect : Shape {
      int w, h;
      Rect(int ww, int hh) : w(ww), h(hh) {}
      int area() const override;
    };
    Shape* make_rect(int w, int h);
    int main() {
      Shape* s = make_rect(6, 7);
      Rect* r = dynamic_cast<Rect*>(s);
      std::printf("%d:%d
", s->area(), r ? r->w + r->h : -1);
      delete s;
      return 0;
    }
    """,
    )
    write_case(
        f"{m03}/b.cpp",
        r"""
    struct Shape {
      virtual int area() const = 0;
      virtual ~Shape() = default;
    };
    struct Rect : Shape {
      int w, h;
      Rect(int ww, int hh) : w(ww), h(hh) {}
      int area() const override;
    };
    int Rect::area() const { return w * h; }
    Shape* make_rect(int w, int h) { return new Rect(w, h); }
    """,
    )
    write_case(f"{m03}/case.manifest.json", manifest_cxx_json)

    m04 = os.path.join(base, "x04_cross_variadic")
    write_case(
        f"{m04}/a.c",
        r"""
    #include <stdio.h>
    double weighted_var(int n, ...);
    int main(void) {
      double v = weighted_var(4, 2, 1.5, 3, 2.5, 4, 3.5, 5, 4.5);
      printf("%.2f
", v);
      return 0;
    }
    """,
    )
    write_case(
        f"{m04}/b.c",
        r"""
    #include <stdarg.h>
    double weighted_var(int n, ...) {
      va_list ap; va_start(ap, n);
      double acc = 0.0;
      for (int i = 0; i < n; ++i) {
        int w = va_arg(ap, int);
        double x = va_arg(ap, double);
        acc += (double)w * x;
      }
      va_end(ap);
      return acc;
    }
    """,
    )
    write_case(f"{m04}/case.manifest.json", manifest_json)

    for k in range(5, 19):
        mk = os.path.join(base, f"x{k:02d}_cross_abi_mix_{k}")
        sz = 4 + (k % 6) * 5
        write_case(
            f"{mk}/a.cpp",
            rf"""
      #include <cstdio>
      struct Payload {{ unsigned char data[{sz}]; int tag; }};
      Payload step_payload(Payload in, int delta);
      int risky_div(int a, int b);
      int main() {{
        Payload p{{}}; p.tag = 3;
        for (int i = 0; i < {sz}; ++i) p.data[i] = static_cast<unsigned char>(i + {k});
        Payload q = step_payload(p, {k});
        int sum = q.tag;
        for (int i = 0; i < {sz}; ++i) sum += q.data[i];
        try {{ sum += risky_div(100, 0); }} catch (int e) {{ sum += e; }}
        std::printf("%d
", sum);
        return 0;
      }}
      """,
        )
        write_case(
            f"{mk}/b.cpp",
            rf"""
      struct Payload {{ unsigned char data[{sz}]; int tag; }};
      Payload step_payload(Payload in, int delta) {{
        for (int i = 0; i < {sz}; ++i) {{
          in.data[i] = static_cast<unsigned char>(in.data[i] + delta + i);
        }}
        in.tag += delta * 2;
        return in;
      }}
      int risky_div(int a, int b) {{
        if (b == 0) throw {k * 10 + 7};
        return a / b;
      }}
      """,
        )
        write_case(f"{mk}/case.manifest.json", manifest_cxx_json)


def build_stage2_completeness_cases(root: str) -> None:
    """Adds 16 C89-C23 & C++98-C++26 differential completeness cases."""
    write_case(
        os.path.join(root, "cat01_int_bit", "c19_zero_width_volatile_bitfields.c"),
        r"""
    /* DIFF_STD: -std=c11 */
    #include <stdio.h>
    struct RegBf {
      volatile unsigned a : 5;
      unsigned : 0;
      volatile signed int b : 7;
      unsigned long long strad : 20;
    };
    int main(void) {
      struct RegBf r = {19u, -23, 0xABCDEULL};
      r.a = (unsigned)(r.a + 3u);
      r.b = (signed int)(r.b - 5);
      printf("%u %d %llu %zu\n", (unsigned)r.a, (int)r.b,
             (unsigned long long)r.strad, sizeof(struct RegBf));
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(root, "cat01_int_bit", "c20_c23_bitint_arith.c"),
        r"""
    /* DIFF_STD: -std=c23 */
    #include <stdio.h>
    int main(void) {
      _BitInt(7) a = -15;
      unsigned _BitInt(24) b = 100000u;
      _BitInt(40) prod = (_BitInt(40))a * (_BitInt(40))b;
      unsigned _BitInt(48) sh = ((unsigned _BitInt(48))b << 12) | 0x3FFu;
      printf("%lld %llu\n", (long long)prod, (unsigned long long)sh);
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(root, "cat02_fp_complex", "c19_complex_abi_calls.c"),
        r"""
    /* DIFF_STD: -std=c11 */
    #include <complex.h>
    #include <stdio.h>
    __attribute__((noinline))
    double _Complex fma_cplx(double _Complex x, double _Complex y, double _Complex z) {
      return x * y + z;
    }
    __attribute__((noinline))
    float _Complex scale_fc(float _Complex v, float k) {
      return v * k;
    }
    int main(void) {
      double _Complex r = fma_cplx(2.0 + 3.0 * I, -1.5 + 4.0 * I, 0.5 - 1.0 * I);
      float _Complex f = scale_fc(1.25f - 2.5f * I, 2.0f);
      printf("%.2f %.2f %.2f %.2f\n", creal(r), cimag(r), (double)crealf(f), (double)cimagf(f));
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(root, "cat02_fp_complex", "c20_fixed_decimal_scaled.c"),
        r"""
    /* DIFF_STD: -std=c89 */
    #include <stdio.h>
    typedef long q16_16_t;
    static q16_16_t q_mul(q16_16_t a, q16_16_t b) {
      long hi = (a >> 16) * (b >> 16);
      unsigned long alo = (unsigned long)(a & 0xFFFFL);
      unsigned long blo = (unsigned long)(b & 0xFFFFL);
      long mid = ((a >> 16) * (long)blo) + ((b >> 16) * (long)alo);
      unsigned long lo = (alo * blo) >> 16;
      return (hi << 16) + mid + (long)lo;
    }
    int main(void) {
      q16_16_t x = (3L << 16) + 0x8000L; /* 3.5 */
      q16_16_t y = (2L << 16) + 0x4000L; /* 2.25 */
      q16_16_t z = q_mul(x, y);          /* 7.875 == 7 * 65536 + 57344 = 516096 */
      printf("%ld\n", z);
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(root, "cat03_abi_agg", "c19_va_arg_structs_hfa.c"),
        r"""
    /* DIFF_STD: -std=c11 */
    #include <stdarg.h>
    #include <stdio.h>
    typedef struct { int a; long b; } SmallS;
    typedef struct { long x, y, z, w; } BigS;
    typedef struct { float x, y, z; } HfaF3;
    typedef struct { double u, v; } HfaD2;
    __attribute__((noinline))
    long sum_va(int tag, ...) {
      va_list ap;
      va_start(ap, tag);
      SmallS s = va_arg(ap, SmallS);
      HfaF3 hf = va_arg(ap, HfaF3);
      BigS bg = va_arg(ap, BigS);
      HfaD2 hd = va_arg(ap, HfaD2);
      va_end(ap);
      return (long)tag + s.a + s.b + (long)(hf.x + hf.y + hf.z)
             + bg.x + bg.w + (long)(hd.u * 10.0 + hd.v * 10.0);
    }
    int main(void) {
      SmallS s = {11, 22};
      HfaF3 hf = {1.5f, 2.5f, 3.0f};
      BigS bg = {100, 200, 300, 400};
      HfaD2 hd = {1.25, 2.75};
      printf("%ld\n", sum_va(5, s, hf, bg, hd));
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(root, "cat04_control_flow", "c19_inline_asm_portable.c"),
        r"""
    #include <stdio.h>
    int main(void) {
      long v = 21;
    #if defined(__aarch64__)
      __asm__ volatile("add %0, %1, #21" : "=r"(v) : "r"(v));
    #elif defined(__x86_64__)
      __asm__ volatile("addq $21, %0" : "+r"(v));
    #else
      v += 21;
    #endif
      printf("%ld\n", v);
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(
            root, "cat05_obj_model", "c19_construction_vtable_dtor_d0_d1_d2.cpp"
        ),
        r"""
    #include <cstdio>
    static int g_log = 0;
    struct VRoot {
      int r = 10;
      virtual int tag() const { return 1; }
      VRoot() { g_log = g_log * 10 + tag(); }
      virtual ~VRoot() { g_log = g_log * 10 + tag(); }
    };
    struct Left : virtual VRoot {
      int l = 20;
      int tag() const override { return 2; }
      Left() { g_log = g_log * 10 + tag(); }
      ~Left() override { g_log = g_log * 10 + tag(); }
    };
    struct Right : virtual VRoot {
      int rg = 30;
      int tag() const override { return 3; }
      Right() { g_log = g_log * 10 + tag(); }
      ~Right() override { g_log = g_log * 10 + tag(); }
    };
    struct Join : Left, Right {
      int j = 40;
      int tag() const override { return 4; }
      Join() { g_log = g_log * 10 + tag(); }
      ~Join() override { g_log = g_log * 10 + tag(); }
    };
    int main() {
      VRoot* p = new Join();
      int Right::* pm_d = &Right::rg;
      int (VRoot::*pm_f)() const = &VRoot::tag;
      Join* jp = dynamic_cast<Join*>(p);
      int val = jp->*pm_d + (p->*pm_f)();
      delete p;
      std::printf("%d %d\n", val, g_log);
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(
            root, "cat06_eh_cleanup", "c19_multithread_eh_and_raii_scopes.cpp"
        ),
        r"""
    #include <atomic>
    #include <cstdio>
    #include <stdexcept>
    #include <thread>
    #include <vector>
    struct Guard {
      int* acc;
      int v;
      ~Guard() { *acc = *acc * 10 + v; }
    };
    static int scope_dance() {
      int acc = 0;
      for (int i = 0; i < 3; ++i) {
        Guard g{&acc, i + 1};
        if (i == 0) continue;
        if (i == 1) break;
      }
      {
        Guard g3{&acc, 7};
        goto after;
      }
    after:
      return acc;
    }
    int main() {
      std::atomic<int> total{0};
      std::vector<std::thread> ts;
      for (int t = 1; t <= 4; ++t) {
        ts.emplace_back([t, &total]() {
          try {
            try {
              throw std::runtime_error("boom");
            } catch (const std::exception&) {
              throw;
            }
          } catch (const std::runtime_error&) {
            total.fetch_add(t * 10, std::memory_order_relaxed);
          }
        });
      }
      for (auto& th : ts) th.join();
      std::printf("%d %d\n", scope_dance(), total.load());
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(
            root,
            "cat08_templates_lambdas",
            "c19_namespaces_nullptr_consteval_deducing_this.cpp",
        ),
        r"""
    // DIFF_STD: -std=c++23
    #include <cstddef>
    #include <cstdio>
    #include <source_location>
    namespace outer::inner {
      inline namespace v1 {
        constexpr int kBase = 7;
      }
    }
    namespace {
      int ovl(int* p) { return p ? *p : -1; }
      int ovl(std::nullptr_t) { return 42; }
    }
    struct PairNTTP { int a; int b; };
    template <PairNTTP P>
    consteval int sum_nttp() { return P.a * 10 + P.b; }
    struct Fib {
      template <typename Self>
      int calc(this Self&& self, int n) {
        return (n <= 1) ? n : self.calc(n - 1) + self.calc(n - 2);
      }
    };
    int cur_ln(std::source_location loc = std::source_location::current()) {
      return (int)loc.line();
    }
    int main() {
      Fib f{};
      int ln = cur_ln();
      std::printf("%d %d %d %d %d\n",
                  outer::inner::kBase, ovl(nullptr),
                  sum_nttp<PairNTTP{3, 9}>(), f.calc(8), ln);
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(root, "cat09_coroutines", "c19_coro_custom_new_and_caught_eh.cpp"),
        r"""
    #include <coroutine>
    #include <cstdio>
    #include <cstdlib>
    static int g_custom_new = 0;
    static int g_custom_del = 0;
    struct Task {
      struct promise_type {
        int result = 0;
        int failed = 0;
        static void* operator new(std::size_t sz) {
          ++g_custom_new;
          return std::malloc(sz);
        }
        static void operator delete(void* p) noexcept {
          ++g_custom_del;
          std::free(p);
        }
        Task get_return_object() {
          return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_value(int v) noexcept { result = v; }
        void unhandled_exception() noexcept { failed = 99; }
      };
      std::coroutine_handle<promise_type> h;
      ~Task() { if (h) h.destroy(); }
    };
    static Task run_coro(bool throw_escaped) {
      int acc = 10;
      try {
        co_await std::suspend_always{};
        throw 5;
      } catch (int x) {
        acc += x;
      }
      if (throw_escaped) throw 77;
      co_return acc;
    }
    int main() {
      int r1 = 0, f2 = 0;
      {
        Task t1 = run_coro(false);
        t1.h.resume();
        t1.h.resume();
        r1 = t1.h.promise().result;
        Task t2 = run_coro(true);
        t2.h.resume();
        t2.h.resume();
        f2 = t2.h.promise().failed;
      }
      std::printf("%d %d %d\n", r1, f2, g_custom_new == g_custom_del ? 1 : 0);
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(root, "cat10_atomics_tls", "c19_c11_atomic_scalars_and_structs.c"),
        r"""
    /* DIFF_STD: -std=c11 */
    #include <stdatomic.h>
    #include <stdio.h>
    struct Pair { int x; int y; };
    int main(void) {
      _Atomic int ai = 10;
      atomic_fetch_add(&ai, 5);
      _Atomic struct Pair ap;
      atomic_init(&ap, (struct Pair){3, 7});
      struct Pair cur = atomic_load(&ap);
      struct Pair nxt = {cur.x + 10, cur.y + 20};
      struct Pair old = atomic_exchange(&ap, nxt);
      struct Pair fin = atomic_load(&ap);
      printf("%d %d:%d %d:%d\n", atomic_load(&ai), old.x, old.y, fin.x, fin.y);
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(root, "cat10_atomics_tls", "c20_static_guard_and_tls_dtors.cpp"),
        r"""
    #include <atomic>
    #include <cstdio>
    #include <thread>
    #include <vector>
    static std::atomic<int> g_static_inits{0};
    static std::atomic<int> g_tls_dtors{0};
    struct HeavySingleton {
      int val;
      HeavySingleton() : val(77) { g_static_inits.fetch_add(1); }
    };
    struct TlsWatcher {
      int id = 1;
      ~TlsWatcher() { g_tls_dtors.fetch_add(id); }
    };
    static int get_singleton() {
      static HeavySingleton s;
      return s.val;
    }
    int main() {
      std::vector<std::thread> ts;
      int sum = 0;
      std::atomic<int> asum{0};
      for (int i = 0; i < 4; ++i) {
        ts.emplace_back([&asum]() {
          thread_local TlsWatcher w;
          w.id = 3;
          asum.fetch_add(get_singleton());
        });
      }
      for (auto& t : ts) t.join();
      sum = asum.load();
      std::printf("%d %d %d\n", sum, g_static_inits.load(), g_tls_dtors.load());
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(root, "cat11_gnu_ext", "c19_c11_generic_desig_fam_vmp.c"),
        r"""
    /* DIFF_STD: -std=c11 */
    #include <stdio.h>
    #include <stdlib.h>
    #define KIND_TAG(x) _Generic((x), int: 10, double: 20, default: 30)
    struct Packet {
      int count;
      int items[];
    };
    static int sum_vmp(int n, int (*mat)[n]) {
      return mat[0][n - 1] + mat[1][0];
    }
    int main(void) {
      int grid[2][4] = {[0] = {[3] = 15}, [1] = {[0] = 27}};
      struct Packet* p = (struct Packet*)malloc(sizeof(struct Packet) + 3 * sizeof(int));
      p->count = 3;
      int* src = (int[3]){4, 5, 6};
      for (int i = 0; i < 3; ++i) p->items[i] = src[i];
      int res = KIND_TAG(1) + KIND_TAG(2.5) + sum_vmp(4, grid)
                + p->items[0] + p->items[2];
      free(p);
      printf("%d\n", res);
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(root, "cat11_gnu_ext", "c20_cxx17_aligned_new_copy_elision.cpp"),
        r"""
    #include <cstdint>
    #include <cstdio>
    struct alignas(64) Aligned64 {
      int payload[8];
    };
    struct Immovable {
      int x;
      int y;
      Immovable(int a, int b) : x(a), y(b) {}
      Immovable(const Immovable&) = delete;
      Immovable(Immovable&&) = delete;
    };
    static Immovable make_immovable(int a, int b) {
      return Immovable(a + 1, b + 2);
    }
    int main() {
      Aligned64* p = new Aligned64{};
      p->payload[0] = 55;
      bool ok_align = (reinterpret_cast<std::uintptr_t>(p) % 64U) == 0U;
      Immovable im = make_immovable(10, 20);
      int total = p->payload[0] + im.x + im.y + (ok_align ? 1 : 0);
      delete p;
      std::printf("%d\n", total);
      return 0;
    }
    """,
    )
    write_case(
        os.path.join(root, "cat11_gnu_ext", "c21_openmp_parallel_reduction.cpp"),
        r"""
    // DIFF_FLAGS: -fopenmp
    #include <cstdio>
    int main() {
      long sum = 0;
      #pragma omp parallel for reduction(+:sum)
      for (int i = 1; i <= 1000; ++i) {
        sum += i;
      }
      std::printf("%ld\n", sum);
      return 0;
    }
    """,
    )
    mod_dir = os.path.join(root, "cat12_cross_abi", "x19_cxx20_modules")
    write_case(
        f"{mod_dir}/m_math.cppm",
        r"""
    export module m_math;
    export int triple_add(int a, int b) {
      return 3 * a + b;
    }
    export struct ModBox {
      int v;
      int scaled(int k) const { return v * k; }
    };
    """,
    )
    write_case(
        f"{mod_dir}/main.cpp",
        r"""
    import m_math;
    #include <cstdio>
    int main() {
      ModBox b{9};
      std::printf("%d %d\n", triple_add(10, 7), b.scaled(4));
      return 0;
    }
    """,
    )
    write_case(
        f"{mod_dir}/case.manifest.json",
        r"""
    {
      "kind": "cxx20_module",
      "mod_name": "m_math",
      "iface": "m_math.cppm",
      "main": "main.cpp"
    }
    """,
    )


def main() -> None:
    """Generates all 232 differential torture test cases."""
    cases_root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cases")
    build_cat01_int_bit(cases_root)
    build_cat02_fp_complex(cases_root)
    build_cat03_abi_agg(cases_root)
    build_cat04_control_flow(cases_root)
    build_cat05_obj_model(cases_root)
    build_cat06_eh_cleanup(cases_root)
    build_cat07_rtti_dyncast(cases_root)
    build_cat08_templates_lambdas(cases_root)
    build_cat09_coroutines(cases_root)
    build_cat10_atomics_tls(cases_root)
    build_cat11_gnu_ext(cases_root)
    build_cat12_cross_abi(cases_root)
    build_stage2_completeness_cases(cases_root)
    print("Populated 232 differential torture test cases under", cases_root)


if __name__ == "__main__":
    main()
