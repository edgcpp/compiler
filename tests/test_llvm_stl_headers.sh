#!/usr/bin/env bash
#
# Standard Library Header Conformance Validation for EDG LLVM Backend
# Compiles a comprehensive set of C++ standard headers to verify they
# lower to valid LLVM IR without errors or verification failures.

set -e

CPFE_PATH="build/bin/cpfe"
if [ ! -f "$CPFE_PATH" ]; then
    echo "Error: $CPFE_PATH not found. Please build the project."
    exit 1
fi

TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT

HEADERS=(
    "iostream"
    "vector"
    "string"
    "map"
    "memory"
    "algorithm"
    "functional"
    "type_traits"
)

echo "--- LLVM Backend STL Header Validation ---"

FAIL_COUNT=0

for header in "${HEADERS[@]}"; do
    SRC_FILE="$TEMP_DIR/test_${header}.cpp"
    LL_FILE="$TEMP_DIR/test_${header}.ll"
    
    echo "#include <${header}>" > "$SRC_FILE"
    echo "int main() { return 0; }" >> "$SRC_FILE"
    
    echo -n "Testing <${header}>... "
    
    # We use cpfe to compile the file to LLVM IR.
    # The backend runs llvm::verifyModule internally. 
    # If the module is malformed, cpfe will exit with an error.
    if "$CPFE_PATH" --c++20 --gen_llvm_file_name "$LL_FILE" "$SRC_FILE" > /dev/null 2>&1; then
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
