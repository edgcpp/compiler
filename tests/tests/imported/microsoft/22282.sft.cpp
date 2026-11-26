//options_all:--microsoft_v 1926
template <class ...Ts>
struct Values {
              template <Ts ...Vs>
              struct Holder {};
};

int i;
Values<char, int*>::Holder<'a', &i> valueHolder;
