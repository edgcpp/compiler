/**
 * @file test_stl_eh_hdr.h
 * @brief Shared cross-TU declarations for native STL exception interop and VMI/diamond catch tests.
 * @details Declares cross-TU RAII unwinding helpers, multiple-inheritance (`flags == 0`), and
 * virtual diamond-inheritance (`flags == 2` and `3`) exception hierarchies linked against real `libc++`/`libstdc++`.
 * @ingroup gcc_gen_be_tests
 */

#ifndef GCC_BE_TEST_STL_EH_HDR_H
#define GCC_BE_TEST_STL_EH_HDR_H

#include "test_common.h"

/**
 * @brief First base in ordinary multiple inheritance (`__vmi_class_type_info` `flags == 0`).
 */
struct MiLeft {
    /** @brief Left payload field at offset 0. */
    long left_val;
    /**
     * @brief Constructs `MiLeft`.
     * @details Initializes `left_val` to `v`.
     * @param[in] v Initial value.
     */
    explicit MiLeft(long v) noexcept : left_val(v) {}
    /**
     * @brief Virtual destructor for `MiLeft`.
     * @details Enables polymorphic `_ZTI` generation for `MiLeft`.
     */
    virtual ~MiLeft() noexcept {}
};

/**
 * @brief Second base in ordinary multiple inheritance (`non-zero` base subobject offset!).
 */
struct MiRight {
    /** @brief Right payload field at non-zero offset inside `MiCombined`. */
    long right_val;
    /**
     * @brief Constructs `MiRight`.
     * @details Initializes `right_val` to `v`.
     * @param[in] v Initial value.
     */
    explicit MiRight(long v) noexcept : right_val(v) {}
    /**
     * @brief Virtual destructor for `MiRight`.
     * @details Enables polymorphic `_ZTI` generation for `MiRight`.
     */
    virtual ~MiRight() noexcept {}
};

/**
 * @brief Multiple-inheritance exception class (`MiLeft` + `MiRight`, `flags == 0`).
 */
struct MiCombined : public MiLeft, public MiRight {
    /** @brief Extra derived tag value. */
    long extra;
    /**
     * @brief Constructs `MiCombined`.
     * @details Initializes `MiLeft(l)`, `MiRight(r)`, and `extra(e)`.
     * @param[in] l Left value.
     * @param[in] r Right value.
     * @param[in] e Extra value.
     */
    MiCombined(long l, long r, long e) noexcept : MiLeft(l), MiRight(r), extra(e) {}
    /**
     * @brief Virtual destructor for `MiCombined`.
     * @details Emits `__vmi_class_type_info` with `flags == 0` and two direct bases.
     */
    ~MiCombined() noexcept override {}
};

/**
 * @brief Virtual root base for diamond inheritance (`__vmi_class_type_info` `flags == 2`).
 */
struct VRoot {
    /** @brief Shared virtual base value. */
    long root_magic;
    /**
     * @brief Constructs `VRoot`.
     * @details Initializes `root_magic` to `m`.
     * @param[in] m Magic value (`0` default).
     */
    explicit VRoot(long m = 0) noexcept : root_magic(m) {}
    /**
     * @brief Virtual destructor for `VRoot`.
     * @details Enables virtual base offset lookup through the active vtable.
     */
    virtual ~VRoot() noexcept {}
};

/**
 * @brief Left arm of virtual diamond inheritance.
 */
struct VArmA : public virtual VRoot {
    /** @brief Left arm payload. */
    long arm_a;
    /**
     * @brief Constructs `VArmA`.
     * @details Initializes `VRoot(a)` and `arm_a(a)`.
     * @param[in] a Arm A value.
     */
    explicit VArmA(long a = 0) noexcept : VRoot(a), arm_a(a) {}
    /**
     * @brief Virtual destructor for `VArmA`.
     * @details Destroys the left virtual-inheritance arm cleanly.
     */
    ~VArmA() noexcept override {}
};

/**
 * @brief Right arm of virtual diamond inheritance.
 */
struct VArmB : public virtual VRoot {
    /** @brief Right arm payload. */
    long arm_b;
    /**
     * @brief Constructs `VArmB`.
     * @details Initializes `VRoot(b)` and `arm_b(b)`.
     * @param[in] b Arm B value.
     */
    explicit VArmB(long b = 0) noexcept : VRoot(b), arm_b(b) {}
    /**
     * @brief Virtual destructor for `VArmB`.
     * @details Destroys the right virtual-inheritance arm cleanly.
     */
    ~VArmB() noexcept override {}
};

/**
 * @brief Diamond-derived exception class (`flags == 2`) testing virtual base pointer adjustment in `catch (VRoot&)`.
 */
struct VDiamond : public VArmA, public VArmB {
    /** @brief Leaf value. */
    long leaf;
    /**
     * @brief Constructs `VDiamond`.
     * @details Initializes shared virtual `VRoot(r)` along with `VArmA(a)`, `VArmB(b)`, and `leaf(lf)`.
     * @param[in] r Root magic.
     * @param[in] a Arm A value.
     * @param[in] b Arm B value.
     * @param[in] lf Leaf value.
     */
    VDiamond(long r, long a, long b, long lf) noexcept : VRoot(r), VArmA(a), VArmB(b), leaf(lf) {}
    /**
     * @brief Virtual destructor for `VDiamond`.
     * @details Emits `_ZTT8VDiamond`, `_ZTC8VDiamond*`, and diamond `__vmi_class_type_info`.
     */
    ~VDiamond() noexcept override {}
};

/**
 * @brief Non-virtual base repeated along two distinct inheritance arms to trigger `__non_diamond_repeat_mask` (`0x1`).
 */
struct NonVirtRepeat {
    /** @brief Repeated non-virtual payload value. */
    long rep_val;
    /**
     * @brief Constructs `NonVirtRepeat`.
     * @details Initializes `rep_val` to `rv`.
     * @param[in] rv Repeated value.
     */
    explicit NonVirtRepeat(long rv = 0) noexcept : rep_val(rv) {}
    /**
     * @brief Virtual destructor for `NonVirtRepeat`.
     * @details Emits `_ZTI13NonVirtRepeat` for runtime catch matching.
     */
    virtual ~NonVirtRepeat() noexcept {}
};

/**
 * @brief Secondary carrier of `NonVirtRepeat` combined with `VDiamond` to produce `flags == 3`.
 */
struct RepeatCarrier : public NonVirtRepeat {
    /** @brief Carrier tag value. */
    long carrier_tag;
    /**
     * @brief Constructs `RepeatCarrier`.
     * @details Initializes `NonVirtRepeat(rv)` and `carrier_tag(ct)`.
     * @param[in] rv Repeated base value.
     * @param[in] ct Carrier tag value.
     */
    RepeatCarrier(long rv, long ct) noexcept : NonVirtRepeat(rv), carrier_tag(ct) {}
    /**
     * @brief Virtual destructor for `RepeatCarrier`.
     * @details Destroys `RepeatCarrier` cleanly.
     */
    ~RepeatCarrier() noexcept override {}
};

/**
 * @brief Combined diamond + non-diamond repeated inheritance class (`__vmi_class_type_info` `flags == 3`).
 * @details Inherits virtually from `VRoot` via `VDiamond` (`0x2` diamond mask) and non-virtually from
 * `NonVirtRepeat` twice (`NonVirtRepeat` direct + `RepeatCarrier`, `0x1` non-diamond repeat mask -> `flags == 3`).
 */
struct VDiamondPlusRepeat : public VDiamond, public NonVirtRepeat, public RepeatCarrier {
    /** @brief Leaf payload for `flags == 3` exception object. */
    long flags3_leaf;
    /**
     * @brief Constructs `VDiamondPlusRepeat`.
     * @details Initializes `VRoot(r)`, `VDiamond(r, a, b, lf)`, `NonVirtRepeat(r1)`, `RepeatCarrier(r2, ct)`, and `flags3_leaf(fl)`.
     * @param[in] r Virtual root magic value.
     * @param[in] a Left diamond arm value.
     * @param[in] b Right diamond arm value.
     * @param[in] lf Diamond leaf value.
     * @param[in] r1 First repeated non-virtual base value.
     * @param[in] r2 Second repeated non-virtual base value.
     * @param[in] ct Carrier tag value.
     * @param[in] fl Leaf tag value.
     */
    VDiamondPlusRepeat(long r, long a, long b, long lf, long r1, long r2, long ct, long fl) noexcept
        : VRoot(r), VDiamond(r, a, b, lf), NonVirtRepeat(r1), RepeatCarrier(r2, ct), flags3_leaf(fl) {}
    /**
     * @brief Virtual destructor for `VDiamondPlusRepeat`.
     * @details Emits `__vmi_class_type_info` with `flags == 3`.
     */
    ~VDiamondPlusRepeat() noexcept override {}
};

/**
 * @brief Throws a `MiCombined` exception from TU B through an intermediate RAII frame in TU B.
 * @details Appends destructor trace digits to `*inout_trace` as TU B unwinds into TU A's `catch`.
 * @param[in,out] inout_trace Pointer to RAII destruction order trace accumulator.
 * @return `GCC_GEN_BE_SUCCESS` if unreachable.
 */
gcc_gen_be_error_t throw_mi_across_tu(int *inout_trace) noexcept(false);

/**
 * @brief Throws a `VDiamond` exception from TU B through an intermediate RAII frame in TU B.
 * @details Exercises virtual base pointer adjustment (`flags == 2`) across TU boundaries.
 * @param[in,out] inout_trace Pointer to RAII destruction order trace accumulator.
 * @return `GCC_GEN_BE_SUCCESS` if unreachable.
 */
gcc_gen_be_error_t throw_diamond_across_tu(int *inout_trace) noexcept(false);

/**
 * @brief Throws a `VDiamondPlusRepeat` (`flags == 3`) exception from TU B through an RAII frame.
 * @details Exercises both ambiguous repeated non-virtual base rejection and unambiguous virtual `VRoot&` adjustment.
 * @param[in,out] inout_trace Pointer to RAII destruction order trace accumulator.
 * @return `GCC_GEN_BE_SUCCESS` if unreachable.
 */
gcc_gen_be_error_t throw_diamond_flags3_across_tu(int *inout_trace) noexcept(false);

#endif /* GCC_BE_TEST_STL_EH_HDR_H */
