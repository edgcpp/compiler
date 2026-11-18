//type:fp
//options_all:--microsoft --microsoft_bugs --microsoft_version 1914 --no_ms_permissive --ms_c++latest

template<class T>
struct lock_me {
    lock_me(T&);
};

struct lockable {};

#ifndef WORKAROUND
template<class T>
#endif
struct someTemplate {
    lockable x;
    void someMember() {
        lock_me lck(x);
    }
};
