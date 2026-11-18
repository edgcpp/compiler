//options_all:--microsoft_version 1914 --ms_c++17
template <int Mode> struct codecvt_one_one {
    void do_in(int ch) {
        (void) ch;
        if constexpr (Mode == 0)
            ;
        else if (ch == 0)
            ;
        else
            ;
    }
};
template struct codecvt_one_one<0>;
