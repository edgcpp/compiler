//type:fp
//options_all:--microsoft_version 1913
struct S
{
    S(int) {};
};
__declspec(thread) S s(1);
