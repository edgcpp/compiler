//options_all:--microsoft_version=1929 --ms_c++14
//type:fp
struct CAtlModule {
    virtual void Unlock() throw() {}
};

struct CServerAppModule : CAtlModule {
     void Unlock() {}
};
