//type:fp
//options:--c++20:--gn 160200:--clang_version 230100
//options_all:-w --no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope pragma@/' | grep -E -e '^(kind|pragma_text):' -e '^file|func-scope ' -e '^$' | edg-enumerate-il-addrs

#pragma clang diagnostic push
#pragma clang diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic pop
