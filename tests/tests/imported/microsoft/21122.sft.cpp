//type:fn
//options_all:--microsoft_version 1920
void f()
{
                int y = 0;
                int x = 0;
                auto lambda = [&]
                {
                                __asm {
                                                mov eax, x
                                                mov y, eax
                                }
                };
}
