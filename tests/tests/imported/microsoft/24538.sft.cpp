//options_all:--ms_c++latest
union ULARGE_INTEGER {
    struct {
        long LowPart;
        long HighPart;
    };
    long long QuadPart;
};

ULARGE_INTEGER ul{ .LowPart = 123, .HighPart = 456 };
