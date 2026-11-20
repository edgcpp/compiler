//options_all:--clang --ms_compatibility
//type:fp
extern "C" {

unsigned long __readcr3(void);

static unsigned long __readcr3(void) {
    return 0;
}

}
