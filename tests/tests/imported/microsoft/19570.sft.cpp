//options_all:--microsoft_version 1914 --ms_c++17
// Test implementation of compiler intrinsic __is_aggregate, to support LWG 2911.
// __is_aggregate(T) returns true iff T is an aggregate type.
// remove_all_extents_t<T> shall be a complete type or cv void.
#define TEST_IS_AGGR(type, result) \
        static_assert(__is_aggregate(type) == result, \
                "expected __is_aggregate(" #type ") == " #result \
        );
// Fundamental types are never aggregate
TEST_IS_AGGR(int, false)
TEST_IS_AGGR(const int, false)
TEST_IS_AGGR(void, false)
TEST_IS_AGGR(volatile void, false)
TEST_IS_AGGR(const volatile char, false)
TEST_IS_AGGR(bool, false)
// Class types:
// N4741 [dcl.init.aggr]/1 An aggregate is ... a class with
// (1.1) no user-provided, explicit, or inherited constructors ([class.ctor]),
// (1.2) no private or protected non-static data members ([class.access]),
// (1.3) no virtual functions
// Since C++17: (1.4) no virtual, private, or protected base classes
// Before C++17: no base classes
// Class types without inheritance [dcl.init.aggr] (1.1 - 1.3)
struct Empty { };
struct NonAggDefaultCtor { NonAggDefaultCtor(); };
struct NonAggCustomCtor { NonAggCustomCtor(int, int); };
struct NonAggCopyCtor { NonAggCopyCtor(NonAggCopyCtor&); };
struct NonAggMoveCtor { NonAggMoveCtor(NonAggMoveCtor&&); };
struct AggCopyAssign { AggCopyAssign& operator=(AggCopyAssign&); };
struct AggMoveAssign { AggMoveAssign& operator=(AggMoveAssign&&); };
struct AggDtor { ~AggDtor(); };
struct NonAggPrivateCtor { private: NonAggPrivateCtor(); };
struct NonAggProtectedCtor { protected: NonAggProtectedCtor(); };
struct AggDeletedDefaultCtor { AggDeletedDefaultCtor() = delete; };
struct AggDeletedCopyCtor { AggDeletedCopyCtor(AggDeletedDefaultCtor&) = delete; };
struct NonAggExplicitlyDefaultedCtor { NonAggExplicitlyDefaultedCtor() = default; };
struct NonAggExplicitDefaultCtor { explicit NonAggExplicitDefaultCtor() = default; };
class ClassAgg {
public:
        int x = 0;
        NonAggDefaultCtor n;
        int& mem_func();
private:
        void private_mem_func();
        static const int private_static_mem = 0;
};
class ClassNonAggPrivate {
private:
        int private_mem;
public:
        void set(int x) { private_mem = x; }
        int get() const { return private_mem; }
};
class ClassNonAggProtected {
protected:
        int protected_mem;
};
union Union {
        int a;
        double b = 3.14;
        Empty c;
};
enum Enumeration { One };
enum class EnumClass { One };
struct AggRef {
        const int& ref_mem;
};
struct AggBitField {
        unsigned a : 3;
        unsigned : 5;
        char b : 8;
        int : 0;
};
struct AggAnonymousUnion {
        union { int c; double d; };
};
struct NonAggVirtualFunc {
        int a;
        virtual void virt_func();
};
struct NonAggPureVirtualFunc {
        virtual int pure_virt_f() = 0;
};
struct Cxx14Agg {
        int x;
        Empty e;
        NonAggDefaultCtor n;
        Union u;
        Empty* ep;
        Enumeration em;
};
TEST_IS_AGGR(Empty, true)
TEST_IS_AGGR(NonAggDefaultCtor, false)
TEST_IS_AGGR(NonAggCustomCtor, false)
TEST_IS_AGGR(NonAggCopyCtor, false)
TEST_IS_AGGR(NonAggMoveCtor, false)
TEST_IS_AGGR(AggCopyAssign, true)
TEST_IS_AGGR(AggMoveAssign, true)
TEST_IS_AGGR(AggDtor, true)
TEST_IS_AGGR(NonAggPrivateCtor, false)
TEST_IS_AGGR(NonAggProtectedCtor, false)
TEST_IS_AGGR(AggDeletedDefaultCtor, true)
TEST_IS_AGGR(AggDeletedCopyCtor, true)
TEST_IS_AGGR(NonAggExplicitlyDefaultedCtor, true)
TEST_IS_AGGR(NonAggExplicitDefaultCtor, false)
TEST_IS_AGGR(ClassAgg, true)
TEST_IS_AGGR(ClassNonAggPrivate, false)
TEST_IS_AGGR(ClassNonAggProtected, false)
TEST_IS_AGGR(Union, true)
TEST_IS_AGGR(Enumeration, false)
TEST_IS_AGGR(EnumClass, false)
TEST_IS_AGGR(AggRef, true)
TEST_IS_AGGR(AggBitField, true)
TEST_IS_AGGR(AggAnonymousUnion, true)
TEST_IS_AGGR(NonAggVirtualFunc, false)
TEST_IS_AGGR(NonAggPureVirtualFunc, false)
TEST_IS_AGGR(Cxx14Agg, true)
// Class types with inheritance: [dcl.init.aggr] (1.4)
struct Cxx17Agg : Cxx14Agg, ClassAgg {
        int x;
};
struct Cxx17AggNonInheritCtor : NonAggDefaultCtor { };
struct NonAggInheritCtor : NonAggDefaultCtor { using NonAggDefaultCtor::NonAggDefaultCtor; };
struct NonAggPrivateBase : private Cxx14Agg { };
struct NonAggProtectedBase : protected Cxx14Agg { };
struct NonAggVirtualBase : virtual Cxx14Agg { };
struct NonAggVirtualBaseFunc : NonAggVirtualFunc { };
struct NonAggVirtualBaseIndirect : NonAggVirtualBase { };
TEST_IS_AGGR(Cxx17Agg, true)
TEST_IS_AGGR(Cxx17AggNonInheritCtor, true)
TEST_IS_AGGR(NonAggInheritCtor, false)
TEST_IS_AGGR(NonAggPrivateBase, false)
TEST_IS_AGGR(NonAggProtectedBase, false)
TEST_IS_AGGR(NonAggVirtualBase, false)
TEST_IS_AGGR(NonAggVirtualBaseFunc, false)
TEST_IS_AGGR(NonAggVirtualBaseIndirect, false)
// Array types: all array types T are aggregate (where remove_all_extents_t<T> is complete)
// N4741 [dcl.init.aggr] An aggregate is an array or ...
using IntArrayBound = int[10];
using IntArrayNoBound = int[];
using ArrayOfAgg = Cxx14Agg[];
using ArrayOfCxx17Agg = Cxx17Agg[];
using ArrayOfNonAgg = NonAggDefaultCtor[];
using PtrArray = int*[10];
using ArrayMultiDim = NonAggDefaultCtor[2][3];
using ArrayEmpty = Empty[10];
TEST_IS_AGGR(IntArrayBound, true)
TEST_IS_AGGR(IntArrayNoBound, true)
TEST_IS_AGGR(ArrayOfAgg, true)
TEST_IS_AGGR(ArrayOfCxx17Agg, true)
TEST_IS_AGGR(ArrayOfNonAgg, true)
TEST_IS_AGGR(PtrArray, true)
TEST_IS_AGGR(ArrayMultiDim, true)
TEST_IS_AGGR(ArrayEmpty, true)
// Indirection types are never aggregate
TEST_IS_AGGR(int&, false)
TEST_IS_AGGR(const int*, false)
TEST_IS_AGGR(const Cxx14Agg&, false)
TEST_IS_AGGR(Cxx14Agg*, false)
// Templated types
template <typename T> struct AggTemplateClass {
        T value;
};
template <typename T> struct NonAggTemplateClassCtor {
        NonAggTemplateClassCtor() : value({}) {}
        T value;
};
TEST_IS_AGGR(AggTemplateClass<int>, true)
TEST_IS_AGGR(NonAggTemplateClassCtor<int>, false)
// Type aliases
using AliasedAgg = Cxx14Agg;
using AliasedAggCxx17 = Cxx17Agg;
using AliasedNonAgg = NonAggDefaultCtor;
template <typename T>
using AggAliasedTemplateClass = AggTemplateClass<T>;
template <typename T>
using NonAggAliasedTemplateClass = NonAggTemplateClassCtor<T>;
TEST_IS_AGGR(AliasedAgg, true)
TEST_IS_AGGR(AliasedAggCxx17, true)
TEST_IS_AGGR(AliasedNonAgg, false)
TEST_IS_AGGR(AggAliasedTemplateClass<Cxx14Agg>, true)
TEST_IS_AGGR(NonAggAliasedTemplateClass<Cxx14Agg>, false)
// Decltype
int integral;
Cxx17Agg cxx17AggObj;
Cxx14Agg cxx14AggObj;
TEST_IS_AGGR(decltype(integral), false)
TEST_IS_AGGR(decltype(cxx17AggObj), true)
TEST_IS_AGGR(decltype(cxx14AggObj), true)
