#include "basic_hdrs.h"
#include "gcc_gen_be_error.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Mock for EDG globals
FILE *f_error = NULL;
a_diagnostic_counter_set diagnostic_counters;

extern "C" {
// Include the source file directly for testing its internal functions if needed
// Or just link against it. Let's just define the test here and link it.
}

int main() {
    f_error = stderr;
    const char *str = NULL;
    gcc_gen_be_error_t err;

    // Test error string mapping
    err = gcc_gen_be_error_string(GCC_GEN_BE_SUCCESS, &str);
    if (err != GCC_GEN_BE_SUCCESS || strcmp(str, "GCC_GEN_BE_SUCCESS") != 0) return 1;

    err = gcc_gen_be_error_string(GCC_GEN_BE_ERROR_OOM, &str);
    if (err != GCC_GEN_BE_SUCCESS || strcmp(str, "GCC_GEN_BE_ERROR_OOM") != 0) return 1;

    err = gcc_gen_be_error_string(GCC_GEN_BE_ERROR_EH_UNSUPPORTED, &str);
    if (err != GCC_GEN_BE_SUCCESS || strcmp(str, "GCC_GEN_BE_ERROR_EH_UNSUPPORTED") != 0) return 1;

    err = gcc_gen_be_error_string((gcc_gen_be_error_t)9999, &str);
    if (err != GCC_GEN_BE_SUCCESS || strcmp(str, "GCC_GEN_BE_ERROR_UNKNOWN") != 0) return 1;

    // Test NULL pointer handling
    err = gcc_gen_be_error_string(GCC_GEN_BE_SUCCESS, NULL);
    if (err != GCC_GEN_BE_ERROR_INVALID_ARGUMENT) return 1;

    // Test report diagnostic
    diagnostic_counters.total.errors = 0;
    diagnostic_counters.total.catastrophes = 0;
    
    gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_EH_UNSUPPORTED, "Test context");
    if (diagnostic_counters.total.errors != 1 || diagnostic_counters.total.catastrophes != 0) return 1;

    gcc_gen_be_report_diagnostic(GCC_GEN_BE_ERROR_OOM, "OOM test");
    if (diagnostic_counters.total.errors != 1 || diagnostic_counters.total.catastrophes != 1) return 1;

    printf("PASS\n");
    return 0;
}
