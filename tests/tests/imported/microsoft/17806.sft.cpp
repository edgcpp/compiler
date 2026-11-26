//type:fn
//options_all:--microsoft_v 1910
struct S {
    S(int);
    operator int();
};
int i = (const S)0;
