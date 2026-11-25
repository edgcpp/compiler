//type:fp
//options:--c++11 --gn 150200 --target linux_riscv64 --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

/* @filter
file-scope type[kind==tk_typeref]
  kind, source_corresp/name, typeref_type, element_type, length_multiplier,
  tuple_elements, predeclared
file-scope type[kind==tk_riscv_vector]
  kind, source_corresp/name, typeref_type, element_type, length_multiplier,
  tuple_elements, predeclared
 */
