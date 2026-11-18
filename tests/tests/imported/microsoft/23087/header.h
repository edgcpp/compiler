namespace std {
     constexpr bool is_constant_evaluated() {
         return __builtin_is_constant_evaluated();
     }

    template <class T>
    constexpr void reverse(T t) {
        is_constant_evaluated();
    }
}
