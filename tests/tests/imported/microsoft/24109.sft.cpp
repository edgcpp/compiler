//options_all:--microsoft_version=1929
//type:fp
template <class R, class ...T>
struct Delegate {
    template <class>
    struct Specialization {
        static const bool Value = false;
    };

    template <>
    struct Specialization<Delegate<R, T...>> {
        static const bool Value = true;
    };
};
