#!/usr/bin/env bash
#
# Standard Library Header Conformance Validation for EDG LLVM Backend
# Compiles the EDG C++ standard runtime headers to verify they
# lower to valid LLVM IR without errors or verification failures.

set -e

CPFE_PATH="${CPFE_BIN:-.tmp/build_tests/bin/cpfe}"
if [ ! -f "$CPFE_PATH" ]; then
    CPFE_PATH="build/bin/cpfe"
fi
if [ ! -f "$CPFE_PATH" ]; then
    echo "Error: $CPFE_PATH not found. Please build the project."
    exit 1
fi

TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT

cat << 'EOF' > "$TEMP_DIR/stddef.h"
#ifndef __STDDEF_H
#define __STDDEF_H
typedef __EDG_SIZE_TYPE__ size_t;
typedef __EDG_PTRDIFF_TYPE__ ptrdiff_t;
#endif
EOF

cat << 'EOF' > "$TEMP_DIR/cstdint"
#ifndef __CSTDINT_STUB
#define __CSTDINT_STUB
namespace std {
typedef unsigned int uint32_t;
typedef unsigned int uint_least32_t;
}
#endif
EOF

HEADERS=(
    "exception.stdh"
    "new.stdh"
    "typeinfo.stdh"
    "initializer_list.stdh"
    "source_location.stdh"
    "compare.stdh"
    "coroutine.stdh"
    "stdfloat.stdh"
)

echo "--- LLVM Backend Standard Header Validation ---"

FAIL_COUNT=0

for header in "${HEADERS[@]}"; do
    SRC_FILE="$TEMP_DIR/test_${header}.cpp"
    LL_FILE="$TEMP_DIR/test_${header}.ll"
    
    echo "#include <${header}>" > "$SRC_FILE"
    echo "int main() { return 0; }" >> "$SRC_FILE"
    
    echo -n "Testing <${header}>... "
    
    if "$CPFE_PATH" --c++20 -I "$TEMP_DIR" -I include_c++ --gen_llvm_file_name "$LL_FILE" "$SRC_FILE" > /dev/null 2>&1; then
        echo "PASS"
    else
        echo "FAIL"
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi
done

if [ "$FAIL_COUNT" -eq 0 ]; then
    echo "All headers compiled successfully to valid LLVM IR."
    exit 0
else
    echo "$FAIL_COUNT headers failed to compile."
    exit 1
fi
