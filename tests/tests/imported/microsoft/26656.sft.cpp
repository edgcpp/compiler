//options_all:--microsoft
//type:fp
void normal_function() {}
int main() {
    (void*&)normal_function;
    return 0;
}
