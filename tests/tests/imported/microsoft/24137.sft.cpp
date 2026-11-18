//options_all:--microsoft_version=1929 --c++20
///type:fp
template <class T>
T &&move(T &&t) { return static_cast<T &&>(t); }

template <class T>
T &&forward(T &t) { return static_cast<T &&>(t); }
template <class T>
T &&forward(T &&t) { return static_cast<T &&>(t); }

template <class R>
concept range = requires(R r) { r.begin(); };

template <class C>
struct RangeAdaptorClosure {
    C callable;

    template <class R>
    auto operator()(R &&r) const { return this->callable(forward<R>(r)); }
};

template <class C>
struct RangeAdaptor {
constexpr RangeAdaptor(C) {}

    template <class ...As>
    auto operator()(As &&...as) const {
        auto closure = [...as(forward<As>(as))]<class R>(R &&r) {
            return C{}(forward<R>(r));
        };
        return RangeAdaptorClosure<decltype(closure)>(move(closure));
    }
};

template<typename C>
RangeAdaptor(C) -> RangeAdaptor<C>;

template <class R>
struct take_view { take_view(R) {} };

template <range R>
take_view(R &&) -> take_view<R>;

inline constexpr RangeAdaptor take = []<class R>(R &&r) {
    return take_view{forward<R>(r)};
};

template <class T>
struct generator { void begin(); };
int main() { take()(generator<const int>()); }
