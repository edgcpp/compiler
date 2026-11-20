//options_all:--microsoft
//type:fp
namespace std {
    template <class _Elem>
    class initializer_list {
    public:
      constexpr initializer_list(const _Elem* _First_arg, const _Elem* _Last_arg) {}
    };
}

struct Event {};
void main()
{
  auto a1 = { Event{ }, { } };
  auto a2 = { Event{ }, Event{ } };
}
