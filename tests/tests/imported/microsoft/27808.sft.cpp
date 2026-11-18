//options:--ms_c++latest --microsoft_v 1940;fp:--ms_c++latest --microsoft_v 1939;fc:--ms_c23 --microsoft_v 1940;fp:--ms_c23 --microsoft_v 1939;fc
#ifdef A
#define N 1
#elifndef B
#define N 2
#else
#define N 3
#endif
#if N != 2
#error Feature not supported
#endif
