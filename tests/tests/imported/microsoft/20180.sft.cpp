//options_all:--microsoft_version 1914
typedef struct _GUID {
              unsigned long  Data1;
              unsigned short Data2;
              unsigned short Data3;
              unsigned char  Data4[8];
} GUID;

struct __declspec(uuid("00000000-0000-0000-C000-000000000046")) __declspec(novtable)
IUnknown {};

constexpr GUID value = __uuidof(IUnknown);
