//type:fp
//options_all:--ms_c++20 -tused --modules --microsoft_version 1952 --ms_mod_file_map A.ifc --target win64

import A;

template<class T>
concept C = requires(T& t) {
    ::fn(t);
};

static_assert(C<int>);

int main() {}
