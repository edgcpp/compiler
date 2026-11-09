//type:fp
//options:--c++11 --gn 150200 --target linux_riscv64 --no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^(file-scope type@.*\nkind: +(tk_typeref|tk_riscv_vector))/' | grep -E -e '^(kind|typeref_type|element_type|length_multiplier|tuple_elements|predeclared|  name):' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs
