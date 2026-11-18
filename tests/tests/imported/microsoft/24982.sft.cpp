//options_all:--g++ --c++17 --ms_extensions
namespace std {
    template <class T>
    struct remove_reference { typedef T type; };
    template <class T>
    struct remove_reference<T &> { typedef T type; };
    template <class T>
    struct remove_reference<T &&> { typedef T type; };

    template <class T>
    constexpr typename std::remove_reference<T>::type &&move(T &&t) noexcept {
        return static_cast<typename std::remove_reference<T>::type &&>(t);
    }
}

class path {
public:
    path(path &&p) : cmpts(std::move(p.cmpts)) {}

private:
    struct _List {
        _List();
        _List(const _List &);
        _List(_List &&) = default;
        _List &operator=(const _List &);
        _List &operator=(_List &&) = default;
        ~_List() = default;
    };

    _List cmpts;
};
