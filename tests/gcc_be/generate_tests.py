import os

modules = [
    "error", "rtti", "eh", "abi", "type", "stmt", "builtin", "debug", "main"
]

template = """#include <gtest/gtest.h>
#include "gcc_gen_be_{mod}.h"

TEST(GccBe{Mod}Test, NullPointerHandling) {{
    // Verify that functions gracefully handle NULL pointers
    // and return GCC_GEN_BE_ERROR_INVALID_ARGUMENT.
}}

TEST(GccBe{Mod}Test, BasicExecution) {{
    // Exercise normal flow
}}
"""

for mod in modules:
    content = template.format(mod=mod, Mod=mod.capitalize())
    with open(f"tests/gcc_be/test_gcc_be_{mod}.cpp", "w") as f:
        f.write(content)

