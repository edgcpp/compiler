//type:cp
//options_all:--ms_c++20 --microsoft_version 1950
namespace std {
    template <class _Ty>
    struct _Choice_t {
        _Ty _Strategy = _Ty{};
    };
    namespace _Strong_order {
        class _Cpo {
        private:
            enum class _St { _None, };
            template <class _Ty1, class _Ty2>
            static consteval _Choice_t<_St> _Choose() {
                return { _St::_None };
            }
            template <class _Ty1, class _Ty2>
            static constexpr _Choice_t<_St> _Choice = _Choose<_Ty1, _Ty2>();
        };
    }
}

int main()
{
    return 0;
}
