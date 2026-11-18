//type:cp
//options_all:--microsoft_v 1940 --ms_c++20
typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
} GUID;

typedef GUID IID;

typedef long HRESULT;

typedef unsigned long ULONG;

extern "C" const IID IID_IUnknown;
extern "C++"
{
    struct __declspec(uuid("00000000-0000-0000-C000-000000000046")) __declspec(novtable)
    IUnknown
    {
    public:
        virtual HRESULT __stdcall QueryInterface(
            const IID& riid,
            void** ppvObject) = 0;

        virtual ULONG __stdcall AddRef(void) = 0;

        virtual ULONG __stdcall Release(void) = 0;

        template<class Q>
        HRESULT __stdcall QueryInterface(Q** pp)
        {
            return QueryInterface(__uuidof(Q), (void**)pp);
        }
    };
} // extern C++

template <class, class>
inline constexpr bool is_same_v = false; // determine whether arguments are the same type
template <class _Ty>
inline constexpr bool is_same_v<_Ty, _Ty> = true;

template <class _Ty1, class _Ty2>
concept _Same_impl = is_same_v<_Ty1, _Ty2>;

template <class _Ty1, class _Ty2>
concept same_as = _Same_impl<_Ty1, _Ty2>&& _Same_impl<_Ty2, _Ty1>;

static_assert(same_as<decltype(__uuidof(IUnknown)), GUID const>);

int main()
{
}
