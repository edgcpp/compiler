//options_all:--c++20
template <class T>
concept has_member = requires(T t) { t.begin(); };

template <class T> requires has_member<T &>
void begin(T &&);

template <class R>
concept range = requires(R &r) { begin(r); };

template <class D>
struct view_interface { void empty() requires range<D>; };

struct iota_view : public view_interface<iota_view> {
    struct iterator {};
    iterator begin();
};

void f() { begin(iota_view{}); }
