//type:cp
//options_all:--microsoft_v 1939 --c
#define maxint(a,b) \
   __extension__({int _a = (a), _b = (b); _a > _b ? _a : _b; })

int main()
{
    return maxint(10, 12);
}
