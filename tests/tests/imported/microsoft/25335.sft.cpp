//options:--clang_v 90000:--clang_v 100000
struct test_struct {
    int test_attr;
};

int main() {
    struct test_struct instance = { .test_attr = 0 };
    struct test_struct *instance_ptr = &instance;
    int a = __builtin_preserve_access_index(instance_ptr->test_attr);
}
