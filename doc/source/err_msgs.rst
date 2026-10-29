.. _error-messages:

==============
Error Messages
==============

.. list-table::

 * - ``0001``\ 
   - | ``last_line_incomplete``\ :
     | last line of file ends without a newline
 * - ``0002``\ 
   - | ``last_line_backslash``\ :
     | last line of file ends with a backslash
 * - ``0003``\ 
   - | ``include_recursion``\ :
     | #include file *"xxxx"*\  includes itself
 * - ``0004``\ 
   - | ``out_of_memory``\ :
     | out of memory
 * - ``0006``\ 
   - | ``comment_unclosed_at_eof``\ :
     | comment unclosed at end of file
 * - ``0007``\ 
   - | ``bad_token``\ :
     | unrecognized token
 * - ``0008``\ 
   - | ``unclosed_string``\ :
     | missing closing quote
 * - ``0009``\ 
   - | ``nested_comment``\ :
     | nested comment is not allowed
 * - ``0010``\ 
   - | ``bad_use_of_sharp``\ :
     | "#" not expected here
 * - ``0011``\ 
   - | ``bad_pp_directive_keyword``\ :
     | unrecognized preprocessing directive
 * - ``0012``\ 
   - | ``end_of_flush``\ :
     | parsing restarts here after previous syntax error
 * - ``0013``\ 
   - | ``exp_file_name``\ :
     | expected a file name
 * - ``0014``\ 
   - | ``extra_text_in_pp_directive``\ :
     | extra text after expected end of preprocessing directive
 * - ``0017``\ 
   - | ``exp_rbracket``\ :
     | expected a "]"
 * - ``0018``\ 
   - | ``exp_rparen``\ :
     | expected a ")"
 * - ``0019``\ 
   - | ``extra_chars_on_number``\ :
     | extra text after expected end of number
 * - ``0020``\ 
   - | ``undefined_identifier``\ :
     | identifier *"xxxx"*\  is undefined
 * - ``0021``\ 
   - | ``useless_type_qualifiers``\ :
     | type qualifiers are meaningless in this declaration
 * - ``0022``\ 
   - | ``bad_hex_digit``\ :
     | invalid hexadecimal number
 * - ``0023``\ 
   - | ``integer_too_large``\ :
     | integer constant is too large
 * - ``0024``\ 
   - | ``bad_octal_digit``\ :
     | invalid octal digit
 * - ``0025``\ 
   - | ``zero_length_string``\ :
     | quoted string should contain at least one character
 * - ``0026``\ 
   - | ``too_many_characters``\ :
     | too many characters in character constant
 * - ``0027``\ 
   - | ``bad_character_value``\ :
     | character value is out of range
 * - ``0028``\ 
   - | ``expr_not_constant``\ :
     | expression must have a constant value
 * - ``0029``\ 
   - | ``exp_primary_expr``\ :
     | expected an expression
 * - ``0030``\ 
   - | ``bad_float_value``\ :
     | floating constant is out of range
 * - ``0031``\ 
   - | ``expr_not_integral``\ :
     | expression must have integral type
 * - ``0032``\ 
   - | ``expr_not_arithmetic``\ :
     | expression must have arithmetic type
 * - ``0033``\ 
   - | ``exp_line_number``\ :
     | expected a line number
 * - ``0034``\ 
   - | ``bad_line_number``\ :
     | invalid line number
 * - ``0035``\ 
   - | ``error_directive``\ :
     | #error directive: *xxxx*\ 
 * - ``0036``\ 
   - | ``missing_pp_if``\ :
     | the #if for this directive is missing
 * - ``0037``\ 
   - | ``missing_endif``\ :
     | the #endif for this directive is missing
 * - ``0038``\ 
   - | ``pp_else_already_appeared``\ :
     | directive is not allowed -- an #else has already appeared
 * - ``0039``\ 
   - | ``divide_by_zero``\ :
     | division by zero
 * - ``0040``\ 
   - | ``exp_identifier``\ :
     | expected an identifier
 * - ``0041``\ 
   - | ``expr_not_scalar``\ :
     | expression must have arithmetic or pointer type
 * - ``0042``\ 
   - | ``incompatible_operands``\ :
     | operand types are incompatible (*"type"*\  and *"type"*\ )
 * - ``0044``\ 
   - | ``expr_not_pointer``\ :
     | expression must have pointer type
 * - ``0045``\ 
   - | ``cannot_undef_predef_macro``\ :
     | #undef may not be used on this predefined name
 * - ``0046``\ 
   - | ``cannot_redef_predef_macro``\ :
     | *"entity"*\  is predefined; attempted redefinition ignored
 * - ``0047``\ 
   - | ``bad_macro_redef``\ :
     | incompatible redefinition of macro *"entity"*\  (declared at line
       *xxxx*\ )
 * - ``0049``\ 
   - | ``duplicate_macro_param_name``\ :
     | duplicate macro parameter name
 * - ``0050``\ 
   - | ``paste_cannot_be_first``\ :
     | "##" may not be first in a macro definition
 * - ``0051``\ 
   - | ``paste_cannot_be_last``\ :
     | "##" may not be last in a macro definition
 * - ``0052``\ 
   - | ``exp_macro_param``\ :
     | expected a macro parameter name
 * - ``0053``\ 
   - | ``exp_colon``\ :
     | expected a ":"
 * - ``0054``\ 
   - | ``too_few_macro_args``\ :
     | too few arguments in invocation of *entity-kind "entity"*\ 
 * - ``0055``\ 
   - | ``too_many_macro_args``\ :
     | too many arguments in invocation of *entity-kind "entity"*\ 
 * - ``0056``\ 
   - | ``sizeof_function``\ :
     | operand of sizeof may not be a function
 * - ``0057``\ 
   - | ``bad_constant_operator``\ :
     | this operator is not allowed in a constant expression
 * - ``0058``\ 
   - | ``bad_pp_operator``\ :
     | this operator is not allowed in a preprocessing expression
 * - ``0059``\ 
   - | ``bad_constant_function_call``\ :
     | function call is not allowed in a constant expression
 * - ``0060``\ 
   - | ``bad_integral_operator``\ :
     | this operator is not allowed in an integral constant expression
 * - ``0061``\ 
   - | ``integer_overflow``\ :
     | integer operation result is out of range
 * - ``0062``\ 
   - | ``negative_shift_count``\ :
     | shift count is negative
 * - ``0063``\ 
   - | ``shift_count_too_large``\ :
     | shift count is too large
 * - ``0064``\ 
   - | ``useless_decl``\ :
     | declaration does not declare anything
 * - ``0065``\ 
   - | ``exp_semicolon``\ :
     | expected a ";"
 * - ``0066``\ 
   - | ``enum_value_out_of_int_range``\ :
     | enumeration value is out of "int" range
 * - ``0067``\ 
   - | ``exp_rbrace``\ :
     | expected a "}"
 * - ``0068``\ 
   - | ``integer_sign_change``\ :
     | integer conversion resulted in a change of sign
 * - ``0069``\ 
   - | ``integer_truncated``\ :
     | integer conversion resulted in truncation
 * - ``0070``\ 
   - | ``incomplete_type_not_allowed``\ :
     | incomplete type *"type"*\  is not allowed
 * - ``0071``\ 
   - | ``sizeof_bit_field``\ :
     | operand of sizeof may not be a bit field
 * - ``0075``\ 
   - | ``bad_indirection_operand``\ :
     | operand of "\*" must be a pointer but has type *"type"*\ 
 * - ``0076``\ 
   - | ``empty_macro_argument``\ :
     | argument to macro is empty
 * - ``0077``\ 
   - | ``missing_decl_specifiers``\ :
     | this declaration has no storage class or type specifier
 * - ``0078``\ 
   - | ``initializer_in_param``\ :
     | a parameter declaration may not have an initializer
 * - ``0079``\ 
   - | ``exp_type_specifier``\ :
     | expected a type specifier
 * - ``0080``\ 
   - | ``storage_class_not_allowed``\ :
     | a storage class may not be specified here
 * - ``0081``\ 
   - | ``mult_storage_classes``\ :
     | more than one storage class may not be specified
 * - ``0082``\ 
   - | ``storage_class_not_first``\ :
     | storage class is not first
 * - ``0083``\ 
   - | ``dupl_type_qualifier``\ :
     | type qualifier specified more than once
 * - ``0084``\ 
   - | ``bad_combination_of_type_specifiers``\ :
     | invalid combination of type specifiers
 * - ``0085``\ 
   - | ``bad_param_storage_class``\ :
     | invalid storage class for a parameter
 * - ``0086``\ 
   - | ``bad_function_storage_class``\ :
     | invalid storage class for a function
 * - ``0087``\ 
   - | ``type_specifier_not_allowed``\ :
     | a type specifier may not be used here
 * - ``0088``\ 
   - | ``array_of_function``\ :
     | array of functions is not allowed
 * - ``0089``\ 
   - | ``array_of_void``\ :
     | array of void is not allowed
 * - ``0090``\ 
   - | ``function_returning_function``\ :
     | function returning function is not allowed
 * - ``0091``\ 
   - | ``function_returning_array``\ :
     | function returning array is not allowed
 * - ``0092``\ 
   - | ``param_id_list_needs_function_def``\ :
     | identifier-list parameters may only be used in a function definition
 * - ``0093``\ 
   - | ``function_type_must_come_from_declarator``\ :
     | function type may not come from a typedef
 * - ``0094``\ 
   - | ``array_size_must_be_positive``\ :
     | the size of an array must be greater than zero
 * - ``0095``\ 
   - | ``array_size_too_large``\ :
     | array is too large
 * - ``0096``\ 
   - | ``empty_translation_unit``\ :
     | a translation unit must contain at least one declaration
 * - ``0097``\ 
   - | ``bad_function_return_type``\ :
     | a function may not return a value of this type
 * - ``0098``\ 
   - | ``bad_array_element_type``\ :
     | an array may not have elements of this type
 * - ``0099``\ 
   - | ``decl_should_be_of_param``\ :
     | a declaration here must declare a parameter
 * - ``0100``\ 
   - | ``dupl_param_name``\ :
     | duplicate parameter name
 * - ``0101``\ 
   - | ``id_already_declared``\ :
     | *"xxxx"*\  has already been declared in the current scope
 * - ``0102``\ 
   - | ``nonstd_forward_decl_enum``\ :
     | forward declaration of enum type is nonstandard
 * - ``0103``\ 
   - | ``class_too_large``\ :
     | class is too large
 * - ``0104``\ 
   - | ``struct_too_large``\ :
     | struct or union is too large
 * - ``0105``\ 
   - | ``bad_bit_field_size``\ :
     | invalid size for bit field
 * - ``0106``\ 
   - | ``bad_bit_field_type``\ :
     | invalid type for a bit field
 * - ``0107``\ 
   - | ``zero_length_bit_field_must_be_unnamed``\ :
     | zero-length bit field must be unnamed
 * - ``0108``\ 
   - | ``signed_one_bit_field``\ :
     | signed bit field of length 1
 * - ``0109``\ 
   - | ``expr_not_ptr_to_function``\ :
     | expression preceding parentheses of apparent call must have
       (pointer-to-) function type
 * - ``0110``\ 
   - | ``exp_definition_of_tag``\ :
     | expected either a definition or a tag name
 * - ``0111``\ 
   - | ``code_is_unreachable``\ :
     | statement is unreachable
 * - ``0112``\ 
   - | ``exp_while``\ :
     | expected "while"
 * - ``0114``\ 
   - | ``never_defined``\ :
     | *entity-kind "entity"*\  was referenced but not defined
 * - ``0115``\ 
   - | ``continue_must_be_in_loop``\ :
     | a continue statement may only be used within a loop
 * - ``0116``\ 
   - | ``break_must_be_in_loop_or_switch``\ :
     | a break statement may only be used within a loop or switch
 * - ``0117``\ 
   - | ``no_value_returned_in_non_void_function``\ :
     | non-void *entity-kind "entity"*\  should return a value
 * - ``0118``\ 
   - | ``value_returned_in_void_function``\ :
     | a void function may not return a value
 * - ``0119``\ 
   - | ``cast_to_bad_type``\ :
     | cast to type *"type"*\  is not allowed
 * - ``0120``\ 
   - | ``bad_return_value_type``\ :
     | return value type does not match the function type
 * - ``0121``\ 
   - | ``case_label_must_be_in_switch``\ :
     | a case label may only be used within a switch
 * - ``0122``\ 
   - | ``default_label_must_be_in_switch``\ :
     | a default label may only be used within a switch
 * - ``0124``\ 
   - | ``default_label_appears_more_than_once``\ :
     | default label has already appeared in this switch
 * - ``0125``\ 
   - | ``exp_lparen``\ :
     | expected a "("
 * - ``0126``\ 
   - | ``expr_not_an_lvalue``\ :
     | expression must be an lvalue
 * - ``0127``\ 
   - | ``exp_statement``\ :
     | expected a statement
 * - ``0128``\ 
   - | ``loop_not_reachable``\ :
     | loop is not reachable
 * - ``0129``\ 
   - | ``block_scope_function_must_be_extern``\ :
     | a block-scope function may only have extern storage class
 * - ``0130``\ 
   - | ``exp_lbrace``\ :
     | expected a "{"
 * - ``0131``\ 
   - | ``expr_not_ptr_to_class``\ :
     | expression must have pointer-to-class type but it has type *"type"*\ 
 * - ``0132``\ 
   - | ``expr_not_ptr_to_struct_or_union``\ :
     | expression must have pointer-to-struct-or-union type but it has type
       *"type"*\ 
 * - ``0133``\ 
   - | ``exp_member_name``\ :
     | expected a member name
 * - ``0134``\ 
   - | ``exp_field_name``\ :
     | expected a field name
 * - ``0135``\ 
   - | ``not_a_member``\ :
     | *entity-kind "entity"*\  has no member *"xxxx"*\ 
 * - ``0136``\ 
   - | ``not_a_field``\ :
     | *entity-kind "entity"*\  has no field *"xxxx"*\ 
 * - ``0137``\ 
   - | ``expr_not_a_modifiable_lvalue``\ :
     | expression must be a modifiable lvalue
 * - ``0138``\ 
   - | ``address_of_register_variable``\ :
     | taking the address of a register variable is not allowed
 * - ``0139``\ 
   - | ``address_of_bit_field``\ :
     | taking the address of a bit field is not allowed
 * - ``0140``\ 
   - | ``too_many_arguments``\ :
     | too many arguments in function call
 * - ``0141``\ 
   - | ``all_proto_params_must_be_named``\ :
     | unnamed prototyped parameters not allowed when body is present
 * - ``0142``\ 
   - | ``expr_not_pointer_to_object``\ :
     | expression must have pointer-to-object type but it has type *"type"*\ 
 * - ``0143``\ 
   - | ``program_too_large``\ :
     | program too large or complicated to compile
 * - ``0144``\ 
   - | ``bad_initializer_type``\ :
     | a value of type *"type"*\  cannot be used to initialize an entity of
       type *"type"*\ 
 * - ``0145``\ 
   - | ``cannot_initialize``\ :
     | *entity-kind "entity"*\  may not be initialized
 * - ``0146``\ 
   - | ``too_many_initializer_values``\ :
     | too many initializer values
 * - ``0147``\ 
   - | ``not_compatible_with_previous_decl``\ :
     | declaration is incompatible with *entity-kind "entity"*\  (declared
       at line *xxxx*\ )
 * - ``0148``\ 
   - | ``already_initialized``\ :
     | *entity-kind "entity"*\  has already been initialized
 * - ``0149``\ 
   - | ``bad_file_scope_storage_class``\ :
     | a global-scope declaration may not have this storage class
 * - ``0150``\ 
   - | ``type_cannot_be_param_name``\ :
     | a type name may not be redeclared as a parameter
 * - ``0151``\ 
   - | ``typedef_cannot_be_param_name``\ :
     | a typedef name may not be redeclared as a parameter
 * - ``0152``\ 
   - | ``non_zero_int_conv_to_pointer``\ :
     | conversion of nonzero integer to pointer
 * - ``0153``\ 
   - | ``expr_not_class``\ :
     | expression must have class type but it has type *"type"*\ 
 * - ``0154``\ 
   - | ``expr_not_struct_or_union``\ :
     | expression must have struct or union type but it has type *"type"*\ 
 * - ``0155``\ 
   - | ``old_fashioned_assignment_operator``\ :
     | old-fashioned assignment operator
 * - ``0156``\ 
   - | ``old_fashioned_initializer``\ :
     | old-fashioned initializer
 * - ``0157``\ 
   - | ``expr_not_integral_constant``\ :
     | expression must be an integral constant expression
 * - ``0158``\ 
   - | ``expr_not_an_lvalue_or_function_designator``\ :
     | expression must be an lvalue or a function designator
 * - ``0159``\ 
   - | ``decl_incompatible_with_previous_use``\ :
     | declaration is incompatible with previous *"entity"*\  (declared at
       line *xxxx*\ )
 * - ``0160``\ 
   - | ``external_name_clash``\ :
     | external name conflicts with external name of *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``0161``\ 
   - | ``unrecognized_pragma``\ :
     | unrecognized #pragma
 * - ``0163``\ 
   - | ``cannot_open_temp_file_reason``\ :
     | could not open temporary file *"xxxx"*\ : *xxxx*\ 
 * - ``0165``\ 
   - | ``too_few_arguments``\ :
     | too few arguments in function call
 * - ``0166``\ 
   - | ``bad_float_constant``\ :
     | invalid floating constant
 * - ``0167``\ 
   - | ``incompatible_param``\ :
     | argument of type *"type"*\  is incompatible with parameter of type
       *"type"*\ 
 * - ``0168``\ 
   - | ``function_type_not_allowed``\ :
     | a function type is not allowed here
 * - ``0169``\ 
   - | ``exp_declaration``\ :
     | expected a declaration
 * - ``0170``\ 
   - | ``pointer_outside_base_object``\ :
     | pointer points outside of underlying object
 * - ``0171``\ 
   - | ``bad_cast``\ :
     | invalid type conversion
 * - ``0172``\ 
   - | ``linkage_conflict``\ :
     | external/internal linkage conflict with previous declaration at line
       *xxxx*\ 
 * - ``0173``\ 
   - | ``float_to_integer_conversion``\ :
     | floating-point value does not fit in required integral type
 * - ``0174``\ 
   - | ``expr_has_no_effect``\ :
     | expression has no effect
 * - ``0175``\ 
   - | ``subscript_out_of_range``\ :
     | subscript out of range
 * - ``0177``\ 
   - | ``declared_but_not_referenced``\ :
     | *entity-kind "entity"*\  was declared but never referenced
 * - ``0178``\ 
   - | ``pcc_address_of_array``\ :
     | "&" applied to an array has no effect
 * - ``0179``\ 
   - | ``mod_by_zero``\ :
     | right operand of "%" is zero
 * - ``0180``\ 
   - | ``old_style_incompatible_param``\ :
     | argument is incompatible with formal parameter
 * - ``0181``\ 
   - | ``printf_arg_mismatch``\ :
     | argument is incompatible with corresponding format string conversion
       (expected type *"type"*\  but argument has type *"type"*\ )
 * - ``0182``\ 
   - | ``empty_include_search_path``\ :
     | could not open source file *"xxxx"*\  (no directories in search list)
 * - ``0183``\ 
   - | ``cast_not_integral``\ :
     | type of cast must be integral
 * - ``0184``\ 
   - | ``cast_not_scalar``\ :
     | type of cast must be arithmetic or pointer
 * - ``0185``\ 
   - | ``initialization_not_reachable``\ :
     | dynamic initialization in unreachable code
 * - ``0186``\ 
   - | ``unsigned_compare_with_zero``\ :
     | pointless comparison of unsigned integer with zero
 * - ``0187``\ 
   - | ``assign_where_compare_meant``\ :
     | use of "=" where "==" may have been intended
 * - ``0188``\ 
   - | ``mixed_enum_type``\ :
     | enumerated type mixed with another type
 * - ``0189``\ 
   - | ``file_write_error``\ :
     | error while writing *xxxx*\  file
 * - ``0190``\ 
   - | ``bad_il_file``\ :
     | invalid intermediate language file
 * - ``0191``\ 
   - | ``cast_to_qualified_type``\ :
     | type qualifier is meaningless on cast type
 * - ``0192``\ 
   - | ``unrecognized_char_escape``\ :
     | unrecognized character escape sequence
 * - ``0193``\ 
   - | ``undefined_preproc_id``\ :
     | zero used for undefined preprocessing identifier *"xxxx"*\ 
 * - ``0194``\ 
   - | ``exp_asm_string``\ :
     | expected an asm string
 * - ``0195``\ 
   - | ``asm_func_must_be_prototyped``\ :
     | an asm function must be prototyped
 * - ``0196``\ 
   - | ``bad_asm_func_ellipsis``\ :
     | an asm function may not have an ellipsis
 * - ``0219``\ 
   - | ``file_delete_error_reason``\ :
     | error while deleting file *"xxxx"*\ : *xxxx*\ 
 * - ``0220``\ 
   - | ``integer_to_float_conversion``\ :
     | integral value does not fit in required floating-point type
 * - ``0221``\ 
   - | ``float_to_float_conversion``\ :
     | floating-point value does not fit in required floating-point type
 * - ``0222``\ 
   - | ``bad_float_operation_result``\ :
     | floating-point operation result is out of range
 * - ``0223``\ 
   - | ``implicit_func_decl``\ :
     | function *"xxxx"*\  declared implicitly
 * - ``0224``\ 
   - | ``too_few_printf_args``\ :
     | the format string requires additional arguments
 * - ``0225``\ 
   - | ``too_many_printf_args``\ :
     | the format string ends before this argument
 * - ``0226``\ 
   - | ``bad_printf_format_string``\ :
     | invalid format string conversion
 * - ``0227``\ 
   - | ``macro_recursion``\ :
     | macro recursion
 * - ``0228``\ 
   - | ``nonstd_extra_comma``\ :
     | trailing comma is nonstandard
 * - ``0229``\ 
   - | ``enum_bit_field_too_small``\ :
     | bit field cannot contain all values of the enumerated type
 * - ``0230``\ 
   - | ``nonstd_bit_field_type``\ :
     | nonstandard type for a bit field
 * - ``0231``\ 
   - | ``decl_in_prototype_scope``\ :
     | declaration is not visible outside of function
 * - ``0232``\ 
   - | ``decl_of_void_ignored``\ :
     | old-fashioned typedef of "void" ignored
 * - ``0233``\ 
   - | ``old_fashioned_field_selection``\ :
     | left operand is not a struct or union containing this field
 * - ``0234``\ 
   - | ``old_fashioned_ptr_field_selection``\ :
     | pointer does not point to struct or union containing this field
 * - ``0235``\ 
   - | ``var_retained_incomp_type``\ :
     | variable *"xxxx"*\  was declared with a never-completed type
 * - ``0236``\ 
   - | ``boolean_controlling_expr_is_constant``\ :
     | controlling expression is constant
 * - ``0237``\ 
   - | ``switch_selector_expr_is_constant``\ :
     | selector expression is constant
 * - ``0238``\ 
   - | ``bad_param_specifier``\ :
     | invalid specifier on a parameter
 * - ``0239``\ 
   - | ``bad_specifier_outside_class_decl``\ :
     | invalid specifier outside a class declaration
 * - ``0240``\ 
   - | ``dupl_decl_specifier``\ :
     | duplicate specifier in declaration
 * - ``0241``\ 
   - | ``base_class_not_allowed_for_union``\ :
     | a union is not allowed to have a base class
 * - ``0242``\ 
   - | ``access_already_specified``\ :
     | multiple access control specifiers are not allowed
 * - ``0243``\ 
   - | ``missing_class_definition``\ :
     | class or struct definition is missing
 * - ``0244``\ 
   - | ``name_not_member_of_class_or_base_classes``\ :
     | qualified name is not a member of class *"type"*\  or its base classes
 * - ``0245``\ 
   - | ``member_ref_requires_object``\ :
     | a nonstatic member reference must be relative to a specific object
 * - ``0246``\ 
   - | ``nonstatic_member_def_not_allowed``\ :
     | a nonstatic data member may not be defined outside its class
 * - ``0247``\ 
   - | ``already_defined``\ :
     | *entity-kind "entity"*\  has already been defined
 * - ``0248``\ 
   - | ``pointer_to_reference``\ :
     | pointer to reference is not allowed
 * - ``0249``\ 
   - | ``reference_to_reference``\ :
     | reference to reference is not allowed
 * - ``0250``\ 
   - | ``reference_to_void``\ :
     | reference to void is not allowed
 * - ``0251``\ 
   - | ``array_of_reference``\ :
     | array of reference is not allowed
 * - ``0252``\ 
   - | ``missing_initializer_on_reference``\ :
     | reference *entity-kind "entity"*\  requires an initializer
 * - ``0253``\ 
   - | ``exp_comma``\ :
     | expected a ","
 * - ``0254``\ 
   - | ``type_identifier_not_allowed``\ :
     | type name is not allowed
 * - ``0255``\ 
   - | ``type_definition_not_allowed``\ :
     | type definition is not allowed
 * - ``0256``\ 
   - | ``bad_type_name_redeclaration``\ :
     | invalid redeclaration of type name *"entity"*\  (declared at line
       *xxxx*\ )
 * - ``0257``\ 
   - | ``missing_initializer_on_const``\ :
     | const *entity-kind "entity"*\  requires an initializer
 * - ``0258``\ 
   - | ``this_used_incorrectly``\ :
     | "this" may only be used inside a nonstatic member function
 * - ``0259``\ 
   - | ``constant_value_not_known``\ :
     | constant value is not known
 * - ``0260``\ 
   - | ``missing_type_specifier``\ :
     | explicit type is missing ("int" assumed)
 * - ``0261``\ 
   - | ``missing_access_specifier``\ :
     | access control not specified (*"xxxx"*\  by default)
 * - ``0262``\ 
   - | ``not_a_class_or_struct_name``\ :
     | not a class or struct name
 * - ``0263``\ 
   - | ``dupl_base_class_name``\ :
     | duplicate base class name
 * - ``0264``\ 
   - | ``bad_base_class``\ :
     | invalid base class
 * - ``0265``\ 
   - | ``no_access_to_name``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) is inaccessible
 * - ``0266``\ 
   - | ``ambiguous_name``\ :
     | *"entity"*\  is ambiguous
 * - ``0267``\ 
   - | ``old_style_parameter_list``\ :
     | old-style parameter list (anachronism)
 * - ``0268``\ 
   - | ``declaration_after_statements``\ :
     | declaration may not appear after executable statement in block
 * - ``0269``\ 
   - | ``inaccessible_base_class``\ :
     | conversion to inaccessible base class *"type"*\  is not allowed
 * - ``0274``\ 
   - | ``improperly_terminated_macro_call``\ :
     | improperly terminated macro invocation
 * - ``0276``\ 
   - | ``id_must_be_class_or_namespace_name``\ :
     | name followed by "::" must be a class or namespace name
 * - ``0277``\ 
   - | ``bad_friend_decl``\ :
     | invalid friend declaration
 * - ``0278``\ 
   - | ``value_returned_in_constructor``\ :
     | a constructor or destructor may not return a value
 * - ``0279``\ 
   - | ``bad_destructor_decl``\ :
     | invalid destructor declaration
 * - ``0280``\ 
   - | ``class_and_member_name_conflict``\ :
     | declaration of a member with the same name as its class
 * - ``0281``\ 
   - | ``global_qualifier_not_allowed``\ :
     | global-scope qualifier (leading "::") is not allowed
 * - ``0282``\ 
   - | ``name_not_found_in_file_scope``\ :
     | the global scope has no *"xxxx"*\ 
 * - ``0283``\ 
   - | ``qualified_name_not_allowed``\ :
     | qualified name is not allowed
 * - ``0284``\ 
   - | ``null_reference``\ :
     | NULL reference is not allowed
 * - ``0285``\ 
   - | ``brace_initialization_not_allowed``\ :
     | initialization with "{...}" is not allowed for object of type
       *"type"*\ 
 * - ``0286``\ 
   - | ``ambiguous_base_class``\ :
     | base class *"type"*\  is ambiguous
 * - ``0287``\ 
   - | ``ambiguous_derived_class``\ :
     | derived class *"type"*\  contains more than one instance of class
       *"type"*\ 
 * - ``0288``\ 
   - | ``derived_class_from_virtual_base``\ :
     | cannot convert pointer to base class *"type"*\  to pointer to derived
       class *"type"*\  -- base class is virtual
 * - ``0289``\ 
   - | ``no_matching_constructor``\ :
     | no instance of constructor *"entity"*\  matches the argument list
 * - ``0290``\ 
   - | ``ambiguous_copy_constructor``\ :
     | copy constructor for class *"type"*\  is ambiguous
 * - ``0291``\ 
   - | ``no_default_constructor``\ :
     | no default constructor exists for class *"type"*\ 
 * - ``0292``\ 
   - | ``not_a_field_or_base_class``\ :
     | *"xxxx"*\  is not a nonstatic data member or base class of class
       *"type"*\ 
 * - ``0293``\ 
   - | ``indirect_nonvirtual_base_class_not_allowed``\ :
     | indirect nonvirtual base class is not allowed
 * - ``0294``\ 
   - | ``bad_union_field``\ :
     | invalid union member -- class *"type"*\  has a disallowed member
       function
 * - ``0296``\ 
   - | ``bad_rvalue_array``\ :
     | invalid use of non-lvalue array
 * - ``0297``\ 
   - | ``exp_operator``\ :
     | expected an operator
 * - ``0298``\ 
   - | ``inherited_member_not_allowed``\ :
     | inherited member is not allowed
 * - ``0299``\ 
   - | ``indeterminate_overloaded_function``\ :
     | cannot determine which instance of *entity-kind "entity"*\  is
       intended
 * - ``0300``\ 
   - | ``bound_function_must_be_called``\ :
     | a pointer to a bound function may only be used to call the function
 * - ``0301``\ 
   - | ``duplicate_typedef``\ :
     | typedef name has already been declared (with same type)
 * - ``0304``\ 
   - | ``no_matching_function``\ :
     | no instance of *entity-kind "entity"*\  matches the argument list
 * - ``0305``\ 
   - | ``type_def_not_allowed_in_func_type_decl``\ :
     | type definition is not allowed in function return type declaration
 * - ``0306``\ 
   - | ``default_arg_not_at_end``\ :
     | default argument not at end of parameter list
 * - ``0307``\ 
   - | ``default_arg_already_defined``\ :
     | redefinition of default argument
 * - ``0308``\ 
   - | ``ambiguous_overloaded_function``\ :
     | more than one instance of *entity-kind "entity"*\  matches the
       argument list:
 * - ``0309``\ 
   - | ``ambiguous_constructor``\ :
     | more than one instance of constructor *"entity"*\  matches the
       argument list:
 * - ``0310``\ 
   - | ``bad_default_arg_type``\ :
     | default argument of type *"type"*\  is incompatible with parameter of
       type *"type"*\ 
 * - ``0311``\ 
   - | ``return_type_cannot_distinguish_functions``\ :
     | cannot overload functions distinguished by return type alone
 * - ``0312``\ 
   - | ``no_user_defined_conversion``\ :
     | no suitable user-defined conversion from *"type"*\  to *"type"*\ 
       exists
 * - ``0314``\ 
   - | ``virtual_static_not_allowed``\ :
     | only nonstatic member functions may be virtual
 * - ``0315``\ 
   - | ``unqual_function_with_qual_object``\ :
     | the object has type qualifiers that are not compatible with the
       member function
 * - ``0316``\ 
   - | ``too_many_virtual_functions``\ :
     | program too large to compile (too many virtual functions)
 * - ``0317``\ 
   - | ``bad_return_type_on_virtual_function_override``\ :
     | return type is not identical to nor covariant with return type
       *"type"*\  of overridden virtual function *"entity"*\ 
 * - ``0318``\ 
   - | ``ambiguous_virtual_function_override``\ :
     | override of virtual *entity-kind "entity"*\  is ambiguous
 * - ``0319``\ 
   - | ``pure_specifier_on_nonvirtual_function``\ :
     | pure specifier ("= 0") allowed only on virtual functions
 * - ``0320``\ 
   - | ``bad_pure_specifier``\ :
     | badly-formed pure specifier (only "= 0" is allowed)
 * - ``0321``\ 
   - | ``bad_data_member_initialization``\ :
     | data member initializer is not allowed
 * - ``0322``\ 
   - | ``abstract_class_object_not_allowed``\ :
     | object of abstract class type *"type"*\  is not allowed:
 * - ``0323``\ 
   - | ``function_returning_abstract_class``\ :
     | function returning abstract class *"type"*\  is not allowed:
 * - ``0324``\ 
   - | ``duplicate_friend_decl``\ :
     | duplicate friend declaration
 * - ``0325``\ 
   - | ``inline_and_nonfunction``\ :
     | inline specifier allowed on function declarations only
 * - ``0326``\ 
   - | ``inline_not_allowed``\ :
     | "inline" is not allowed
 * - ``0327``\ 
   - | ``bad_storage_class_with_inline``\ :
     | invalid storage class for an inline function
 * - ``0328``\ 
   - | ``bad_member_storage_class``\ :
     | invalid storage class for a class member
 * - ``0329``\ 
   - | ``local_class_function_def_missing``\ :
     | local class member *entity-kind "entity"*\  requires a definition
 * - ``0330``\ 
   - | ``inaccessible_special_function``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) is inaccessible
 * - ``0332``\ 
   - | ``missing_const_copy_constructor``\ :
     | class *"type"*\  has no copy constructor to copy a const object
 * - ``0333``\ 
   - | ``definition_of_implicitly_declared_function``\ :
     | defining an implicitly declared member function is not allowed
 * - ``0334``\ 
   - | ``no_suitable_copy_constructor``\ :
     | class *"type"*\  has no suitable copy constructor
 * - ``0335``\ 
   - | ``linkage_specifier_not_allowed``\ :
     | linkage specification is not allowed
 * - ``0336``\ 
   - | ``bad_linkage_specifier``\ :
     | unknown external linkage specification
 * - ``0337``\ 
   - | ``incompatible_linkage_specifier``\ :
     | linkage specification is incompatible with previous *"entity"*\ 
       (declared at line *xxxx*\ )
 * - ``0338``\ 
   - | ``overloaded_function_linkage``\ :
     | more than one instance of overloaded function *"entity"*\  has "C"
       linkage
 * - ``0339``\ 
   - | ``ambiguous_default_constructor``\ :
     | class *"type"*\  has more than one default constructor
 * - ``0340``\ 
   - | ``temp_used_for_ref_init``\ :
     | value copied to temporary, reference to temporary used
 * - ``0341``\ 
   - | ``nonmember_operator_not_allowed``\ :
     | "operator*xxxx*\ " must be a member function
 * - ``0342``\ 
   - | ``static_member_operator_not_allowed``\ :
     | operator may not be a static member function
 * - ``0343``\ 
   - | ``too_many_args_for_conversion``\ :
     | no arguments allowed on user-defined conversion
 * - ``0344``\ 
   - | ``too_many_args_for_operator``\ :
     | too many parameters for this operator function
 * - ``0345``\ 
   - | ``too_few_args_for_operator``\ :
     | too few parameters for this operator function
 * - ``0346``\ 
   - | ``no_params_with_class_type``\ :
     | nonmember operator requires a parameter with class type
 * - ``0347``\ 
   - | ``default_arg_expr_not_allowed``\ :
     | default argument is not allowed
 * - ``0348``\ 
   - | ``ambiguous_user_defined_conversion``\ :
     | more than one user-defined conversion from *"type"*\  to *"type"*\ 
       applies:
 * - ``0349``\ 
   - | ``no_matching_operator_function``\ :
     | no operator *"xxxx"*\  matches these operands
 * - ``0350``\ 
   - | ``ambiguous_operator_function``\ :
     | more than one operator *"xxxx"*\  matches these operands:
 * - ``0351``\ 
   - | ``bad_arg_type_for_operator_new``\ :
     | first parameter of allocation function must be of type "size_t"
 * - ``0352``\ 
   - | ``bad_return_type_for_op_new``\ :
     | allocation function requires "void \*" return type
 * - ``0353``\ 
   - | ``bad_return_type_for_op_delete``\ :
     | deallocation function requires "void" return type
 * - ``0354``\ 
   - | ``bad_first_arg_type_for_operator_delete``\ :
     | first parameter of deallocation function must be of type "void \*"
 * - ``0356``\ 
   - | ``type_must_be_object_type``\ :
     | type must be an object type
 * - ``0357``\ 
   - | ``base_class_already_initialized``\ :
     | base class *"type"*\  has already been initialized
 * - ``0358``\ 
   - | ``base_class_init_anachronism``\ :
     | base class name required -- *"type"*\  assumed (anachronism)
 * - ``0359``\ 
   - | ``member_already_initialized``\ :
     | *entity-kind "entity"*\  has already been initialized
 * - ``0360``\ 
   - | ``missing_base_class_or_member_name``\ :
     | name of member or base class is missing
 * - ``0361``\ 
   - | ``assignment_to_this``\ :
     | assignment to "this" (anachronism)
 * - ``0362``\ 
   - | ``overload_anachronism``\ :
     | "overload" keyword used (anachronism)
 * - ``0363``\ 
   - | ``anon_union_member_access``\ :
     | invalid anonymous union -- nonpublic member is not allowed
 * - ``0364``\ 
   - | ``anon_union_member_function``\ :
     | invalid anonymous union -- member function is not allowed
 * - ``0365``\ 
   - | ``anon_union_storage_class``\ :
     | anonymous union at global or namespace scope must be declared static
 * - ``0366``\ 
   - | ``missing_initializer_on_fields``\ :
     | *entity-kind "entity"*\  provides no initializer for:
 * - ``0367``\ 
   - | ``cannot_initialize_fields``\ :
     | implicitly generated constructor for class *"type"*\  cannot
       initialize:
 * - ``0368``\ 
   - | ``no_ctor_but_const_or_ref_member``\ :
     | *entity-kind "entity"*\  defines no constructor to initialize the
       following:
 * - ``0369``\ 
   - | ``var_with_uninitialized_member``\ :
     | *entity-kind "entity"*\  has an uninitialized const or reference
       member
 * - ``0370``\ 
   - | ``var_with_uninitialized_field``\ :
     | *entity-kind "entity"*\  has an uninitialized const field
 * - ``0371``\ 
   - | ``missing_const_assignment_operator``\ :
     | class *"type"*\  has no assignment operator to copy a const object
 * - ``0372``\ 
   - | ``no_suitable_assignment_operator``\ :
     | class *"type"*\  has no suitable assignment operator
 * - ``0373``\ 
   - | ``ambiguous_assignment_operator``\ :
     | ambiguous assignment operator for class *"type"*\ 
 * - ``0375``\ 
   - | ``missing_typedef_name``\ :
     | declaration requires a typedef name
 * - ``0377``\ 
   - | ``virtual_not_allowed``\ :
     | "virtual" is not allowed
 * - ``0378``\ 
   - | ``static_not_allowed``\ :
     | "static" is not allowed
 * - ``0379``\ 
   - | ``bound_function_cast_anachronism``\ :
     | cast of bound function to normal function pointer (anachronism)
 * - ``0380``\ 
   - | ``expr_not_ptr_to_member``\ :
     | expression must have pointer-to-member type
 * - ``0381``\ 
   - | ``extra_semicolon``\ :
     | extra ";" ignored
 * - ``0382``\ 
   - | ``nonstd_const_member``\ :
     | in-class initializer for nonstatic member is nonstandard
 * - ``0384``\ 
   - | ``no_matching_new_function``\ :
     | no instance of overloaded *"entity"*\  matches the argument list
 * - ``0386``\ 
   - | ``no_match_for_addr_of_overloaded_function``\ :
     | no instance of *entity-kind "entity"*\  matches the required type
 * - ``0387``\ 
   - | ``delete_count_anachronism``\ :
     | delete array size expression used (anachronism)
 * - ``0389``\ 
   - | ``cast_to_abstract_class``\ :
     | a cast to abstract class *"type"*\  is not allowed:
 * - ``0390``\ 
   - | ``bad_use_of_main``\ :
     | function "main" may not be called or have its address taken
 * - ``0391``\ 
   - | ``initializer_not_allowed_on_array_new``\ :
     | a new-initializer may not be specified for an array
 * - ``0392``\ 
   - | ``member_function_redecl_outside_class``\ :
     | member function *"entity"*\  may not be redeclared outside its class
 * - ``0394``\ 
   - | ``ref_to_nested_function_var``\ :
     | reference to local variable of enclosing function is not allowed
 * - ``0395``\ 
   - | ``single_arg_postfix_incr_decr_anachronism``\ :
     | single-argument function used for postfix *"xxxx"*\  (anachronism)
 * - ``0397``\ 
   - | ``bad_default_assignment``\ :
     | implicitly generated assignment operator cannot copy:
 * - ``0398``\ 
   - | ``nonstd_array_cast``\ :
     | cast to array type is nonstandard (treated as cast to *"type"*\ )
 * - ``0399``\ 
   - | ``class_with_op_new_but_no_op_delete``\ :
     | *entity-kind "entity"*\  has an operator new*xxxx*\ () but no default
       operator delete*xxxx*\ ()
 * - ``0400``\ 
   - | ``class_with_op_delete_but_no_op_new``\ :
     | *entity-kind "entity"*\  has a default operator delete*xxxx*\ () but
       no operator new*xxxx*\ ()
 * - ``0401``\ 
   - | ``base_class_with_nonvirtual_dtor``\ :
     | destructor for base class *"entity"*\  (declared at line *xxxx*\ ) is
       not virtual
 * - ``0403``\ 
   - | ``member_function_redeclaration``\ :
     | invalid redeclaration of member *entity-kind "entity"*\  (declared at
       line *xxxx*\ )
 * - ``0404``\ 
   - | ``inline_main``\ :
     | function "main" may not be declared inline
 * - ``0405``\ 
   - | ``class_and_member_function_name_conflict``\ :
     | member function with the same name as its class must be a constructor
 * - ``0406``\ 
   - | ``nested_class_anachronism``\ :
     | using nested *entity-kind "entity"*\  (anachronism)
 * - ``0407``\ 
   - | ``too_many_params_for_destructor``\ :
     | a destructor may not have parameters
 * - ``0408``\ 
   - | ``bad_constructor_param``\ :
     | copy constructor for class *"type"*\  may not have a parameter of
       type *"type"*\ 
 * - ``0409``\ 
   - | ``incomplete_function_return_type``\ :
     | *entity-kind "entity"*\  returns incomplete type *"type"*\ 
 * - ``0410``\ 
   - | ``protected_access_problem``\ :
     | protected *entity-kind "entity"*\  (declared at line *xxxx*\ ) is not
       accessible through a *"type"*\  pointer or object
 * - ``0411``\ 
   - | ``param_not_allowed``\ :
     | a parameter is not allowed
 * - ``0412``\ 
   - | ``asm_decl_not_allowed``\ :
     | an "asm" declaration is not allowed here
 * - ``0413``\ 
   - | ``no_conversion_function``\ :
     | no suitable conversion function from *"type"*\  to *"type"*\  exists
 * - ``0414``\ 
   - | ``delete_of_incomplete_class``\ :
     | delete of pointer to incomplete class
 * - ``0415``\ 
   - | ``no_constructor_for_conversion``\ :
     | no suitable constructor exists to convert from *"type"*\  to
       *"type"*\ 
 * - ``0416``\ 
   - | ``ambiguous_constructor_for_conversion``\ :
     | more than one constructor applies to convert from *"type"*\  to
       *"type"*\ :
 * - ``0417``\ 
   - | ``ambiguous_conversion_function``\ :
     | more than one conversion function from *"type"*\  to *"type"*\ 
       applies:
 * - ``0418``\ 
   - | ``ambiguous_conversion_to_builtin``\ :
     | more than one conversion function from *"type"*\  to a built-in type
       applies:
 * - ``0424``\ 
   - | ``addr_of_constructor_or_destructor``\ :
     | a constructor or destructor may not have its address taken
 * - ``0426``\ 
   - | ``nonconst_ref_init_anachronism``\ :
     | temporary used for initial value of reference to non-const
       (anachronism)
 * - ``0427``\ 
   - | ``qualifier_in_member_declaration``\ :
     | qualified name is not allowed in member declaration
 * - ``0428``\ 
   - | ``mixed_enum_type_anachronism``\ :
     | enumerated type mixed with another type (anachronism)
 * - ``0429``\ 
   - | ``new_array_size_must_be_nonnegative``\ :
     | the size of an array in "new" must be non-negative
 * - ``0430``\ 
   - | ``return_ref_init_requires_temp``\ :
     | returning reference to local temporary
 * - ``0432``\ 
   - | ``enum_not_allowed``\ :
     | "enum" declaration is not allowed
 * - ``0433``\ 
   - | ``qualifier_dropped_in_ref_init``\ :
     | qualifiers dropped in binding reference of type *"type"*\  to
       initializer of type *"type"*\ 
 * - ``0434``\ 
   - | ``bad_nonconst_ref_init``\ :
     | a reference of type *"type"*\  (not const-qualified) cannot be
       initialized with a value of type *"type"*\ 
 * - ``0435``\ 
   - | ``delete_of_function_pointer``\ :
     | a pointer to function may not be deleted
 * - ``0436``\ 
   - | ``bad_conversion_function_decl``\ :
     | conversion function must be a nonstatic member function
 * - ``0437``\ 
   - | ``bad_template_declaration_scope``\ :
     | a template declaration is not allowed here
 * - ``0438``\ 
   - | ``exp_lt``\ :
     | expected a "<"
 * - ``0439``\ 
   - | ``exp_gt``\ :
     | expected a ">"
 * - ``0440``\ 
   - | ``missing_template_param``\ :
     | template parameter declaration is missing
 * - ``0441``\ 
   - | ``missing_template_arg_list``\ :
     | argument list for *entity-kind "entity"*\  is missing
 * - ``0442``\ 
   - | ``too_few_template_args``\ :
     | too few arguments for *entity-kind "entity"*\ 
 * - ``0443``\ 
   - | ``too_many_template_args``\ :
     | too many arguments for *entity-kind "entity"*\ 
 * - ``0445``\ 
   - | ``not_used_in_template_function_params``\ :
     | *entity-kind "entity"*\  is not used in declaring the parameter types
       of *entity-kind "entity"*\ 
 * - ``0446``\ 
   - | ``cfront_multiple_nested_types``\ :
     | two nested types have the same name: *"entity"*\  and *"entity"*\ 
       (declared at line *xxxx*\ ) (cfront compatibility)
 * - ``0447``\ 
   - | ``cfront_global_defined_after_nested_type``\ :
     | global *"entity"*\  was declared after nested *"entity"*\  (declared
       at line *xxxx*\ ) (cfront compatibility)
 * - ``0449``\ 
   - | ``ambiguous_ptr_to_overloaded_function``\ :
     | more than one instance of *entity-kind "entity"*\  matches the
       required type
 * - ``0450``\ 
   - | ``nonstd_long_long``\ :
     | the type "long long" is nonstandard
 * - ``0451``\ 
   - | ``nonstd_friend_decl``\ :
     | omission of *"xxxx"*\  is nonstandard
 * - ``0452``\ 
   - | ``return_type_on_conversion_function``\ :
     | return type may not be specified on a conversion function
 * - ``0456``\ 
   - | ``runaway_recursive_instantiation``\ :
     | excessive recursion at instantiation of *entity-kind "entity"*\ 
 * - ``0457``\ 
   - | ``bad_template_declaration``\ :
     | *"xxxx"*\  is not a function or static data member
 * - ``0458``\ 
   - | ``bad_nontype_template_arg``\ :
     | argument of type *"type"*\  is incompatible with template parameter
       of type *"type"*\ 
 * - ``0459``\ 
   - | ``init_needing_temp_not_allowed``\ :
     | initialization requiring a temporary or conversion is not allowed
 * - ``0460``\ 
   - | ``decl_hides_function_parameter``\ :
     | declaration of *"xxxx"*\  hides function parameter
 * - ``0461``\ 
   - | ``nonconst_ref_init_from_rvalue``\ :
     | initial value of reference to non-const must be an lvalue
 * - ``0463``\ 
   - | ``template_not_allowed``\ :
     | "template" is not allowed
 * - ``0464``\ 
   - | ``not_a_class_template``\ :
     | *"type"*\  is not a class template
 * - ``0466``\ 
   - | ``function_template_named_main``\ :
     | "main" is not a valid name for a function template
 * - ``0467``\ 
   - | ``union_nonunion_mismatch``\ :
     | invalid reference to *entity-kind "entity"*\  (union/nonunion
       mismatch)
 * - ``0468``\ 
   - | ``local_type_in_template_arg``\ :
     | a template argument may not reference a local type
 * - ``0469``\ 
   - | ``tag_kind_incompatible_with_declaration``\ :
     | tag kind of *xxxx*\  is incompatible with declaration of *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``0470``\ 
   - | ``name_not_tag_in_file_scope``\ :
     | the global scope has no tag named *"xxxx"*\ 
 * - ``0471``\ 
   - | ``not_a_tag_member``\ :
     | *entity-kind "entity"*\  has no tag member named *"xxxx"*\ 
 * - ``0472``\ 
   - | ``ptr_to_member_typedef``\ :
     | member function typedef (allowed for cfront compatibility)
 * - ``0473``\ 
   - | ``bad_use_of_member_function_typedef``\ :
     | *entity-kind "entity"*\  may be used only in pointer-to-member
       declaration
 * - ``0475``\ 
   - | ``nonexternal_entity_in_template_arg``\ :
     | a template argument may not reference a non-external entity
 * - ``0476``\ 
   - | ``id_must_be_class_or_type_name``\ :
     | name followed by "::~" must be a class name or a type name
 * - ``0478``\ 
   - | ``destructor_type_mismatch``\ :
     | type used as destructor name does not match type *"type"*\ 
 * - ``0479``\ 
   - | ``called_function_redeclared_inline``\ :
     | *entity-kind "entity"*\  redeclared "inline" after being called
 * - ``0481``\ 
   - | ``bad_storage_class_on_template_decl``\ :
     | invalid storage class for a template declaration
 * - ``0482``\ 
   - | ``no_access_to_type_cfront_mode``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) is an
       inaccessible type (allowed for cfront compatibility)
 * - ``0484``\ 
   - | ``invalid_instantiation_argument``\ :
     | invalid explicit instantiation declaration
 * - ``0485``\ 
   - | ``not_instantiatable_entity``\ :
     | *entity-kind "entity"*\  is not an entity that can be instantiated
 * - ``0486``\ 
   - | ``compiler_generated_function_cannot_be_instantiated``\ :
     | compiler generated *entity-kind "entity"*\  cannot be explicitly
       instantiated
 * - ``0487``\ 
   - | ``inline_function_cannot_be_instantiated``\ :
     | inline *entity-kind "entity"*\  cannot be explicitly instantiated
 * - ``0489``\ 
   - | ``instantiation_requested_no_definition_supplied``\ :
     | *entity-kind "entity"*\  cannot be instantiated -- no template
       definition was supplied
 * - ``0490``\ 
   - | ``instantiation_requested_and_specialized``\ :
     | *entity-kind "entity"*\  cannot be instantiated -- it has been
       explicitly specialized
 * - ``0493``\ 
   - | ``no_match_for_type_of_overloaded_function``\ :
     | no instance of *entity-kind "entity"*\  matches the specified type
 * - ``0494``\ 
   - | ``nonstd_void_param_list``\ :
     | declaring a void parameter list with a typedef is nonstandard
 * - ``0495``\ 
   - | ``cfront_name_lookup_bug``\ :
     | global *entity-kind "entity"*\  used instead of *entity-kind
       "entity"*\  (cfront compatibility)
 * - ``0496``\ 
   - | ``redeclaration_of_template_param_name``\ :
     | template parameter *"xxxx"*\  may not be redeclared in this scope
 * - ``0497``\ 
   - | ``decl_hides_template_parameter``\ :
     | declaration of *"xxxx"*\  hides template parameter
 * - ``0498``\ 
   - | ``must_be_prototype_instantiation``\ :
     | template argument list must match the parameter list
 * - ``0500``\ 
   - | ``bad_extra_arg_for_postfix_operator``\ :
     | extra parameter of postfix "operator*xxxx*\ " must be of type "int"
 * - ``0501``\ 
   - | ``function_type_required``\ :
     | an operator name must be declared as a function
 * - ``0502``\ 
   - | ``operator_name_not_allowed``\ :
     | operator name is not allowed
 * - ``0503``\ 
   - | ``bad_scope_for_specialization``\ :
     | *entity-kind "entity"*\  cannot be specialized in the current scope
 * - ``0504``\ 
   - | ``nonstd_member_function_address``\ :
     | nonstandard form for taking the address of a member function
 * - ``0505``\ 
   - | ``too_few_template_params``\ :
     | too few template parameters -- does not match previous declaration
       (declared at line *xxxx*\ )
 * - ``0506``\ 
   - | ``too_many_template_params``\ :
     | too many template parameters -- does not match previous declaration
       (declared at line *xxxx*\ )
 * - ``0507``\ 
   - | ``template_operator_delete``\ :
     | function template for operator delete(void \*) is not allowed
 * - ``0508``\ 
   - | ``class_template_same_name_as_templ_param``\ :
     | class template and template parameter may not have the same name
 * - ``0510``\ 
   - | ``unnamed_type_in_template_arg``\ :
     | a template argument may not reference an unnamed type
 * - ``0511``\ 
   - | ``enum_type_not_allowed``\ :
     | this operation on an enumerated type requires an applicable
       user-defined operator function
 * - ``0512``\ 
   - | ``qualified_reference_type``\ :
     | type qualifier on a reference type is not allowed
 * - ``0513``\ 
   - | ``incompatible_assignment_operands``\ :
     | a value of type *"type"*\  cannot be assigned to an entity of type
       *"type"*\ 
 * - ``0514``\ 
   - | ``unsigned_compare_with_negative``\ :
     | pointless comparison of unsigned integer with a negative constant
 * - ``0515``\ 
   - | ``converting_to_incomplete_class``\ :
     | cannot convert to incomplete class *"type"*\ 
 * - ``0516``\ 
   - | ``missing_initializer_on_unnamed_const``\ :
     | const object requires an initializer
 * - ``0517``\ 
   - | ``unnamed_object_with_uninitialized_field``\ :
     | object has an uninitialized const or reference member
 * - ``0518``\ 
   - | ``nonstd_pp_directive``\ :
     | nonstandard preprocessing directive
 * - ``0519``\ 
   - | ``unexpected_template_arg_list``\ :
     | *entity-kind "entity"*\  may not have a template argument list
 * - ``0520``\ 
   - | ``missing_initializer_list``\ :
     | initialization with "{...}" expected for aggregate object
 * - ``0521``\ 
   - | ``incompatible_ptr_to_member_selection_operands``\ :
     | pointer-to-member selection class types are incompatible (*"type"*\ 
       and *"type"*\ )
 * - ``0522``\ 
   - | ``self_friendship``\ :
     | pointless friend declaration
 * - ``0523``\ 
   - | ``period_used_as_qualifier``\ :
     | "." used in place of "::" to form a qualified name
 * - ``0524``\ 
   - | ``const_function_anachronism``\ :
     | non-const function called for const object (anachronism)
 * - ``0525``\ 
   - | ``dependent_stmt_is_declaration``\ :
     | a dependent statement may not be a declaration
 * - ``0526``\ 
   - | ``void_param_not_allowed``\ :
     | a parameter may not have void type
 * - ``0529``\ 
   - | ``bad_templ_arg_expr_operator``\ :
     | this operator is not allowed in a template argument expression
 * - ``0530``\ 
   - | ``missing_handler``\ :
     | try block requires at least one handler
 * - ``0531``\ 
   - | ``missing_exception_declaration``\ :
     | handler requires an exception declaration
 * - ``0532``\ 
   - | ``masked_by_default_handler``\ :
     | handler is masked by default handler
 * - ``0533``\ 
   - | ``masked_by_handler``\ :
     | handler is potentially masked by previous handler for type *"type"*\ 
 * - ``0534``\ 
   - | ``local_type_used_in_exception``\ :
     | use of a local type to specify an exception
 * - ``0535``\ 
   - | ``redundant_exception_specification_type``\ :
     | redundant type in exception specification
 * - ``0536``\ 
   - | ``incompatible_exception_specification``\ :
     | exception specification is incompatible with that of previous
       *entity-kind "entity"*\  (declared at line *xxxx*\ ):
 * - ``0540``\ 
   - | ``no_exception_support``\ :
     | support for exception handling is disabled
 * - ``0541``\ 
   - | ``omitted_exception_specification``\ :
     | allowing all exceptions is incompatible with previous *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``0542``\ 
   - | ``cannot_create_instantiation_request_file``\ :
     | could not create instantiation request file *"xxxx"*\ 
 * - ``0543``\ 
   - | ``non_arith_operation_in_templ_arg``\ :
     | non-arithmetic operation not allowed in nontype template argument
 * - ``0544``\ 
   - | ``local_type_in_nonlocal_var``\ :
     | use of a local type to declare a nonlocal variable
 * - ``0545``\ 
   - | ``local_type_in_function``\ :
     | use of a local type to declare a function
 * - ``0546``\ 
   - | ``branch_past_initialization``\ :
     | transfer of control bypasses initialization of:
 * - ``0548``\ 
   - | ``branch_into_handler``\ :
     | transfer of control into an exception handler
 * - ``0549``\ 
   - | ``used_before_set``\ :
     | *entity-kind "entity"*\  is used before its value is set
 * - ``0550``\ 
   - | ``set_but_not_used``\ :
     | *entity-kind "entity"*\  was set but never used
 * - ``0551``\ 
   - | ``bad_scope_for_definition``\ :
     | *entity-kind "entity"*\  cannot be defined in the current scope
 * - ``0552``\ 
   - | ``exception_specification_not_allowed``\ :
     | exception specification is not allowed
 * - ``0553``\ 
   - | ``template_and_instance_linkage_conflict``\ :
     | external/internal linkage conflict for *entity-kind "entity"*\ 
       (declared at line *xxxx*\ )
 * - ``0554``\ 
   - | ``conversion_function_not_usable``\ :
     | *entity-kind "entity"*\  will not be called for implicit or explicit
       conversions
 * - ``0555``\ 
   - | ``tag_kind_incompatible_with_template_parameter``\ :
     | tag kind of *xxxx*\  is incompatible with template parameter of type
       *"type"*\ 
 * - ``0556``\ 
   - | ``template_operator_new``\ :
     | function template for operator new(size_t) is not allowed
 * - ``0558``\ 
   - | ``bad_member_type_in_ptr_to_member``\ :
     | pointer to member of type *"type"*\  is not allowed
 * - ``0559``\ 
   - | ``ellipsis_on_operator_function``\ :
     | ellipsis is not allowed in operator function parameter list
 * - ``0560``\ 
   - | ``unimplemented_keyword``\ :
     | *"entity"*\  is reserved for future use as a keyword
 * - ``0561``\ 
   - | ``cl_invalid_macro_definition``\ :
     | invalid macro definition: *xxxx*\ 
 * - ``0562``\ 
   - | ``cl_invalid_macro_undefinition``\ :
     | invalid macro undefinition: *xxxx*\ 
 * - ``0565``\ 
   - | ``cl_il_file_must_be_specified``\ :
     | IL file name must be specified if input is 
 * - ``0570``\ 
   - | ``cl_error_in_debug_option_argument``\ :
     | error in debug option argument
 * - ``0571``\ 
   - | ``cl_invalid_option``\ :
     | invalid option: *xxxx*\ 
 * - ``0572``\ 
   - | ``cl_back_end_requires_il_file``\ :
     | back end requires name of IL file
 * - ``0573``\ 
   - | ``cl_could_not_open_il_file``\ :
     | could not open IL file *xxxx*\ 
 * - ``0574``\ 
   - | ``cl_invalid_number``\ :
     | invalid number: *xxxx*\ 
 * - ``0575``\ 
   - | ``cl_incorrect_host_id``\ :
     | incorrect host CPU id
 * - ``0576``\ 
   - | ``cl_invalid_instantiation_mode``\ :
     | invalid instantiation mode: *xxxx*\ 
 * - ``0578``\ 
   - | ``cl_invalid_error_limit``\ :
     | invalid error limit: *xxxx*\ 
 * - ``0585``\ 
   - | ``cl_vtbl_option_only_in_cplusplus``\ :
     | virtual function tables can only be suppressed when compiling C++
 * - ``0586``\ 
   - | ``cl_anachronism_option_only_in_cplusplus``\ :
     | anachronism option can be used only when compiling C++
 * - ``0587``\ 
   - | ``cl_instantiation_option_only_in_cplusplus``\ :
     | instantiation mode option can be used only when compiling C++
 * - ``0588``\ 
   - | ``cl_auto_instantiation_option_only_in_cplusplus``\ :
     | automatic instantiation mode can be used only when compiling C++
 * - ``0589``\ 
   - | ``cl_implicit_inclusion_option_only_in_cplusplus``\ :
     | implicit template inclusion mode can be used only when compiling C++
 * - ``0590``\ 
   - | ``cl_exceptions_option_only_in_cplusplus``\ :
     | exception handling option can be used only when compiling C++
 * - ``0591``\ 
   - | ``cl_strict_mode_incompatible_with_pcc``\ :
     | strict mode is incompatible with K&R mode
 * - ``0592``\ 
   - | ``cl_strict_mode_incompatible_with_cfront``\ :
     | strict mode is incompatible with cfront mode
 * - ``0593``\ 
   - | ``cl_missing_source_file_name``\ :
     | missing source file name
 * - ``0594``\ 
   - | ``cl_output_file_incompatible_with_multiple_inputs``\ :
     | output files may not be specified when compiling several input files
 * - ``0595``\ 
   - | ``cl_too_many_arguments``\ :
     | too many arguments on command line
 * - ``0596``\ 
   - | ``cl_no_output_file_needed``\ :
     | an output file was specified, but none is needed
 * - ``0597``\ 
   - | ``cl_il_display_requires_il_file_name``\ :
     | IL display requires name of IL file
 * - ``0598``\ 
   - | ``void_template_parameter``\ :
     | a template parameter may not have void type
 * - ``0599``\ 
   - | ``too_many_unused_instantiations``\ :
     | excessive recursive instantiation of *entity-kind "entity"*\  due to
       instantiate-all mode
 * - ``0600``\ 
   - | ``cl_strict_mode_incompatible_with_anachronisms``\ :
     | strict mode is incompatible with allowing anachronisms
 * - ``0601``\ 
   - | ``void_throw``\ :
     | a throw expression may not have void type
 * - ``0602``\ 
   - | ``cl_tim_local_conflicts_with_auto_instantiation``\ :
     | local instantiation mode is incompatible with automatic instantiation
 * - ``0603``\ 
   - | ``abstract_class_param_type``\ :
     | parameter of abstract class type *"type"*\  is not allowed:
 * - ``0604``\ 
   - | ``array_of_abstract_class``\ :
     | array of abstract class *"type"*\  is not allowed:
 * - ``0605``\ 
   - | ``float_template_parameter``\ :
     | floating-point template parameter is nonstandard
 * - ``0606``\ 
   - | ``pragma_must_precede_declaration``\ :
     | this pragma must immediately precede a declaration
 * - ``0607``\ 
   - | ``pragma_must_precede_statement``\ :
     | this pragma must immediately precede a statement
 * - ``0608``\ 
   - | ``pragma_must_precede_decl_or_stmt``\ :
     | this pragma must immediately precede a declaration or statement
 * - ``0609``\ 
   - | ``pragma_may_not_be_used_here``\ :
     | this kind of pragma may not be used here
 * - ``0611``\ 
   - | ``partial_override``\ :
     | overloaded virtual function *"entity"*\  is only partially overridden
       in *entity-kind "entity"*\ 
 * - ``0612``\ 
   - | ``specialization_of_called_inline_template_function``\ :
     | specific definition of inline template function must precede its
       first use
 * - ``0613``\ 
   - | ``cl_invalid_error_tag``\ :
     | invalid error tag in diagnostic control option: *xxxx*\ 
 * - ``0614``\ 
   - | ``cl_invalid_error_number``\ :
     | invalid error number in diagnostic control option: *xxxx*\ 
 * - ``0617``\ 
   - | ``ptr_to_member_cast_to_ptr_to_function``\ :
     | pointer-to-member-function cast to pointer to function
 * - ``0618``\ 
   - | ``no_named_fields``\ :
     | struct or union declares no named members
 * - ``0619``\ 
   - | ``nonstd_unnamed_field``\ :
     | nonstandard unnamed field
 * - ``0620``\ 
   - | ``nonstd_unnamed_member``\ :
     | nonstandard unnamed member
 * - ``0624``\ 
   - | ``not_a_type_name``\ :
     | *"xxxx"*\  is not a type name
 * - ``0625``\ 
   - | ``cannot_open_pch_input_file_reason``\ :
     | cannot open precompiled header input file *"xxxx"*\ : *xxxx*\ 
 * - ``0626``\ 
   - | ``invalid_pch_file``\ :
     | precompiled header file *"xxxx"*\  is either invalid or not generated
       by this version of the compiler
 * - ``0627``\ 
   - | ``pch_curr_directory_changed``\ :
     | precompiled header file *"xxxx"*\  was not generated in this directory
 * - ``0628``\ 
   - | ``pch_header_files_have_changed``\ :
     | header files used to generate precompiled header file *"xxxx"*\  have
       changed
 * - ``0629``\ 
   - | ``pch_cmd_line_option_mismatch``\ :
     | the command line options do not match those used when precompiled
       header file *"xxxx"*\  was created
 * - ``0630``\ 
   - | ``pch_file_prefix_mismatch``\ :
     | the initial sequence of preprocessing directives is not compatible
       with those of precompiled header file *"xxxx"*\ 
 * - ``0631``\ 
   - | ``unable_to_get_mapped_memory``\ :
     | unable to obtain mapped memory
 * - ``0632``\ 
   - | ``using_pch``\ :
     | "*xxxx*\ ": using precompiled header file "*xxxx*\ "
 * - ``0633``\ 
   - | ``creating_pch``\ :
     | "*xxxx*\ ": creating precompiled header file "*xxxx*\ "
 * - ``0634``\ 
   - | ``memory_mismatch``\ :
     | memory usage conflict with precompiled header file *"xxxx"*\ 
 * - ``0635``\ 
   - | ``cl_invalid_pch_size``\ :
     | invalid PCH memory size: *xxxx*\  
 * - ``0636``\ 
   - | ``cl_pch_must_be_first``\ :
     | PCH options must appear first in the command line
 * - ``0637``\ 
   - | ``out_of_memory_during_pch_allocation``\ :
     | insufficient memory for PCH memory allocation
 * - ``0638``\ 
   - | ``cl_pch_incompatible_with_multiple_inputs``\ :
     | precompiled header files may not be used when compiling several input
       files
 * - ``0639``\ 
   - | ``not_enough_preallocated_memory``\ :
     | insufficient preallocated memory for generation of precompiled header
       file (*xxxx*\  bytes required)
 * - ``0640``\ 
   - | ``program_entity_too_large_for_pch``\ :
     | very large entity in program prevents generation of precompiled
       header file
 * - ``0641``\ 
   - | ``cannot_chdir``\ :
     | *"xxxx"*\  is not a valid directory
 * - ``0642``\ 
   - | ``cannot_build_temp_file_name``\ :
     | cannot build temporary file name
 * - ``0643``\ 
   - | ``restrict_not_allowed``\ :
     | "restrict" is not allowed
 * - ``0644``\ 
   - | ``restrict_pointer_to_function``\ :
     | a pointer or reference to function type may not be qualified by
       "restrict"
 * - ``0646``\ 
   - | ``calling_convention_not_allowed``\ :
     | a calling convention modifier may not be specified here
 * - ``0647``\ 
   - | ``conflicting_calling_conventions``\ :
     | conflicting calling convention modifiers
 * - ``0648``\ 
   - | ``cl_strict_mode_incompatible_with_microsoft``\ :
     | strict mode is incompatible with Microsoft mode
 * - ``0649``\ 
   - | ``cl_cfront_incompatible_with_microsoft``\ :
     | cfront mode is incompatible with Microsoft mode
 * - ``0650``\ 
   - | ``calling_convention_ignored``\ :
     | calling convention specified here is ignored
 * - ``0651``\ 
   - | ``calling_convention_may_not_precede_nested_declarator``\ :
     | a calling convention may not be followed by a nested declarator
 * - ``0652``\ 
   - | ``calling_convention_ignored_for_type``\ :
     | calling convention is ignored for this type
 * - ``0654``\ 
   - | ``decl_modifiers_incompatible_with_previous_decl``\ :
     | declaration modifiers are incompatible with previous declaration
 * - ``0655``\ 
   - | ``decl_modifiers_invalid_for_this_decl``\ :
     | the modifier *"xxxx"*\  is not allowed on this declaration
 * - ``0656``\ 
   - | ``branch_into_try_block``\ :
     | transfer of control into a try block
 * - ``0657``\ 
   - | ``incompatible_inline_specifier_on_specific_decl``\ :
     | inline specification is incompatible with previous *"entity"*\ 
       (declared at line *xxxx*\ )
 * - ``0658``\ 
   - | ``template_missing_closing_brace``\ :
     | closing brace of template definition not found
 * - ``0659``\ 
   - | ``cl_wchar_t_option_only_in_cplusplus``\ :
     | wchar_t keyword option can be used only when compiling C++
 * - ``0660``\ 
   - | ``bad_pack_alignment``\ :
     | invalid packing alignment value
 * - ``0661``\ 
   - | ``exp_int_constant``\ :
     | expected an integer constant
 * - ``0662``\ 
   - | ``call_of_pure_virtual``\ :
     | call of pure virtual function
 * - ``0663``\ 
   - | ``bad_ident_string``\ :
     | invalid source file identifier string
 * - ``0664``\ 
   - | ``template_friend_definition_not_allowed``\ :
     | a class template cannot be defined in a friend declaration
 * - ``0665``\ 
   - | ``asm_not_allowed``\ :
     | "asm" is not allowed
 * - ``0666``\ 
   - | ``bad_asm_function_def``\ :
     | "asm" must be used with a function definition
 * - ``0667``\ 
   - | ``nonstd_asm_function``\ :
     | "asm" function is nonstandard
 * - ``0668``\ 
   - | ``nonstd_ellipsis_only_param``\ :
     | ellipsis with no explicit parameters is nonstandard
 * - ``0669``\ 
   - | ``nonstd_address_of_ellipsis``\ :
     | "&..." is nonstandard
 * - ``0670``\ 
   - | ``bad_address_of_ellipsis``\ :
     | invalid use of "&..."
 * - ``0672``\ 
   - | ``const_volatile_ref_init_anachronism``\ :
     | temporary used for initial value of reference to const volatile
       (anachronism)
 * - ``0673``\ 
   - | ``bad_const_volatile_ref_init``\ :
     | a reference of type *"type"*\  cannot be initialized with a value of
       type *"type"*\ 
 * - ``0674``\ 
   - | ``const_volatile_ref_init_from_rvalue``\ :
     | initial value of reference to const volatile must be an lvalue
 * - ``0675``\ 
   - | ``cl_SVR4_C_option_only_in_ansi_C``\ :
     | SVR4 C compatibility option can be used only when compiling ANSI C
 * - ``0676``\ 
   - | ``using_out_of_scope_declaration``\ :
     | using out-of-scope declaration of *entity-kind "entity"*\  (declared
       at line *xxxx*\ )
 * - ``0677``\ 
   - | ``cl_strict_mode_incompatible_with_SVR4``\ :
     | strict mode is incompatible with SVR4 C mode
 * - ``0678``\ 
   - | ``cannot_inline_call``\ :
     | call of *entity-kind "entity"*\  (declared at line *xxxx*\ ) cannot
       be inlined
 * - ``0679``\ 
   - | ``cannot_inline``\ :
     | *entity-kind "entity"*\  cannot be inlined
 * - ``0680``\ 
   - | ``cl_invalid_pch_directory``\ :
     | invalid PCH directory: *xxxx*\ 
 * - ``0681``\ 
   - | ``exp_except_or_finally``\ :
     | expected \__except or \__finally
 * - ``0682``\ 
   - | ``leave_must_be_in_try``\ :
     | a \__leave statement may only be used within a \__try
 * - ``0688``\ 
   - | ``not_found_on_pack_alignment_stack``\ :
     | *"xxxx"*\  not found on pack alignment stack
 * - ``0689``\ 
   - | ``empty_pack_alignment_stack``\ :
     | empty pack alignment stack
 * - ``0690``\ 
   - | ``cl_rtti_option_only_in_cplusplus``\ :
     | RTTI option can be used only when compiling C++
 * - ``0691``\ 
   - | ``inaccessible_elided_cctor``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ), required for
       copy that was eliminated, is inaccessible
 * - ``0692``\ 
   - | ``uncallable_elided_cctor``\ :
     | *entity-kind "entity"*\ , required for copy that was eliminated, is
       not callable because reference parameter cannot be bound to rvalue
 * - ``0693``\ 
   - | ``typeid_needs_typeinfo``\ :
     | <typeinfo> must be included before typeid is used
 * - ``0694``\ 
   - | ``cannot_cast_away_const``\ :
     | *xxxx*\  cannot cast away const or other type qualifiers
 * - ``0695``\ 
   - | ``bad_dynamic_cast_type``\ :
     | the type in a dynamic_cast must be a pointer or reference to a
       complete class type, or void \*
 * - ``0696``\ 
   - | ``bad_ptr_dynamic_cast_operand``\ :
     | the operand of a pointer dynamic_cast must be a pointer to a complete
       class type
 * - ``0697``\ 
   - | ``bad_ref_dynamic_cast_operand``\ :
     | the operand of a reference dynamic_cast must be an lvalue of a
       complete class type
 * - ``0698``\ 
   - | ``dynamic_cast_operand_must_be_polymorphic``\ :
     | the operand of a runtime dynamic_cast must have a polymorphic class
       type
 * - ``0699``\ 
   - | ``cl_bool_option_only_in_cplusplus``\ :
     | bool option can be used only when compiling C++
 * - ``0701``\ 
   - | ``array_type_not_allowed``\ :
     | an array type is not allowed here
 * - ``0702``\ 
   - | ``exp_assign``\ :
     | expected an "="
 * - ``0704``\ 
   - | ``redeclaration_of_condition_decl_name``\ :
     | *"xxxx"*\ , declared in condition, may not be redeclared in this scope
 * - ``0705``\ 
   - | ``default_template_arg_not_allowed``\ :
     | default template arguments are not allowed for function templates
 * - ``0706``\ 
   - | ``exp_comma_or_gt``\ :
     | expected a "," or ">"
 * - ``0707``\ 
   - | ``missing_template_param_list``\ :
     | expected a template parameter list
 * - ``0708``\ 
   - | ``incr_of_bool_deprecated``\ :
     | incrementing a bool value is deprecated
 * - ``0709``\ 
   - | ``bool_type_not_allowed``\ :
     | bool type is not allowed
 * - ``0710``\ 
   - | ``base_class_offset_too_large``\ :
     | offset of base class *"entity"*\  within class *"entity"*\  is too
       large
 * - ``0711``\ 
   - | ``expr_not_bool``\ :
     | expression must have bool type (or be convertible to bool)
 * - ``0712``\ 
   - | ``cl_array_new_and_delete_option_only_in_cplusplus``\ :
     | array new and delete option can be used only when compiling C++
 * - ``0713``\ 
   - | ``based_requires_variable_name``\ :
     | *entity-kind "entity"*\  is not a variable name
 * - ``0714``\ 
   - | ``based_not_allowed_here``\ :
     | __based modifier is not allowed here
 * - ``0715``\ 
   - | ``based_not_followed_by_star``\ :
     | __based does not precede a pointer operator, \__based ignored
 * - ``0716``\ 
   - | ``based_var_must_be_ptr``\ :
     | variable in \__based modifier must have pointer type
 * - ``0717``\ 
   - | ``bad_const_cast_type``\ :
     | the type in a const_cast must be a pointer, reference, or pointer to
       member to an object type
 * - ``0718``\ 
   - | ``bad_const_cast``\ :
     | a const_cast can only adjust type qualifiers; it cannot change the
       underlying type
 * - ``0719``\ 
   - | ``mutable_not_allowed``\ :
     | mutable is not allowed
 * - ``0720``\ 
   - | ``cannot_change_access``\ :
     | redeclaration of *entity-kind "entity"*\  is not allowed to alter its
       access
 * - ``0722``\ 
   - | ``probable_inadvertent_lbracket_digraph``\ :
     | use of alternative token "<:" appears to be unintended
 * - ``0723``\ 
   - | ``probable_inadvertent_sharp_digraph``\ :
     | use of alternative token "%:" appears to be unintended
 * - ``0724``\ 
   - | ``namespace_def_not_allowed``\ :
     | namespace definition is not allowed
 * - ``0725``\ 
   - | ``missing_namespace_name``\ :
     | name must be a namespace name
 * - ``0726``\ 
   - | ``namespace_alias_def_not_allowed``\ :
     | namespace alias definition is not allowed
 * - ``0727``\ 
   - | ``namespace_qualified_name_required``\ :
     | namespace-qualified name is required
 * - ``0728``\ 
   - | ``namespace_name_not_allowed``\ :
     | a namespace name is not allowed
 * - ``0729``\ 
   - | ``bad_combination_of_dll_attributes``\ :
     | invalid combination of DLL attributes
 * - ``0730``\ 
   - | ``sym_not_a_class_template``\ :
     | *entity-kind "entity"*\  is not a class template
 * - ``0731``\ 
   - | ``array_of_incomplete_type``\ :
     | array with incomplete element type is nonstandard
 * - ``0732``\ 
   - | ``allocation_operator_in_namespace``\ :
     | allocation operator may not be declared in a namespace
 * - ``0733``\ 
   - | ``deallocation_operator_in_namespace``\ :
     | deallocation operator may not be declared in a namespace
 * - ``0734``\ 
   - | ``conflicts_with_using_decl``\ :
     | *entity-kind "entity"*\  conflicts with using-declaration of
       *entity-kind "entity"*\ 
 * - ``0735``\ 
   - | ``using_decl_conflicts_with_prev_decl``\ :
     | using-declaration of *entity-kind "entity"*\  conflicts with
       *entity-kind "entity"*\  (declared at line *xxxx*\ )
 * - ``0736``\ 
   - | ``cl_namespaces_option_only_in_cplusplus``\ :
     | namespaces option can be used only when compiling C++
 * - ``0737``\ 
   - | ``useless_using_declaration``\ :
     | using-declaration ignored -- it refers to the current namespace
 * - ``0738``\ 
   - | ``class_qualified_name_required``\ :
     | a class-qualified name is required
 * - ``0742``\ 
   - | ``not_an_actual_member``\ :
     | *entity-kind "entity"*\  has no actual member *"xxxx"*\ 
 * - ``0744``\ 
   - | ``mem_attrib_incompatible``\ :
     | incompatible memory attributes specified
 * - ``0745``\ 
   - | ``mem_attrib_ignored``\ :
     | memory attribute ignored
 * - ``0746``\ 
   - | ``mem_attrib_may_not_precede_nested_declarator``\ :
     | memory attribute may not be followed by a nested declarator
 * - ``0747``\ 
   - | ``dupl_mem_attrib``\ :
     | memory attribute specified more than once
 * - ``0748``\ 
   - | ``dupl_calling_convention``\ :
     | calling convention specified more than once
 * - ``0749``\ 
   - | ``type_qualifier_not_allowed``\ :
     | a type qualifier is not allowed
 * - ``0750``\ 
   - | ``template_instance_already_used``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) was used before
       its template was declared
 * - ``0751``\ 
   - | ``static_nonstatic_with_same_param_types``\ :
     | static and nonstatic member functions with same parameter types
       cannot be overloaded
 * - ``0752``\ 
   - | ``no_prior_declaration``\ :
     | no prior declaration of *entity-kind "entity"*\ 
 * - ``0753``\ 
   - | ``template_id_not_allowed``\ :
     | a template-id is not allowed
 * - ``0754``\ 
   - | ``class_qualified_name_not_allowed``\ :
     | a class-qualified name is not allowed
 * - ``0755``\ 
   - | ``bad_scope_for_redeclaration``\ :
     | *entity-kind "entity"*\  may not be redeclared in the current scope
 * - ``0756``\ 
   - | ``qualifier_in_namespace_member_decl``\ :
     | qualified name is not allowed in namespace member declaration
 * - ``0757``\ 
   - | ``sym_not_a_type_name``\ :
     | *entity-kind "entity"*\  is not a type name
 * - ``0758``\ 
   - | ``explicit_instantiation_not_in_namespace_scope``\ :
     | explicit instantiation is not allowed in the current scope
 * - ``0759``\ 
   - | ``bad_scope_for_explicit_instantiation``\ :
     | *entity-kind "entity"*\  cannot be explicitly instantiated in the
       current scope
 * - ``0760``\ 
   - | ``multiple_explicit_instantiations``\ :
     | *entity-kind "entity"*\  explicitly instantiated more than once
 * - ``0761``\ 
   - | ``typename_not_in_template``\ :
     | typename may only be used within a template
 * - ``0762``\ 
   - | ``cl_special_subscript_cost_option_only_in_cplusplus``\ :
     | special_subscript_cost option can be used only when compiling C++
 * - ``0763``\ 
   - | ``cl_typename_option_only_in_cplusplus``\ :
     | typename option can be used only when compiling C++
 * - ``0764``\ 
   - | ``cl_implicit_typename_option_only_in_cplusplus``\ :
     | implicit typename option can be used only when compiling C++
 * - ``0765``\ 
   - | ``nonstd_character_at_start_of_macro_def``\ :
     | nonstandard character at start of object-like macro definition
 * - ``0766``\ 
   - | ``exception_spec_override_incompat``\ :
     | exception specification for virtual *entity-kind "entity"*\  is
       incompatible with that of overridden *entity-kind "entity"*\ 
 * - ``0767``\ 
   - | ``pointer_conversion_loses_bits``\ :
     | conversion from pointer to smaller integer
 * - ``0768``\ 
   - | ``generated_exception_spec_override_incompat``\ :
     | exception specification for implicitly declared virtual *entity-kind
       "entity"*\  is incompatible with that of overridden *entity-kind
       "entity"*\ 
 * - ``0769``\ 
   - | ``implicit_call_of_ambiguous_name``\ :
     | *"entity"*\ , implicitly called from *entity-kind "entity"*\ , is
       ambiguous
 * - ``0770``\ 
   - | ``cl_explicit_option_only_in_cplusplus``\ :
     | option "explicit" can be used only when compiling C++
 * - ``0771``\ 
   - | ``explicit_not_allowed``\ :
     | "explicit" is not allowed
 * - ``0772``\ 
   - | ``conflicts_with_predeclared_type_info``\ :
     | declaration conflicts with *"xxxx"*\  (reserved class name)
 * - ``0773``\ 
   - | ``array_member_initialization``\ :
     | only "()" is allowed as initializer for array *entity-kind "entity"*\ 
 * - ``0774``\ 
   - | ``virtual_function_template``\ :
     | "virtual" is not allowed in a function template declaration
 * - ``0775``\ 
   - | ``anon_union_class_member_template``\ :
     | invalid anonymous union -- class member template is not allowed
 * - ``0776``\ 
   - | ``template_depth_mismatch``\ :
     | template nesting depth does not match the previous declaration of
       *entity-kind "entity"*\ 
 * - ``0777``\ 
   - | ``multiple_template_decls_not_allowed``\ :
     | this declaration cannot have multiple "template <...>" clauses
 * - ``0778``\ 
   - | ``cl_old_for_init_option_only_in_cplusplus``\ :
     | option to control the for-init scope can be used only when compiling
       C++
 * - ``0779``\ 
   - | ``redeclaration_of_for_init_decl_name``\ :
     | *"xxxx"*\ , declared in for-loop initialization, may not be
       redeclared in this scope
 * - ``0780``\ 
   - | ``hidden_by_old_for_init``\ :
     | reference is to *entity-kind "entity"*\  (declared at line *xxxx*\ )
       -- under old for-init scoping rules it would have been *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``0781``\ 
   - | ``cl_for_init_diff_warning_option_only_in_cplusplus``\ :
     | option to control warnings on for-init differences can be used only
       when compiling C++
 * - ``0782``\ 
   - | ``unnamed_class_virtual_function_def_missing``\ :
     | definition of virtual *entity-kind "entity"*\  is required here
 * - ``0783``\ 
   - | ``svr4_token_pasting_comment``\ :
     | empty comment interpreted as token-pasting operator "##"
 * - ``0784``\ 
   - | ``storage_class_in_friend_decl``\ :
     | a storage class is not allowed in a friend declaration
 * - ``0785``\ 
   - | ``templ_param_list_not_allowed``\ :
     | template parameter list for *"entity"*\  is not allowed in this
       declaration
 * - ``0786``\ 
   - | ``bad_member_template_sym``\ :
     | *entity-kind "entity"*\  is not a valid class member template
 * - ``0787``\ 
   - | ``bad_member_template_decl``\ :
     | not a valid member class or function template declaration
 * - ``0788``\ 
   - | ``specialization_follows_param_list``\ :
     | a template declaration containing a template parameter list may not
       be followed by an explicit specialization declaration
 * - ``0789``\ 
   - | ``specialization_of_referenced_template``\ :
     | explicit specialization of *entity-kind "entity"*\  must precede the
       first use of *entity-kind "entity"*\ 
 * - ``0790``\ 
   - | ``explicit_specialization_not_in_namespace_scope``\ :
     | explicit specialization is not allowed in the current scope
 * - ``0791``\ 
   - | ``partial_specialization_not_allowed``\ :
     | partial specialization of *entity-kind "entity"*\  is not allowed
 * - ``0792``\ 
   - | ``entity_cannot_be_specialized``\ :
     | *entity-kind "entity"*\  is not an entity that can be explicitly
       specialized
 * - ``0793``\ 
   - | ``specialization_of_referenced_entity``\ :
     | explicit specialization of *entity-kind "entity"*\  must precede its
       first use
 * - ``0794``\ 
   - | ``template_param_in_elab_type``\ :
     | template parameter *"xxxx"*\  may not be used in an elaborated type
       specifier
 * - ``0795``\ 
   - | ``old_specialization_not_allowed``\ :
     | specializing *entity-kind "entity"*\  requires "template<>" syntax
 * - ``0798``\ 
   - | ``cl_old_specializations_option_only_in_cplusplus``\ :
     | option "old_specializations" can be used only when compiling C++
 * - ``0799``\ 
   - | ``nonstd_old_specialization``\ :
     | specializing *entity-kind "entity"*\  without "template<>" syntax is
       nonstandard
 * - ``0800``\ 
   - | ``bad_linkage_for_decl``\ :
     | this declaration may not have extern "C" linkage
 * - ``0801``\ 
   - | ``not_a_template_name``\ :
     | *"xxxx"*\  is not a class or function template name in the current
       scope
 * - ``0802``\ 
   - | ``nonstd_default_arg_on_function_template_redecl``\ :
     | specifying a default argument when redeclaring an unreferenced
       function template is nonstandard
 * - ``0803``\ 
   - | ``default_arg_on_function_template_not_allowed``\ :
     | specifying a default argument when redeclaring an already referenced
       function template is not allowed
 * - ``0804``\ 
   - | ``pm_derived_class_from_virtual_base``\ :
     | cannot convert pointer to member of base class *"type"*\  to pointer
       to member of derived class *"type"*\  -- base class is virtual
 * - ``0805``\ 
   - | ``bad_exception_specification_for_specialization``\ :
     | exception specification is incompatible with that of *entity-kind
       "entity"*\  (declared at line *xxxx*\ ):
 * - ``0806``\ 
   - | ``omitted_exception_specification_on_specialization``\ :
     | allowing all exceptions is incompatible with *entity-kind "entity"*\ 
       (declared at line *xxxx*\ )
 * - ``0807``\ 
   - | ``unexpected_end_of_default_arg``\ :
     | unexpected end of default argument expression
 * - ``0808``\ 
   - | ``default_init_of_reference``\ :
     | default-initialization of reference is not allowed
 * - ``0809``\ 
   - | ``uninitialized_field_with_const_member``\ :
     | uninitialized *entity-kind "entity"*\  has a const member
 * - ``0810``\ 
   - | ``uninitialized_base_class_with_const_member``\ :
     | uninitialized base class *"type"*\  has a const member
 * - ``0811``\ 
   - | ``missing_default_constructor_on_const``\ :
     | const *entity-kind "entity"*\  requires an initializer -- class
       *"type"*\  has no user-provided default constructor
 * - ``0812``\ 
   - | ``missing_default_constructor_on_unnamed_const``\ :
     | const object requires an initializer -- class *"type"*\  has no
       user-provided default constructor
 * - ``0813``\ 
   - | ``cl_impl_extern_c_conv_option_only_in_cplusplus``\ :
     | option "implicit_extern_c_type_conversion" can be used only when
       compiling C++
 * - ``0814``\ 
   - | ``cl_strict_mode_incompatible_with_long_preserving``\ :
     | strict mode is incompatible with long preserving rules
 * - ``0815``\ 
   - | ``useless_type_qualifier_on_return_type``\ :
     | type qualifier on return type is meaningless
 * - ``0816``\ 
   - | ``type_qualifier_on_void_return_type``\ :
     | in a function definition a type qualifier on a "void" return type is
       not allowed
 * - ``0817``\ 
   - | ``static_data_member_not_allowed``\ :
     | static data member declaration is not allowed in this class
 * - ``0818``\ 
   - | ``invalid_declaration``\ :
     | template instantiation resulted in an invalid function declaration
 * - ``0819``\ 
   - | ``ellipsis_not_allowed``\ :
     | "..." is not allowed
 * - ``0820``\ 
   - | ``cl_extern_inline_option_only_in_cplusplus``\ :
     | option "extern_inline" can be used only when compiling C++
 * - ``0821``\ 
   - | ``extern_inline_never_defined``\ :
     | extern inline *entity-kind "entity"*\  was referenced but not defined
 * - ``0822``\ 
   - | ``invalid_destructor_name``\ :
     | invalid destructor name for type *"type"*\ 
 * - ``0824``\ 
   - | ``ambiguous_destructor``\ :
     | destructor reference is ambiguous -- both *entity-kind "entity"*\ 
       and *entity-kind "entity"*\  could be used
 * - ``0825``\ 
   - | ``virtual_inline_never_defined``\ :
     | virtual inline *entity-kind "entity"*\  was never defined
 * - ``0826``\ 
   - | ``unreferenced_function_param``\ :
     | *entity-kind "entity"*\  was never referenced
 * - ``0827``\ 
   - | ``union_already_initialized``\ :
     | only one member of a union may be specified in a constructor
       initializer list
 * - ``0828``\ 
   - | ``no_array_new_and_delete_support``\ :
     | support for "new[]" and "delete[]" is disabled
 * - ``0829``\ 
   - | ``double_for_long_double``\ :
     | "double" used for "long double" in generated C code
 * - ``0830``\ 
   - | ``no_corresponding_delete``\ :
     | *entity-kind "entity"*\  has no corresponding operator delete*xxxx*\ 
       (to be called if an exception is thrown during initialization of an
       allocated object)
 * - ``0831``\ 
   - | ``useless_placement_delete``\ :
     | support for placement delete is disabled
 * - ``0832``\ 
   - | ``no_appropriate_delete``\ :
     | no appropriate operator delete is visible
 * - ``0833``\ 
   - | ``ptr_or_ref_to_incomplete_type``\ :
     | pointer or reference to incomplete type *"type"*\  is not allowed
 * - ``0834``\ 
   - | ``bad_partial_specialization``\ :
     | invalid partial specialization -- *entity-kind "entity"*\  is already
       fully specialized
 * - ``0835``\ 
   - | ``incompatible_exception_specs``\ :
     | incompatible exception specifications
 * - ``0836``\ 
   - | ``returning_ref_to_local_variable``\ :
     | returning reference to local variable
 * - ``0837``\ 
   - | ``nonstd_implicit_int``\ :
     | omission of explicit type is nonstandard ("int" assumed)
 * - ``0838``\ 
   - | ``ambiguous_partial_spec``\ :
     | more than one partial specialization matches the template argument
       list of *entity-kind "entity"*\ 
 * - ``0840``\ 
   - | ``partial_spec_is_primary_template``\ :
     | a template argument list is not allowed in a declaration of a primary
       template
 * - ``0841``\ 
   - | ``default_not_allowed_on_partial_spec``\ :
     | partial specializations may not have default template arguments
 * - ``0842``\ 
   - | ``not_used_in_partial_spec_arg_list``\ :
     | *entity-kind "entity"*\  is not used in or cannot be deduced from the
       template argument list of *entity-kind "entity"*\ 
 * - ``0844``\ 
   - | ``partial_spec_arg_depends_on_templ_param``\ :
     | the template argument list of the partial specialization includes a
       nontype argument whose type depends on a template parameter
 * - ``0845``\ 
   - | ``partial_spec_after_instantiation``\ :
     | this partial specialization would have been used to instantiate
       *entity-kind "entity"*\ 
 * - ``0846``\ 
   - | ``partial_spec_after_instantiation_ambiguous``\ :
     | this partial specialization would have made the instantiation of
       *entity-kind "entity"*\  ambiguous
 * - ``0847``\ 
   - | ``expr_not_integral_or_enum``\ :
     | expression must have integral or enum type
 * - ``0848``\ 
   - | ``expr_not_arithmetic_or_enum``\ :
     | expression must have arithmetic or enum type
 * - ``0849``\ 
   - | ``expr_not_arithmetic_or_enum_or_pointer``\ :
     | expression must have arithmetic, enum, or pointer type
 * - ``0850``\ 
   - | ``cast_not_integral_or_enum``\ :
     | type of cast must be integral or enum
 * - ``0851``\ 
   - | ``cast_not_arithmetic_or_enum_or_pointer``\ :
     | type of cast must be arithmetic, enum, or pointer
 * - ``0852``\ 
   - | ``expr_not_object_pointer``\ :
     | expression must be a pointer to a complete object type
 * - ``0855``\ 
   - | ``different_return_type_on_virtual_function_override``\ :
     | return type is not identical to return type *"type"*\  of overridden
       virtual function *"entity"*\ 
 * - ``0856``\ 
   - | ``cl_guiding_decls_option_only_in_cplusplus``\ :
     | option "guiding_decls" can be used only when compiling C++
 * - ``0857``\ 
   - | ``member_partial_spec_not_in_namespace``\ :
     | a partial specialization of a class template must be declared in the
       namespace of which it is a member
 * - ``0858``\ 
   - | ``pure_virtual_function``\ :
     | *entity-kind "entity"*\  is a pure virtual function
 * - ``0859``\ 
   - | ``no_overrider_for_pure_virtual_function``\ :
     | pure virtual *entity-kind "entity"*\  has no overrider
 * - ``0860``\ 
   - | ``decl_modifiers_ignored``\ :
     | __declspec attributes ignored
 * - ``0861``\ 
   - | ``invalid_char``\ :
     | invalid character in input line
 * - ``0862``\ 
   - | ``incomplete_return_type``\ :
     | function returns incomplete type *"type"*\ 
 * - ``0863``\ 
   - | ``local_pragma_pack``\ :
     | effect of this "#pragma pack" directive is local to *entity-kind
       "entity"*\ 
 * - ``0864``\ 
   - | ``not_a_template``\ :
     | *xxxx*\  is not a template
 * - ``0865``\ 
   - | ``friend_partial_specialization``\ :
     | a friend declaration may not declare a partial specialization
 * - ``0866``\ 
   - | ``exception_specification_ignored``\ :
     | exception specification ignored
 * - ``0867``\ 
   - | ``unexpected_type_for_size_t``\ :
     | declaration of "size_t" does not match the expected type *"type"*\ 
 * - ``0868``\ 
   - | ``exp_gt_not_shift_right``\ :
     | space required between adjacent ">" delimiters of nested template
       argument lists (">>" is the right shift operator)
 * - ``0869``\ 
   - | ``bad_multibyte_char_locale``\ :
     | could not set locale *"xxxx"*\  to allow processing of multibyte
       characters
 * - ``0870``\ 
   - | ``bad_multibyte_char``\ :
     | invalid multibyte character sequence
 * - ``0871``\ 
   - | ``bad_type_from_instantiation``\ :
     | template instantiation resulted in unexpected function type of
       *"type"*\  (the meaning of a name may have changed since the template
       declaration -- the type of the template is *"type"*\ )
 * - ``0872``\ 
   - | ``ambiguous_guiding_decl``\ :
     | ambiguous guiding declaration -- more than one function template
       *"entity"*\  matches type *"type"*\ 
 * - ``0873``\ 
   - | ``non_integral_operation_in_templ_arg``\ :
     | non-integral operation not allowed in nontype template argument
 * - ``0874``\ 
   - | ``cl_embedded_cplusplus_option_only_in_cplusplus``\ :
     | option "embedded_c++" can be used only when compiling C++
 * - ``0875``\ 
   - | ``templates_in_embedded_cplusplus``\ :
     | Embedded C++ does not support templates
 * - ``0876``\ 
   - | ``exceptions_in_embedded_cplusplus``\ :
     | Embedded C++ does not support exception handling
 * - ``0877``\ 
   - | ``namespaces_in_embedded_cplusplus``\ :
     | Embedded C++ does not support namespaces
 * - ``0878``\ 
   - | ``rtti_in_embedded_cplusplus``\ :
     | Embedded C++ does not support run-time type information
 * - ``0879``\ 
   - | ``new_cast_in_embedded_cplusplus``\ :
     | Embedded C++ does not support the new cast syntax
 * - ``0880``\ 
   - | ``using_decl_in_embedded_cplusplus``\ :
     | Embedded C++ does not support using-declarations
 * - ``0881``\ 
   - | ``mutable_in_embedded_cplusplus``\ :
     | Embedded C++ does not support "mutable"
 * - ``0882``\ 
   - | ``multiple_inheritance_in_embedded_cplusplus``\ :
     | Embedded C++ does not support multiple or virtual inheritance
 * - ``0883``\ 
   - | ``cl_invalid_microsoft_version``\ :
     | invalid Microsoft version number: *xxxx*\ 
 * - ``0884``\ 
   - | ``inheritance_kind_already_set``\ :
     | pointer-to-member representation *"xxxx"*\  has already been set for
       *entity-kind "entity"*\ 
 * - ``0885``\ 
   - | ``bad_constructor_type``\ :
     | *"type"*\  cannot be used to designate constructor for *"type"*\ 
 * - ``0886``\ 
   - | ``bad_suffix``\ :
     | invalid suffix on integral constant
 * - ``0887``\ 
   - | ``uuidof_requires_uuid_class_type``\ :
     | operand of \__uuidof must have a class or enum type for which
       \__declspec(uuid("...")) has been specified
 * - ``0888``\ 
   - | ``bad_uuid_string``\ :
     | invalid GUID string in \__declspec(uuid("..."))
 * - ``0889``\ 
   - | ``cl_vla_option_only_in_C``\ :
     | option "vla" can be used only when compiling C
 * - ``0890``\ 
   - | ``vla_with_unspecified_bound_not_allowed``\ :
     | variable length array with unspecified bound is not allowed
 * - ``0891``\ 
   - | ``explicit_template_args_not_allowed``\ :
     | an explicit template argument list is not allowed on this declaration
 * - ``0892``\ 
   - | ``variably_modified_type_not_allowed``\ :
     | an entity with linkage cannot have a type involving a variable length
       array
 * - ``0893``\ 
   - | ``vla_is_not_auto``\ :
     | a variable length array cannot have static storage duration
 * - ``0894``\ 
   - | ``sym_not_a_template``\ :
     | *entity-kind "entity"*\  is not a template
 * - ``0896``\ 
   - | ``expected_template_arg``\ :
     | expected a template argument
 * - ``0898``\ 
   - | ``no_params_with_class_or_enum_type``\ :
     | nonmember operator requires a parameter with class or enum type
 * - ``0899``\ 
   - | ``cl_enum_overloading_option_only_in_cplusplus``\ :
     | option "enum_overloading" can be used only when compiling C++
 * - ``0901``\ 
   - | ``destructor_qualifier_type_mismatch``\ :
     | qualifier of destructor name *"type"*\  does not match type *"type"*\ 
 * - ``0902``\ 
   - | ``type_qualifier_ignored``\ :
     | type qualifier ignored
 * - ``0903``\ 
   - | ``cl_nonstandard_qualifier_deduction_option_only_in_cplusplus``\ :
     | option "nonstd_qualifier_deduction" can be used only when compiling
       C++
 * - ``0904``\ 
   - | ``cannot_define_dllimport_function``\ :
     | a function declared "dllimport" may not be defined
 * - ``0905``\ 
   - | ``bad_declspec_property``\ :
     | incorrect property specification; correct form is
       \__declspec(property(get=name1,put=name2))
 * - ``0906``\ 
   - | ``dupl_get_or_put``\ :
     | property has already been specified
 * - ``0907``\ 
   - | ``declspec_property_not_allowed``\ :
     | __declspec(property) is not allowed on this declaration
 * - ``0908``\ 
   - | ``no_get_property``\ :
     | member is declared with \__declspec(property), but no "get" function
       was specified
 * - ``0909``\ 
   - | ``get_property_function_missing``\ :
     | the \__declspec(property) "get" function *"xxxx"*\  is missing
 * - ``0910``\ 
   - | ``no_put_property``\ :
     | member is declared with \__declspec(property), but no "put" function
       was specified
 * - ``0911``\ 
   - | ``put_property_function_missing``\ :
     | the \__declspec(property) "put" function *"xxxx"*\  is missing
 * - ``0912``\ 
   - | ``dual_lookup_ambiguous_name``\ :
     | ambiguous class member reference -- *entity-kind "entity"*\ 
       (declared at line *xxxx*\ ) used in preference to *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``0916``\ 
   - | ``pm_virtual_base_from_derived_class``\ :
     | cannot convert pointer to member of derived class *"type"*\  to
       pointer to member of base class *"type"*\  -- base class is virtual
 * - ``0917``\ 
   - | ``cl_invalid_instantiation_directory``\ :
     | invalid directory for instantiation files: *xxxx*\ 
 * - ``0918``\ 
   - | ``cl_one_instantiation_per_object_option_only_in_cplusplus``\ :
     | option "one_instantiation_per_object" can be used only when compiling
       C++
 * - ``0921``\ 
   - | ``cl_ii_file_name_incompatible_with_multiple_inputs``\ :
     | an instantiation information file name may not be specified when
       compiling several input files
 * - ``0922``\ 
   - | ``cl_one_instantiation_per_object_incompatible_with_multiple_inputs``\
       :
     | option "one_instantiation_per_object" may not be used when compiling
       several input files
 * - ``0923``\ 
   - | ``cl_ambiguous_option``\ :
     | more than one command line option matches the abbreviation
       "--*xxxx*\ ":
 * - ``0925``\ 
   - | ``cv_qualified_function_type``\ :
     | type qualifiers on function types are ignored
 * - ``0927``\ 
   - | ``cl_late_tiebreaker_option_only_in_cplusplus``\ :
     | late/early tiebreaker option can be used only when compiling C++
 * - ``0928``\ 
   - | ``bad_va_start``\ :
     | incorrect use of va_start
 * - ``0929``\ 
   - | ``bad_va_arg``\ :
     | incorrect use of va_arg
 * - ``0930``\ 
   - | ``bad_va_end``\ :
     | incorrect use of va_end
 * - ``0931``\ 
   - | ``cl_pending_instantiations_option_only_in_cplusplus``\ :
     | pending instantiations option can be used only when compiling C++
 * - ``0932``\ 
   - | ``cl_invalid_import_directory``\ :
     | invalid directory for #import files: *xxxx*\ 
 * - ``0933``\ 
   - | ``cl_import_only_in_microsoft``\ :
     | an import directory can be specified only in Microsoft mode
 * - ``0934``\ 
   - | ``ref_not_allowed_in_union``\ :
     | a member with reference type is not allowed in a union
 * - ``0935``\ 
   - | ``typedef_not_allowed``\ :
     | "typedef" may not be specified here
 * - ``0936``\ 
   - | ``redecl_changes_access``\ :
     | redeclaration of *entity-kind "entity"*\  alters its access
 * - ``0937``\ 
   - | ``qualified_name_required``\ :
     | a class or namespace qualified name is required
 * - ``0938``\ 
   - | ``implicit_int_on_main``\ :
     | return type "int" omitted in declaration of function "main"
 * - ``0939``\ 
   - | ``invalid_inheritance_kind_for_class``\ :
     | pointer-to-member representation *"xxxx"*\  is too restrictive for
       *entity-kind "entity"*\ 
 * - ``0940``\ 
   - | ``implicit_return_from_non_void_function``\ :
     | missing return statement at end of non-void *entity-kind "entity"*\ 
 * - ``0941``\ 
   - | ``duplicate_using_decl``\ :
     | duplicate using-declaration of *"entity"*\  ignored
 * - ``0942``\ 
   - | ``unsigned_enum_bit_field_with_signed_enumerator``\ :
     | enum bit fields are always unsigned, but enum *"type"*\  includes
       negative enumerator
 * - ``0943``\ 
   - | ``cl_class_name_injection_option_only_in_cplusplus``\ :
     | option "class_name_injection" can be used only when compiling C++
 * - ``0944``\ 
   - | ``cl_arg_dependent_lookup_option_only_in_cplusplus``\ :
     | option "arg_dep_lookup" can be used only when compiling C++
 * - ``0945``\ 
   - | ``cl_friend_injection_option_only_in_cplusplus``\ :
     | option "friend_injection" can be used only when compiling C++
 * - ``0946``\ 
   - | ``invalid_name_after_template``\ :
     | name following "template" must be a template
 * - ``0948``\ 
   - | ``local_class_friend_requires_prior_decl``\ :
     | nonstandard local-class friend declaration -- no prior declaration in
       the enclosing scope
 * - ``0949``\ 
   - | ``nonstd_default_arg``\ :
     | specifying a default argument on this declaration is nonstandard
 * - ``0950``\ 
   - | ``cl_nonstd_using_decl_option_only_in_cplusplus``\ :
     | option "nonstd_using_decl" can be used only when compiling C++
 * - ``0951``\ 
   - | ``bad_return_type_on_main``\ :
     | return type of function "main" must be "int"
 * - ``0952``\ 
   - | ``template_parameter_has_class_type``\ :
     | a nontype template parameter may not have class type
 * - ``0953``\ 
   - | ``default_arg_on_member_decl``\ :
     | a default template argument cannot be specified on the definition of
       a member of a class template outside the template
 * - ``0954``\ 
   - | ``return_from_ctor_function_try_block_handler``\ :
     | a return statement is not allowed in a handler of a function try
       block of a constructor
 * - ``0955``\ 
   - | ``no_ordinary_and_extended_designators``\ :
     | ordinary and extended designators cannot be combined in an
       initializer designation
 * - ``0956``\ 
   - | ``no_negative_designator_range``\ :
     | the second subscript must not be smaller than the first
 * - ``0958``\ 
   - | ``cl_extended_designators_option_only_in_C``\ :
     | option "extended_designators" can be used only when compiling C
 * - ``0959``\ 
   - | ``extra_bits_ignored``\ :
     | declared size for bit field is larger than the size of the bit field
       type; truncated to *xxxx*\  bits
 * - ``0960``\ 
   - | ``constructor_type_mismatch``\ :
     | type used as constructor name does not match type *"type"*\ 
 * - ``0961``\ 
   - | ``type_with_no_linkage_in_var_with_linkage``\ :
     | use of a type with no linkage to declare a variable with linkage
 * - ``0962``\ 
   - | ``type_with_no_linkage_in_function``\ :
     | use of a type with no linkage to declare a function
 * - ``0963``\ 
   - | ``return_type_on_constructor``\ :
     | return type may not be specified on a constructor
 * - ``0964``\ 
   - | ``return_type_on_destructor``\ :
     | return type may not be specified on a destructor
 * - ``0965``\ 
   - | ``malformed_universal_character``\ :
     | incorrectly formed universal character name
 * - ``0966``\ 
   - | ``invalid_UCN``\ :
     | universal character name specifies an invalid character
 * - ``0967``\ 
   - | ``UCN_names_basic_char``\ :
     | a universal character name cannot designate a character in the basic
       character set
 * - ``0968``\ 
   - | ``invalid_identifier_UCN``\ :
     | this universal character is not allowed in an identifier
 * - ``0969``\ 
   - | ``VA_ARGS_not_allowed``\ :
     | the identifier \__VA_ARGS\__ can only appear in the replacement lists
       of variadic macros
 * - ``0970``\ 
   - | ``friend_qualification_ignored``\ :
     | the qualifier on this friend declaration is ignored
 * - ``0971``\ 
   - | ``no_range_designator_with_dynamic_init``\ :
     | array range designators cannot be applied to dynamic initializers
 * - ``0972``\ 
   - | ``property_name_not_allowed``\ :
     | property name cannot appear here
 * - ``0973``\ 
   - | ``inline_qualifier_ignored``\ :
     | "inline" used as a function qualifier is ignored
 * - ``0974``\ 
   - | ``cl_compound_literals_option_only_in_C``\ :
     | option "compound_literals" can be used only when compiling C
 * - ``0975``\ 
   - | ``vla_not_allowed``\ :
     | a variable-length array type is not allowed
 * - ``0976``\ 
   - | ``bad_integral_compound_literal``\ :
     | a compound literal is not allowed in an integral constant expression
 * - ``0977``\ 
   - | ``bad_compound_literal_type``\ :
     | a compound literal of type *"type"*\  is not allowed
 * - ``0978``\ 
   - | ``friend_template_in_local_class``\ :
     | a template friend declaration cannot be declared in a local class
 * - ``0979``\ 
   - | ``ambiguous_question_operator``\ :
     | ambiguous "?" operation: second operand of type *"type"*\  can be
       converted to third operand type *"type"*\ , and vice versa
 * - ``0980``\ 
   - | ``bad_call_of_class_object``\ :
     | call of an object of a class type without appropriate operator() or
       conversion functions to pointer-to-function type
 * - ``0982``\ 
   - | ``ambiguous_class_call``\ :
     | there is more than one way an object of type *"type"*\  can be called
       for the argument list:
 * - ``0983``\ 
   - | ``similar_typedef``\ :
     | typedef name has already been declared (with similar type)
 * - ``0984``\ 
   - | ``no_internal_linkage_for_new_or_delete``\ :
     | operator new and operator delete cannot be given internal linkage
 * - ``0985``\ 
   - | ``no_mutable_allowed_on_anonymous_union``\ :
     | storage class "mutable" is not allowed for anonymous unions
 * - ``0986``\ 
   - | ``bad_pch_file``\ :
     | invalid precompiled header file
 * - ``0987``\ 
   - | ``abstract_class_catch_type``\ :
     | abstract class type *"type"*\  is not allowed as catch type:
 * - ``0988``\ 
   - | ``bad_qualified_function_type``\ :
     | a qualified function type cannot be used to declare a nonmember
       function or a static member function
 * - ``0989``\ 
   - | ``bad_qualified_function_type_parameter``\ :
     | a qualified function type cannot be used to declare a parameter
 * - ``0990``\ 
   - | ``ptr_or_ref_to_qualified_function_type``\ :
     | cannot create a pointer or reference to qualified function type
 * - ``0991``\ 
   - | ``nonstd_braces``\ :
     | extra braces are nonstandard
 * - ``0992``\ 
   - | ``bad_cmd_line_macro``\ :
     | invalid macro definition: *xxxx*\ 
 * - ``0993``\ 
   - | ``nonstandard_ptr_minus_ptr``\ :
     | subtraction of pointer types *"type"*\  and *"type"*\  is nonstandard
 * - ``0994``\ 
   - | ``empty_template_param_list``\ :
     | an empty template parameter list is not allowed in a template
       template parameter declaration
 * - ``0995``\ 
   - | ``exp_class``\ :
     | expected "class"
 * - ``0996``\ 
   - | ``struct_not_allowed``\ :
     | the "struct" keyword may not be used when declaring a template
       template parameter
 * - ``0997``\ 
   - | ``virtual_function_decl_hidden``\ :
     | *entity-kind "entity"*\  is hidden by *"entity"*\  -- virtual
       function override intended?
 * - ``0998``\ 
   - | ``no_qualified_friend_definition``\ :
     | a qualified name is not allowed for a friend declaration that is a
       function definition
 * - ``0999``\ 
   - | ``not_compatible_with_templ_templ_param``\ :
     | *entity-kind "entity"*\  is not compatible with *entity-kind
       "entity"*\ 
 * - ``1000``\ 
   - | ``storage_class_requires_function_or_variable``\ :
     | a storage class may not be specified here
 * - ``1001``\ 
   - | ``member_using_must_be_visible_in_direct_base``\ :
     | class member designated by a using-declaration must be visible in a
       direct base class
 * - ``1003``\ 
   - | ``cl_sun_incompatible_with_cfront``\ :
     | Sun mode is incompatible with cfront mode
 * - ``1004``\ 
   - | ``cl_strict_mode_incompatible_with_sun``\ :
     | strict mode is incompatible with Sun mode
 * - ``1005``\ 
   - | ``cl_sun_mode_only_in_cplusplus``\ :
     | Sun mode is only allowed when compiling C++
 * - ``1006``\ 
   - | ``template_template_param_same_name_as_templ_param``\ :
     | a template template parameter cannot have the same name as one of its
       template parameters
 * - ``1007``\ 
   - | ``recursive_def_arg_instantiation``\ :
     | recursive instantiation of default argument
 * - ``1009``\ 
   - | ``bad_template_name``\ :
     | *entity-kind "entity"*\  is not an entity that can be defined
 * - ``1010``\ 
   - | ``destructor_name_must_be_qualified``\ :
     | destructor name must be qualified
 * - ``1011``\ 
   - | ``no_typename_in_friend_class_decl``\ :
     | friend class name may not be introduced with "typename"
 * - ``1012``\ 
   - | ``no_ctor_or_dtor_using_declaration``\ :
     | a using-declaration may not name a constructor or destructor
 * - ``1013``\ 
   - | ``friend_is_nonreal_template``\ :
     | a qualified friend template declaration must refer to a specific
       previously declared template
 * - ``1014``\ 
   - | ``bad_class_template_decl``\ :
     | invalid specifier in class template declaration
 * - ``1015``\ 
   - | ``simple_incompatible_param``\ :
     | argument is incompatible with formal parameter
 * - ``1016``\ 
   - | ``cl_dep_name_option_only_in_cplusplus``\ :
     | option "dep_name" can be used only when compiling C++
 * - ``1017``\ 
   - | ``op_arrow_loop``\ :
     | loop in sequence of "operator->" functions starting at class
       *"type"*\ 
 * - ``1018``\ 
   - | ``not_a_member_class``\ :
     | *entity-kind "entity"*\  has no member class *"xxxx"*\ 
 * - ``1019``\ 
   - | ``name_not_class_in_file_scope``\ :
     | the global scope has no class named *"xxxx"*\ 
 * - ``1020``\ 
   - | ``recursive_inst_of_templ_default_arg``\ :
     | recursive instantiation of template default argument
 * - ``1021``\ 
   - | ``no_access_or_using_decl_in_union``\ :
     | access declarations and using-declarations cannot appear in unions
 * - ``1022``\ 
   - | ``not_class_member``\ :
     | *"entity"*\  is not a class member
 * - ``1023``\ 
   - | ``nonstd_const_member_decl_not_allowed``\ :
     | nonstandard member constant declaration is not allowed
 * - ``1024``\ 
   - | ``cl_ignore_std_option_only_in_cplusplus``\ :
     | option "ignore_std" can be used only when compiling C++
 * - ``1025``\ 
   - | ``cl_parse_nonclass_templates_option_only_in_cplusplus``\ :
     | option "parse_templates" can be used only when compiling C++
 * - ``1026``\ 
   - | ``cl_dep_name_requires_parse_nonclass_templates``\ :
     | option "dep_name" cannot be used with "no_parse_templates"
 * - ``1027``\ 
   - | ``cl_incompatible_language_modes``\ :
     | language modes specified are incompatible
 * - ``1028``\ 
   - | ``invalid_nested_class_redecl``\ :
     | invalid redeclaration of nested class
 * - ``1029``\ 
   - | ``flexible_array_member_not_allowed``\ :
     | type containing an unknown-size array is not allowed
 * - ``1030``\ 
   - | ``static_variable_in_inline_function``\ :
     | a variable with static storage duration cannot be defined within an
       inline function
 * - ``1031``\ 
   - | ``bad_linkage_of_ref_within_inline_function``\ :
     | an entity with internal linkage cannot be referenced within an inline
       function with external linkage
 * - ``1032``\ 
   - | ``type_generic_function_mismatch``\ :
     | argument type *"type"*\  does not match this type-generic function
       macro
 * - ``1034``\ 
   - | ``friend_cannot_add_default_arguments``\ :
     | friend declaration cannot add default arguments to previous
       declaration
 * - ``1035``\ 
   - | ``cannot_be_declared_in_scope``\ :
     | *entity-kind "entity"*\  cannot be declared in this scope
 * - ``1036``\ 
   - | ``id_can_only_appear_in_function``\ :
     | the reserved identifier *"xxxx"*\  may only be used inside a function
 * - ``1037``\ 
   - | ``invalid_identifier_start_UCN``\ :
     | this universal character cannot begin an identifier
 * - ``1038``\ 
   - | ``exp_string_literal``\ :
     | expected a string literal
 * - ``1039``\ 
   - | ``unrecognized_stdc_pragma``\ :
     | unrecognized STDC pragma
 * - ``1040``\ 
   - | ``bad_stdc_pragma_arg``\ :
     | expected "ON", "OFF", or "DEFAULT"
 * - ``1041``\ 
   - | ``stdc_pragma_not_allowed_here``\ :
     | a STDC pragma may only appear between declarations in the global
       scope or before any statements or declarations in a block scope
 * - ``1042``\ 
   - | ``bad_va_copy``\ :
     | incorrect use of va_copy
 * - ``1043``\ 
   - | ``only_applies_to_float_types``\ :
     | *xxxx*\  can only be used with floating-point types
 * - ``1044``\ 
   - | ``complex_type_not_allowed``\ :
     | complex type is not allowed
 * - ``1045``\ 
   - | ``invalid_designator_kind``\ :
     | invalid designator kind
 * - ``1046``\ 
   - | ``inexact_fp_conversion``\ :
     | floating-point value cannot be represented exactly
 * - ``1047``\ 
   - | ``bad_complex_operation_result``\ :
     | complex floating-point operation result is out of range
 * - ``1048``\ 
   - | ``real_imaginary_conversion``\ :
     | conversion between real and imaginary yields zero
 * - ``1049``\ 
   - | ``cannot_initialize_flexible_array_member``\ :
     | an initializer cannot be specified for a flexible array member
 * - ``1050``\ 
   - | ``imaginary_times_assign``\ :
     | imaginary \*= imaginary sets the left-hand operand to zero
 * - ``1051``\ 
   - | ``undeclared_parameter``\ :
     | standard requires that *entity-kind "entity"*\  be given a type by a
       subsequent declaration ("int" assumed)
 * - ``1052``\ 
   - | ``inline_never_defined``\ :
     | a definition is required for inline *entity-kind "entity"*\ 
 * - ``1053``\ 
   - | ``conversion_to_pointer_loses_bits``\ :
     | conversion from integer to smaller pointer
 * - ``1054``\ 
   - | ``missing_floating_point_type``\ :
     | a floating-point type must be included in the type specifier for a
       _Complex or _Imaginary type
 * - ``1055``\ 
   - | ``type_decl_in_anon_union``\ :
     | types cannot be declared in anonymous unions
 * - ``1056``\ 
   - | ``returning_ptr_to_local_variable``\ :
     | returning pointer to local variable
 * - ``1057``\ 
   - | ``returning_ptr_to_local_temp``\ :
     | returning pointer to local temporary
 * - ``1058``\ 
   - | ``cl_export_template_option_only_in_cplusplus``\ :
     | option "export" can be used only when compiling C++
 * - ``1059``\ 
   - | ``cl_export_template_requires_dep_name``\ :
     | option "export" cannot be used with "no_dep_name"
 * - ``1060``\ 
   - | ``cl_export_template_requires_no_implicit_include``\ :
     | option "export" cannot be used with "implicit_include"
 * - ``1061``\ 
   - | ``corresp_decl_incompatible``\ :
     | declaration of *entity-kind "entity"*\  is incompatible with a
       declaration in another translation unit
 * - ``1062``\ 
   - | ``corresp_decl_at``\ :
     | the other declaration is at line *xxxx*\ 
 * - ``1065``\ 
   - | ``field_cannot_involve_vla_type``\ :
     | a field declaration cannot have a type involving a variable length
       array
 * - ``1066``\ 
   - | ``entity_differs_in_other_trans_unit``\ :
     | declaration of *entity-kind "entity"*\  had a different meaning
       during compilation of *"xxxx"*\ 
 * - ``1067``\ 
   - | ``exp_template``\ :
     | expected "template"
 * - ``1068``\ 
   - | ``export_on_instantiation``\ :
     | "export" cannot be used on an explicit instantiation
 * - ``1069``\ 
   - | ``bad_decl_for_export``\ :
     | "export" cannot be used on this declaration
 * - ``1070``\ 
   - | ``exported_in_unnamed_namespace``\ :
     | a member of an unnamed namespace cannot be declared "export"
 * - ``1071``\ 
   - | ``export_after_definition``\ :
     | a template cannot be declared "export" after it has been defined
 * - ``1072``\ 
   - | ``labeled_declaration``\ :
     | a declaration cannot have a label
 * - ``1073``\ 
   - | ``no_export_support``\ :
     | support for exported templates is disabled
 * - ``1075``\ 
   - | ``entity_defined_in_other_trans_unit``\ :
     | *entity-kind "entity"*\  already defined during compilation of
       *"xxxx"*\ 
 * - ``1076``\ 
   - | ``entity_defined_twice``\ :
     | *entity-kind "entity"*\  already defined in another translation unit
 * - ``1077``\ 
   - | ``based_var_cannot_be_local``\ :
     | a nonstatic local variable may not be used in a \__based specification
 * - ``1078``\ 
   - | ``cl_list_make_dependencies_incompatible_with_multiple_trans_units``\ 
       :
     | the option to list makefile dependencies may not be specified when
       compiling more than one translation unit
 * - ``1080``\ 
   - | ``cl_pp_output_incompatible_with_multiple_trans_units``\ :
     | the option to generate preprocessed output may not be specified when
       compiling more than one translation unit
 * - ``1081``\ 
   - | ``field_name_conflicts_with_class``\ :
     | a field with the same name as its class cannot be declared in a class
       with a user-declared constructor
 * - ``1082``\ 
   - | ``cl_implicit_include_incompatible_with_multiple_trans_units``\ :
     | "implicit_include" cannot be used when compiling more than one
       translation unit
 * - ``1083``\ 
   - | ``corrupted_export_template_file``\ :
     | exported template file *"xxxx"*\  is corrupted
 * - ``1084``\ 
   - | ``exported_instantiation_and_specialized``\ :
     | *entity-kind "entity"*\  cannot be instantiated -- it has been
       explicitly specialized in the translation unit containing the
       exported definition
 * - ``1086``\ 
   - | ``unqual_named_function_with_qual_object``\ :
     | the object has type qualifiers that are not compatible with the
       member *entity-kind "entity"*\ 
 * - ``1087``\ 
   - | ``no_matching_function_due_to_selector``\ :
     | no instance of *entity-kind "entity"*\  matches the argument list and
       object (the object has type qualifiers that prevent a match)
 * - ``1088``\ 
   - | ``mode_incompatible_with_type``\ :
     | an attribute specifies a mode incompatible with *"type"*\ 
 * - ``1089``\ 
   - | ``no_type_of_specified_width``\ :
     | there is no type with the width specified
 * - ``1090``\ 
   - | ``bad_attribute_alignment``\ :
     | invalid alignment value specified by attribute
 * - ``1091``\ 
   - | ``attribute_does_not_apply_to_type``\ :
     | invalid attribute for *"type"*\ 
 * - ``1094``\ 
   - | ``arguments_provided_for_attribute``\ :
     | attribute *"xxxx"*\  does not take arguments
 * - ``1096``\ 
   - | ``exp_attribute_name``\ :
     | expected an attribute name
 * - ``1097``\ 
   - | ``unrecognized_attribute``\ :
     | unknown attribute *"xxxx"*\ 
 * - ``1098``\ 
   - | ``attribute_not_allowed``\ :
     | attributes may not appear here
 * - ``1099``\ 
   - | ``invalid_argument_to_attribute``\ :
     | invalid argument to attribute *"xxxx"*\ 
 * - ``1101``\ 
   - | ``assigned_goto_requires_void_ptr``\ :
     | in "goto \*expr", expr must have type "void \*"
 * - ``1102``\ 
   - | ``nonstd_assigned_goto``\ :
     | "goto \*expr" is nonstandard
 * - ``1103``\ 
   - | ``nonstd_address_of_label``\ :
     | taking the address of a label is nonstandard
 * - ``1104``\ 
   - | ``cl_duplicate_file_name``\ :
     | file name specified more than once: *xxxx*\ 
 * - ``1105``\ 
   - | ``warning_directive``\ :
     | #warning directive: *xxxx*\ 
 * - ``1107``\ 
   - | ``transparent_type_is_not_union``\ :
     | the "transparent_union" attribute only applies to unions, and
       *"type"*\  is not a union
 * - ``1108``\ 
   - | ``transparent_attribute_ignored``\ :
     | the "transparent_union" attribute is ignored on incomplete types
 * - ``1109``\ 
   - | ``union_cannot_be_transparent_sym``\ :
     | *"type"*\  cannot be transparent because *entity-kind "entity"*\ 
       does not have the same size as the first field
 * - ``1110``\ 
   - | ``union_cannot_be_transparent``\ :
     | *"type"*\  cannot be transparent because it has a field of type
       *"type"*\  which is not the same size as the first field
 * - ``1112``\ 
   - | ``attribute_does_not_apply_to_local_variable``\ :
     | attribute *"xxxx"*\  does not apply to local variables
 * - ``1113``\ 
   - | ``attributes_in_rout_defn``\ :
     | attributes are not permitted in a function definition
 * - ``1115``\ 
   - | ``invalid_case_range``\ :
     | the second constant in a case range must be larger than the first
 * - ``1116``\ 
   - | ``asm_name_in_rout_defn``\ :
     | an asm name is not permitted in a function definition
 * - ``1117``\ 
   - | ``asm_name_in_typedef``\ :
     | an asm name is ignored in a typedef
 * - ``1118``\ 
   - | ``bad_reg_name``\ :
     | unknown register name "*xxxx*\ "
 * - ``1120``\ 
   - | ``bad_asm_constraint_modifier``\ :
     | unknown asm constraint modifier '*xxxx*\ '
 * - ``1121``\ 
   - | ``bad_asm_constraint_letter``\ :
     | unknown asm constraint letter '*xxxx*\ '
 * - ``1122``\ 
   - | ``missing_constraint_letter``\ :
     | asm operand has no constraint letter
 * - ``1123``\ 
   - | ``asm_output_must_have_output_mod``\ :
     | an asm output operand must have one of the '=' or '+' modifiers
 * - ``1124``\ 
   - | ``asm_input_must_not_have_output_mod``\ :
     | an asm input operand may not have the '=' or '+' modifiers
 * - ``1127``\ 
   - | ``register_used_twice``\ :
     | register "*xxxx*\ " used more than once
 * - ``1128``\ 
   - | ``register_used_and_clobbered``\ :
     | register "*xxxx*\ " is both used and clobbered
 * - ``1129``\ 
   - | ``register_clobbered_twice``\ :
     | register "*xxxx*\ " clobbered more than once
 * - ``1130``\ 
   - | ``fixed_register_used``\ :
     | register "*xxxx*\ " has a fixed purpose and may not be used in an asm
       statement
 * - ``1131``\ 
   - | ``fixed_register_clobbered``\ :
     | register "*xxxx*\ " has a fixed purpose and may not be clobbered in
       an asm statement
 * - ``1132``\ 
   - | ``empty_clobbers_list``\ :
     | an empty clobbers list must be omitted entirely
 * - ``1133``\ 
   - | ``exp_asm_operand``\ :
     | expected an asm operand
 * - ``1134``\ 
   - | ``exp_asm_clobber``\ :
     | expected a register to clobber
 * - ``1135``\ 
   - | ``format_rout_not_varargs``\ :
     | "format" attribute requires an ellipsis parameter or parameter pack
 * - ``1136``\ 
   - | ``subst_arg_is_not_variable``\ :
     | first substitution argument is not the first variable argument
 * - ``1137``\ 
   - | ``fmt_arg_does_not_exist``\ :
     | format argument index is greater than number of parameters
 * - ``1138``\ 
   - | ``fmt_arg_is_not_string``\ :
     | format argument does not have string type
 * - ``1139``\ 
   - | ``template_not_in_template``\ :
     | the "template" keyword used for syntactic disambiguation may only be
       used within a template
 * - ``1142``\ 
   - | ``attr_requires_func_type``\ :
     | attribute *"xxxx"*\  does not apply to non-function type *"type"*\ 
 * - ``1143``\ 
   - | ``nonobject_pointer_arithmetic``\ :
     | arithmetic on pointer to void or function type
 * - ``1144``\ 
   - | ``invalid_storage_class_in_for_init``\ :
     | storage class must be auto or register
 * - ``1145``\ 
   - | ``va_arg_would_have_been_promoted``\ :
     | *"type"*\  would have been promoted to *"type"*\  when passed through
       the ellipsis parameter; use the latter type instead
 * - ``1146``\ 
   - | ``not_a_base_class_member``\ :
     | *"xxxx"*\  is not a base class member
 * - ``1147``\ 
   - | ``super_after_scope``\ :
     | __super cannot appear after "::"
 * - ``1148``\ 
   - | ``super_not_in_class``\ :
     | __super may only be used in a class scope
 * - ``1149``\ 
   - | ``unqualified_super``\ :
     | __super must be followed by "::"
 * - ``1151``\ 
   - | ``mangled_name_too_long``\ :
     | mangled name is too long
 * - ``1152``\ 
   - | ``aliased_name_undeclared``\ :
     | declaration aliased to undefined entity *"xxxx"*\ 
 * - ``1153``\ 
   - | ``aliased_name_bad_kind``\ :
     | declaration does not match its alias *entity-kind "entity"*\ 
 * - ``1154``\ 
   - | ``alias_cannot_have_definition``\ :
     | entity declared as alias cannot have definition
 * - ``1155``\ 
   - | ``vla_size_ignored``\ :
     | variable-length array field type will be treated as zero-length array
       field type
 * - ``1156``\ 
   - | ``gcc_lvalue_cast_ignored``\ :
     | nonstandard cast on lvalue ignored
 * - ``1157``\ 
   - | ``cl_invalid_flag_name``\ :
     | unrecognized flag name: *xxxx*\ 
 * - ``1158``\ 
   - | ``qualified_void_return_type``\ :
     | void return type cannot be qualified
 * - ``1159``\ 
   - | ``auto_ignored``\ :
     | the auto specifier is ignored here (invalid in standard C/C++)
 * - ``1160``\ 
   - | ``alignment_reduction_ignored``\ :
     | a reduction in alignment without the "packed" attribute is ignored
 * - ``1161``\ 
   - | ``corresp_member_template_is_different_kind``\ :
     | a member template corresponding to *"entity"*\  is declared as a
       template of a different kind in another translation unit
 * - ``1162``\ 
   - | ``excess_initializers_ignored``\ :
     | excess initializers are ignored
 * - ``1163``\ 
   - | ``va_start_requires_ellipsis_function``\ :
     | va_start can appear only in a function with an ellipsis parameter
 * - ``1164``\ 
   - | ``cl_short_enums_requires_gcc_mode``\ :
     | the "short_enums" option is only valid in GNU C and GNU C++ modes
 * - ``1165``\ 
   - | ``bad_export_info_file``\ :
     | invalid export information file *"xxxx"*\  at line number *xxxx*\ 
 * - ``1166``\ 
   - | ``statement_expression_in_function_only``\ :
     | statement expressions are only allowed in block scope
 * - ``1168``\ 
   - | ``asm_name_on_auto_variable``\ :
     | an asm name is ignored on a non-register automatic variable
 * - ``1170``\ 
   - | ``unrecognized_upc_pragma``\ :
     | unrecognized UPC pragma
 * - ``1171``\ 
   - | ``mismatched_shared_block_size``\ :
     | shared block size does not match one previously specified
 * - ``1172``\ 
   - | ``ambiguous_block_size_spec``\ :
     | bracketed expression is assumed to be a block size specification
       rather than an array dimension
 * - ``1173``\ 
   - | ``shared_block_size_must_be_positive``\ :
     | the block size of a shared array must be greater than zero
 * - ``1174``\ 
   - | ``multiple_block_sizes``\ :
     | multiple block sizes not allowed
 * - ``1175``\ 
   - | ``nonshared_strict_relaxed``\ :
     | strict or relaxed requires shared
 * - ``1176``\ 
   - | ``threads_constant_not_allowed``\ :
     | THREADS not allowed in this context
 * - ``1177``\ 
   - | ``shared_block_size_too_large``\ :
     | block size specified exceeds the maximum value of *xxxx*\ 
 * - ``1178``\ 
   - | ``function_returning_shared``\ :
     | function returning shared is not allowed
 * - ``1180``\ 
   - | ``shared_nonthreads_dim``\ :
     | one dimension of an array of a shared type must be a multiple of
       THREADS when the number of threads is nonconstant
 * - ``1181``\ 
   - | ``shared_inside_struct``\ :
     | shared type inside a struct or union is not allowed
 * - ``1182``\ 
   - | ``shared_parameter``\ :
     | parameters may not have shared types
 * - ``1183``\ 
   - | ``threads_dimension_requires_definite_block_size``\ :
     | a dynamic THREADS dimension requires a definite block size
 * - ``1184``\ 
   - | ``bad_shared_storage_class``\ :
     | shared variables must be static or extern
 * - ``1185``\ 
   - | ``nonshared_blocksizeof``\ :
     | argument of upc_blocksizeof is a pointer to a shared type (not shared
       type itself)
 * - ``1186``\ 
   - | ``nested_upc_forall``\ :
     | affinity expression ignored in nested upc_forall
 * - ``1187``\ 
   - | ``exit_forall``\ :
     | branching into or out of a upc_forall loop is not allowed
 * - ``1188``\ 
   - | ``bad_affinity``\ :
     | affinity expression must have a shared type or point to a shared type
 * - ``1189``\ 
   - | ``shared_affinity_type``\ :
     | affinity has shared type (not pointer to shared)
 * - ``1190``\ 
   - | ``upc_shared_void_comparison``\ :
     | shared void\* types can only be compared for equality
 * - ``1191``\ 
   - | ``cl_upc_requires_ansi_c_dialect``\ :
     | UPC mode is incompatible with C++ and K&R modes
 * - ``1192``\ 
   - | ``null_char_ignored``\ :
     | null (zero) character in input line ignored
 * - ``1193``\ 
   - | ``null_char_in_string``\ :
     | null (zero) character in string or character constant
 * - ``1194``\ 
   - | ``null_char_in_header_name``\ :
     | null (zero) character in header name
 * - ``1195``\ 
   - | ``for_init_hides_declaration``\ :
     | declaration in for-initializer hides a declaration in the surrounding
       scope
 * - ``1196``\ 
   - | ``for_init_hidden_declaration``\ :
     | the hidden declaration is at line *xxxx*\ 
 * - ``1197``\ 
   - | ``prototype_lost``\ :
     | the prototype declaration of *entity-kind "entity"*\  (declared at
       line *xxxx*\ ) is ignored after this unprototyped redeclaration
 * - ``1199``\ 
   - | ``bad_linkage_for_redefine_extname``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) must have
       external C linkage
 * - ``1200``\ 
   - | ``declaration_hides_for_init``\ :
     | variable declaration hides declaration in for-initializer
 * - ``1201``\ 
   - | ``typedef_in_elab_type``\ :
     | typedef *"xxxx"*\  may not be used in an elaborated type specifier
 * - ``1202``\ 
   - | ``call_of_zero``\ :
     | call of zero constant ignored
 * - ``1203``\ 
   - | ``handler_redeclares_parameter``\ :
     | parameter *"xxxx"*\  may not be redeclared in a catch clause of
       function try block
 * - ``1204``\ 
   - | ``specialization_out_of_namespace``\ :
     | the initial explicit specialization of *entity-kind "entity"*\  must
       be declared in the namespace containing the template
 * - ``1205``\ 
   - | ``cc_clobber_ignored``\ :
     | "cc" clobber ignored
 * - ``1206``\ 
   - | ``invalid_token_after_template``\ :
     | "template" must be followed by an identifier
 * - ``1207``\ 
   - | ``mythread_constant_not_allowed``\ :
     | MYTHREAD not allowed in this context
 * - ``1208``\ 
   - | ``bad_upc_shared_pointer_layout_qualifier``\ :
     | layout qualifier cannot qualify pointer to shared
 * - ``1209``\ 
   - | ``bad_upc_shared_array_layout_qualifier``\ :
     | layout qualifier cannot qualify an incomplete array
 * - ``1210``\ 
   - | ``decl_hides_catch_parameter``\ :
     | declaration of *"xxxx"*\  hides handler parameter
 * - ``1211``\ 
   - | ``nonstd_ignored_array_cast``\ :
     | nonstandard cast to array type ignored
 * - ``1212``\ 
   - | ``invalid_pragma_operator``\ :
     | this pragma cannot be used in a _Pragma operator (a #pragma directive
       must be used)
 * - ``1213``\ 
   - | ``field_uses_tail_padding``\ :
     | field uses tail padding of a base class
 * - ``1214``\ 
   - | ``gnu_may_use_bit_padding``\ :
     | GNU C++ compilers may use bit field padding
 * - ``1215``\ 
   - | ``deprecated_entity``\ :
     | *entity-kind "entity"*\  was declared deprecated
 * - ``1216``\ 
   - | ``field_with_asm_name_not_allowed``\ :
     | an asm name is not allowed on a nonstatic member declaration
 * - ``1217``\ 
   - | ``unrecognized_format_function_type``\ :
     | unrecognized format function type *"xxxx"*\  ignored
 * - ``1218``\ 
   - | ``base_uses_tail_padding``\ :
     | base class *"entity"*\  uses tail padding of base class *"entity"*\ 
 * - ``1219``\ 
   - | ``bad_variable_for_init_priority``\ :
     | the "init_priority" attribute can only be used for definitions of
       static data members and namespace scope variables of class types
 * - ``1220``\ 
   - | ``init_priority_reserved``\ :
     | requested initialization priority is reserved for internal use
 * - ``1221``\ 
   - | ``hidden_anonymous_union_field``\ :
     | this anonymous union/struct field is hidden by *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``1222``\ 
   - | ``invalid_error_number``\ :
     | invalid error number
 * - ``1223``\ 
   - | ``invalid_error_tag``\ :
     | invalid error tag
 * - ``1224``\ 
   - | ``exp_error_argument``\ :
     | expected an error number or error tag
 * - ``1225``\ 
   - | ``size_affected_by_tail_padding``\ :
     | size of class is affected by tail padding
 * - ``1226``\ 
   - | ``nonlocal_label_reference``\ :
     | labels can be referenced only in function definitions
 * - ``1227``\ 
   - | ``branch_into_statement_expr``\ :
     | transfer of control into a statement expression is not allowed
 * - ``1229``\ 
   - | ``bad_statement_in_statement_expr``\ :
     | this statement is not allowed inside of a statement expression
 * - ``1230``\ 
   - | ``class_def_in_statement_expr``\ :
     | a class that is not trivially copyable cannot be defined inside a
       statement expression
 * - ``1232``\ 
   - | ``dyn_local_static_in_statement_expr``\ :
     | a dynamically-initialized local static variable is not allowed inside
       of a statement expression
 * - ``1233``\ 
   - | ``vla_in_statement_expr``\ :
     | a variable-length array is not allowed inside of a statement
       expression
 * - ``1234``\ 
   - | ``statement_expr_in_default_arg``\ :
     | a statement expression is not allowed inside of a default argument
 * - ``1235``\ 
   - | ``ptr_func_ptr_data_conv``\ :
     | nonstandard conversion between pointer to function and pointer to data
 * - ``1236``\ 
   - | ``interface_cannot_have_virtual_base``\ :
     | interface types cannot have virtual base classes
 * - ``1237``\ 
   - | ``interface_cannot_have_private_or_protected``\ :
     | interface types cannot specify "private" or "protected"
 * - ``1238``\ 
   - | ``interface_must_derive_from_interface``\ :
     | interface types can only derive from other interface types
 * - ``1239``\ 
   - | ``type_is_interface``\ :
     | *"type"*\  is an interface type
 * - ``1240``\ 
   - | ``interface_cannot_have_typedef``\ :
     | interface types cannot have typedef members
 * - ``1241``\ 
   - | ``interface_cannot_have_ctor_or_dtor``\ :
     | interface types cannot have user-declared constructors or destructors
 * - ``1242``\ 
   - | ``interface_cannot_have_operator``\ :
     | interface types cannot have user-declared member operators
 * - ``1243``\ 
   - | ``interface_cannot_be_local``\ :
     | interface types cannot be declared in functions
 * - ``1245``\ 
   - | ``interface_cannot_have_data_member``\ :
     | interface types cannot have data members
 * - ``1246``\ 
   - | ``interface_cannot_have_friend``\ :
     | interface types cannot contain friend declarations
 * - ``1248``\ 
   - | ``interface_cannot_be_nested_class``\ :
     | interface types cannot be nested class types
 * - ``1249``\ 
   - | ``interface_cannot_have_member_templates``\ :
     | interface types cannot have member templates
 * - ``1250``\ 
   - | ``interface_cannot_have_static_members``\ :
     | interface types cannot have static member functions
 * - ``1251``\ 
   - | ``invalid_microsoft_pragma_operator``\ :
     | this pragma cannot be used in a \__pragma operator (a #pragma
       directive must be used)
 * - ``1252``\ 
   - | ``qualifier_must_be_base_class``\ :
     | qualifier must be base class of *"type"*\ 
 * - ``1253``\ 
   - | ``invalid_selective_overrider_declaration``\ :
     | declaration must correspond to a pure virtual member function in the
       indicated base class
 * - ``1254``\ 
   - | ``integer_overflow_class_internal``\ :
     | integer overflow in internal computation due to size or complexity of
       *"type"*\ 
 * - ``1255``\ 
   - | ``integer_overflow_internal``\ :
     | integer overflow in internal computation
 * - ``1256``\ 
   - | ``invalid_type_for_w64``\ :
     | __w64 can only be specified on int, long, and pointer types
 * - ``1257``\ 
   - | ``ilp64_will_narrow``\ :
     | potentially narrowing conversion when compiled in an environment
       where int, long, or pointer types are 64 bits wide
 * - ``1258``\ 
   - | ``value_of_pragma_pack_show``\ :
     | current value of pragma pack is *xxxx*\ 
 * - ``1259``\ 
   - | ``pragma_pack_show_args_ignored``\ :
     | arguments for pragma pack(show) are ignored
 * - ``1262``\ 
   - | ``multiple_declspec_align``\ :
     | earlier \__declspec(align(...)) ignored
 * - ``1263``\ 
   - | ``exp_ms_attr_enum_value``\ :
     | expected an argument value for the *"xxxx"*\  attribute parameter
 * - ``1264``\ 
   - | ``invalid_ms_attr_enum_value``\ :
     | invalid argument value for the *"xxxx"*\  attribute parameter
 * - ``1265``\ 
   - | ``exp_ms_attr_bool_value``\ :
     | expected a boolean value for the *"xxxx"*\  attribute parameter
 * - ``1266``\ 
   - | ``positional_after_named``\ :
     | a positional argument cannot follow a named argument in an attribute
 * - ``1267``\ 
   - | ``invalid_ms_attr_name``\ :
     | attribute *"xxxx"*\  has no parameter named *"xxxx"*\ 
 * - ``1268``\ 
   - | ``exp_ms_attr_arg_list``\ :
     | expected an argument list for the *"xxxx"*\  attribute
 * - ``1269``\ 
   - | ``exp_comma_or_rbracket``\ :
     | expected a "," or "]"
 * - ``1270``\ 
   - | ``duplicate_ms_attr_arg``\ :
     | attribute argument *"xxxx"*\  has already been given a value
 * - ``1271``\ 
   - | ``cannot_assign_to_ms_attr``\ :
     | a value cannot be assigned to the *"xxxx"*\  attribute
 * - ``1272``\ 
   - | ``ptr_incomplete_throw``\ :
     | a throw expression may not have pointer-to-incomplete type
 * - ``1273``\ 
   - | ``alignof_incomplete_type``\ :
     | alignment-of operator applied to incomplete type
 * - ``1274``\ 
   - | ``invalid_use_of_standalone_ms_attr``\ :
     | *"xxxx"*\  may only be used as a standalone attribute
 * - ``1275``\ 
   - | ``invalid_use_of_ms_attr``\ :
     | *"xxxx"*\  attribute cannot be used here
 * - ``1277``\ 
   - | ``ms_attr_not_allowed``\ :
     | attributes are not allowed here
 * - ``1278``\ 
   - | ``invalid_ms_attr_uuid_value``\ :
     | invalid argument value for the *"xxxx"*\  attribute parameter
 * - ``1279``\ 
   - | ``too_many_ms_attr_args``\ :
     | too many attribute arguments
 * - ``1280``\ 
   - | ``conv_from_inaccessible_base_class``\ :
     | conversion from inaccessible base class *"type"*\  is not allowed
 * - ``1281``\ 
   - | ``cl_export_template_requires_distinct_templ_sigs``\ :
     | option "export" requires distinct template signatures
 * - ``1282``\ 
   - | ``mixed_string_concatenation``\ :
     | string literals with different character kinds cannot be concatenated
 * - ``1283``\ 
   - | ``no_gnu_virtual_base_gap``\ :
     | GNU layout bug not emulated because it places virtual base
       *"entity"*\  outside *"entity"*\  object boundaries
 * - ``1284``\ 
   - | ``gnu_virtual_base_gap``\ :
     | virtual base *"entity"*\  placed outside *"entity"*\  object
       boundaries
 * - ``1285``\ 
   - | ``nonstd_qualifier_in_namespace_member_decl``\ :
     | nonstandard qualified name in namespace member declaration
 * - ``1286``\ 
   - | ``declspec_align_reduction_ignored``\ :
     | reduction in alignment ignored
 * - ``1287``\ 
   - | ``const_ignored``\ :
     | const qualifier ignored
 * - ``1289``\ 
   - | ``invalid_asm_qualifiers``\ :
     | invalid GNU asm qualifiers
 * - ``1290``\ 
   - | ``non_pod_passed_to_ellipsis``\ :
     | a class type that is not trivially copyable passed through ellipsis
 * - ``1291``\ 
   - | ``non_pod_va_arg``\ :
     | a class type that cannot be trivially copied cannot be fetched by
       va_arg
 * - ``1292``\ 
   - | ``nonstd_fixed_point_suffix``\ :
     | the 'u' or 'U' suffix must appear before the 'l' or 'L' suffix in a
       fixed-point literal
 * - ``1293``\ 
   - | ``cl_fixed_point_option_only_in_C``\ :
     | option "fixed_point" can be used only when compiling C
 * - ``1294``\ 
   - | ``integer_may_not_fit_in_fixed_point_result``\ :
     | integer operand may cause fixed-point overflow
 * - ``1295``\ 
   - | ``bad_fixed_point_value``\ :
     | fixed-point constant is out of range
 * - ``1296``\ 
   - | ``inexact_fxp_conversion``\ :
     | fixed-point value cannot be represented exactly
 * - ``1297``\ 
   - | ``c99_constant_in_unsigned_long_long_range``\ :
     | constant is too large for long long; given unsigned long long type
       (nonstandard)
 * - ``1298``\ 
   - | ``bad_upc_shared_void_pointer_layout_qualifier``\ :
     | layout qualifier cannot qualify pointer to shared void
 * - ``1299``\ 
   - | ``duplicate_threads_dim``\ :
     | duplicate THREADS in multidimensional array type
 * - ``1300``\ 
   - | ``bad_strong_using_scope``\ :
     | a strong using-directive may only appear in a namespace scope
 * - ``1301``\ 
   - | ``probable_guiding_friend``\ :
     | *entity-kind "entity"*\  declares a non-template function -- add <>
       to refer to a template instance
 * - ``1302``\ 
   - | ``operation_may_not_fit_in_fixed_point_result``\ :
     | operation may cause fixed-point overflow
 * - ``1303``\ 
   - | ``expr_not_integral_or_enum_or_fixed_point``\ :
     | expression must have integral, enum, or fixed-point type
 * - ``1304``\ 
   - | ``expr_not_integral_or_fixed_point``\ :
     | expression must have integral or fixed-point type
 * - ``1305``\ 
   - | ``noreturn_function_does_return``\ :
     | function declared with "noreturn" does return
 * - ``1306``\ 
   - | ``asm_name_conflict``\ :
     | asm name ignored because it conflicts with a previous declaration
 * - ``1307``\ 
   - | ``duplicate_typedef_in_class``\ :
     | class member typedef may not be redeclared
 * - ``1308``\ 
   - | ``taking_address_of_temporary``\ :
     | taking the address of a temporary
 * - ``1309``\ 
   - | ``attribute_ignored_on_incomplete_class_decl``\ :
     | attributes are ignored on a class declaration that is not also a
       definition
 * - ``1310``\ 
   - | ``implicit_fixed_point_to_floating_point_conversion``\ :
     | fixed-point value implicitly converted to floating-point type
 * - ``1311``\ 
   - | ``no_classification_for_fixed_point_type``\ :
     | fixed-point types have no classification
 * - ``1312``\ 
   - | ``fixed_template_parameter``\ :
     | a template parameter may not have fixed-point type
 * - ``1313``\ 
   - | ``hex_fp_constant``\ :
     | hexadecimal floating-point constants are not allowed
 * - ``1314``\ 
   - | ``cl_named_address_spaces_option_only_in_C``\ :
     | option "named_address_spaces" can be used only when compiling C
 * - ``1315``\ 
   - | ``float_to_fixed_conversion``\ :
     | floating-point value does not fit in required fixed-point type
 * - ``1316``\ 
   - | ``inexact_fixed_conversion``\ :
     | value cannot be converted to fixed-point value exactly
 * - ``1317``\ 
   - | ``fixed_sign_change``\ :
     | fixed-point conversion resulted in a change of sign
 * - ``1318``\ 
   - | ``integer_to_fixed_conversion``\ :
     | integer value does not fit in required fixed-point type
 * - ``1319``\ 
   - | ``bad_fixed_operation_result``\ :
     | fixed-point operation result is out of range
 * - ``1320``\ 
   - | ``multiple_named_address_spaces``\ :
     | multiple named address spaces
 * - ``1321``\ 
   - | ``bad_storage_class_for_named_address_space_variable``\ :
     | variable with automatic storage duration cannot be stored in a named
       address space
 * - ``1322``\ 
   - | ``type_with_named_address_space_not_allowed``\ :
     | type cannot be qualified with named address space
 * - ``1323``\ 
   - | ``named_address_space_on_function_type``\ :
     | function type cannot be qualified with named address space
 * - ``1324``\ 
   - | ``field_type_cannot_be_qualified_with_named_address_space``\ :
     | field type cannot be qualified with named address space
 * - ``1325``\ 
   - | ``fixed_to_float_conversion``\ :
     | fixed-point value does not fit in required floating-point type
 * - ``1326``\ 
   - | ``fixed_to_integer_conversion``\ :
     | fixed-point value does not fit in required integer type
 * - ``1327``\ 
   - | ``fixed_to_fixed_conversion``\ :
     | value does not fit in required fixed-point type
 * - ``1328``\ 
   - | ``cl_named_registers_option_only_in_C``\ :
     | option "named_registers" can be used only when compiling C
 * - ``1329``\ 
   - | ``named_register_not_allowed``\ :
     | a named-register storage class is not allowed here
 * - ``1330``\ 
   - | ``register_storage_class_conflict``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) redeclared with
       incompatible named-register storage class
 * - ``1331``\ 
   - | ``aliased_variable_cannot_have_register_storage_class``\ :
     | named-register storage class cannot be specified for aliased variable
 * - ``1332``\ 
   - | ``register_in_use``\ :
     | named-register storage specifier is already in use
 * - ``1333``\ 
   - | ``embedded_c_option_incompatible_with_individual_feature_options``\ :
     | option "embedded_c" cannot be combined with options to control
       individual Embedded C features
 * - ``1334``\ 
   - | ``cl_invalid_edg_base_directory``\ :
     | invalid EDG_BASE directory: *xxxx*\ 
 * - ``1336``\ 
   - | ``bad_predef_macro_line``\ :
     | invalid predefined macro entry at line *xxxx*\ : *xxxx*\ 
 * - ``1337``\ 
   - | ``bad_macro_mode_name``\ :
     | invalid macro mode name *"xxxx"*\ 
 * - ``1338``\ 
   - | ``bad_predef_macro_redef``\ :
     | incompatible redefinition of predefined macro *"xxxx"*\ 
 * - ``1339``\ 
   - | ``missing_named_register_storage_class``\ :
     | redeclaration of *entity-kind "entity"*\  (declared at line *xxxx*\ )
       is missing a named-register storage class
 * - ``1340``\ 
   - | ``register_too_small``\ :
     | named register is too small for the type of the variable
 * - ``1341``\ 
   - | ``no_named_register_for_array``\ :
     | arrays cannot be declared with named-register storage class
 * - ``1342``\ 
   - | ``enum_const_cast``\ :
     | const_cast to enum type is nonstandard
 * - ``1343``\ 
   - | ``cl_embedded_c_option_only_in_C``\ :
     | option "embedded_c" can be used only when compiling C
 * - ``1344``\ 
   - | ``named_address_space_not_allowed``\ :
     | a named address space qualifier is not allowed here
 * - ``1345``\ 
   - | ``bad_initializer_for_array_with_unspecified_bound``\ :
     | an empty initializer is invalid for an array with unspecified bound
 * - ``1346``\ 
   - | ``incomplete_class_return_type``\ :
     | function returns incomplete class type *"type"*\ 
 * - ``1347``\ 
   - | ``out_of_class_initializer_ignored``\ :
     | *entity-kind "entity"*\  has already been initialized; the
       out-of-class initializer will be ignored
 * - ``1348``\ 
   - | ``local_variable_hidden``\ :
     | declaration hides *entity-kind "entity"*\  (declared at line *xxxx*\ )
 * - ``1349``\ 
   - | ``named_address_space_for_parameter``\ :
     | a parameter cannot be allocated in a named address space
 * - ``1350``\ 
   - | ``bad_float_or_fixed_suffix``\ :
     | invalid suffix on fixed-point or floating-point constant
 * - ``1351``\ 
   - | ``register_in_address_space``\ :
     | a register variable cannot be allocated in a named address space
 * - ``1352``\ 
   - | ``bad_stdc_fx_overflow_pragma_arg``\ :
     | expected "SAT" or "DEFAULT"
 * - ``1353``\ 
   - | ``no_corresponding_member_delete``\ :
     | *entity-kind "entity"*\  has no corresponding member operator
       delete*xxxx*\  (to be called if an exception is thrown during
       initialization of an allocated object)
 * - ``1354``\ 
   - | ``dll_thread_conflict``\ :
     | a thread-local variable cannot be declared with "dllimport" or
       "dllexport"
 * - ``1355``\ 
   - | ``function_returning_named_address_space``\ :
     | a function return type cannot be qualified with a named address space
 * - ``1356``\ 
   - | ``cannot_initialize_destructible_flexible_array``\ :
     | an initializer cannot be specified for a flexible array member whose
       elements have a nontrivial destructor
 * - ``1357``\ 
   - | ``cannot_initialize_indirect_flexible_array``\ :
     | an initializer cannot be specified for an indirect flexible array
       member
 * - ``1358``\ 
   - | ``cl_invalid_gnu_version``\ :
     | invalid GNU version number: *xxxx*\ 
 * - ``1359``\ 
   - | ``attribute_after_parenthesized_initializer``\ :
     | variable attributes appearing after a parenthesized initializer are
       ignored
 * - ``1360``\ 
   - | ``gcc_use_of_cast_as_lvalue``\ :
     | the result of this cast cannot be used as an lvalue
 * - ``1361``\ 
   - | ``unsigned_fixed_point_negation``\ :
     | negation of an unsigned fixed-point value
 * - ``1364``\ 
   - | ``register_name_on_nonregister``\ :
     | register names can only be used for register variables
 * - ``1365``\ 
   - | ``void_named_register``\ :
     | named-register variables cannot have void type
 * - ``1367``\ 
   - | ``parameter_with_link_scope_specifier``\ :
     | parameters cannot have link scope specifiers
 * - ``1368``\ 
   - | ``multiple_link_scope_specifiers``\ :
     | multiple link scope specifiers
 * - ``1369``\ 
   - | ``link_scope_requires_external_linkage``\ :
     | link scope specifiers can only appear on functions and variables with
       external linkage
 * - ``1370``\ 
   - | ``link_scope_relaxation``\ :
     | a redeclaration cannot weaken a link scope
 * - ``1371``\ 
   - | ``invalid_link_scope``\ :
     | link scope specifier not allowed on this declaration
 * - ``1372``\ 
   - | ``nonstd_qualifier_in_global_scope_decl``\ :
     | nonstandard qualified name in global scope declaration
 * - ``1373``\ 
   - | ``impl_narrowing_64_bit_int``\ :
     | implicit conversion of a 64-bit integral type to a smaller integral
       type (potential portability problem)
 * - ``1374``\ 
   - | ``expl_narrowing_64_bit_int``\ :
     | explicit conversion of a 64-bit integral type to a smaller integral
       type (potential portability problem)
 * - ``1375``\ 
   - | ``pointer_conversion_to_same_size_int``\ :
     | conversion from pointer to same-sized integral type (potential
       portability problem)
 * - ``1377``\ 
   - | ``friend_specifier_ignored``\ :
     | friend specifier is not allowed in a class definition; friend
       specifier is ignored
 * - ``1378``\ 
   - | ``cannot_use_thread_local_storage``\ :
     | only static and extern variables can use thread-local storage
 * - ``1379``\ 
   - | ``multiple_thread_local_storage_specifiers``\ :
     | multiple thread-local storage specifiers
 * - ``1380``\ 
   - | ``virtual_function_never_defined``\ :
     | virtual *entity-kind "entity"*\  was not defined (and cannot be
       defined elsewhere because it is a member of an unnamed namespace)
 * - ``1381``\ 
   - | ``stray_carriage_return``\ :
     | carriage return character in source line outside of comment or
       character/string literal
 * - ``1382``\ 
   - | ``expr_not_fixed_point``\ :
     | expression must have fixed-point type
 * - ``1383``\ 
   - | ``invalid_access_specifier``\ :
     | invalid use of access specifier is ignored
 * - ``1384``\ 
   - | ``ptr_conv_to_bool``\ :
     | pointer converted to bool
 * - ``1385``\ 
   - | ``ptr_to_member_conv_to_bool``\ :
     | pointer-to-member converted to bool
 * - ``1386``\ 
   - | ``storage_specifier_ignored``\ :
     | storage specifier ignored
 * - ``1387``\ 
   - | ``dll_interface_ignored_on_class_template``\ :
     | dllexport and dllimport are ignored on class templates
 * - ``1388``\ 
   - | ``base_class_has_different_dll_interface``\ :
     | base class dllexport/dllimport specification differs from that of the
       derived class
 * - ``1389``\ 
   - | ``redeclaration_adds_dll_interface``\ :
     | redeclaration cannot add dllexport/dllimport to *"entity"*\ 
       (declared at line *xxxx*\ )
 * - ``1390``\ 
   - | ``dll_interface_conflict_dllexport_assumed``\ :
     | dllexport/dllimport conflict with *"entity"*\  (declared at line
       *xxxx*\ ); dllexport assumed
 * - ``1391``\ 
   - | ``dllimport_defined``\ :
     | cannot define dllimport entity
 * - ``1392``\ 
   - | ``dll_interface_requires_external_linkage``\ :
     | dllexport/dllimport requires external linkage
 * - ``1393``\ 
   - | ``class_and_member_have_dll_interface``\ :
     | a member of a class declared with dllexport/dllimport cannot itself
       be declared with such a specifier
 * - ``1394``\ 
   - | ``field_without_dll_interface``\ :
     | field of class type without a DLL interface used in a class with a
       DLL interface
 * - ``1395``\ 
   - | ``microsoft_parenthesized_member``\ :
     | parenthesized member declaration is nonstandard
 * - ``1396``\ 
   - | ``white_space_inside_splice``\ :
     | white space between backslash and newline in line splice ignored
 * - ``1397``\ 
   - | ``dll_interface_conflict_none_assumed``\ :
     | dllexport/dllimport conflict with *"entity"*\  (declared at line
       *xxxx*\ ); dllimport/dllexport dropped
 * - ``1398``\ 
   - | ``bad_nonstd_anonymous_union_field``\ :
     | invalid member for anonymous member class -- class *"type"*\  has a
       disallowed member function
 * - ``1399``\ 
   - | ``nonstd_reinterpret_cast``\ :
     | nonstandard reinterpret_cast
 * - ``1400``\ 
   - | ``positional_format_specifier_zero``\ :
     | positional format specifier cannot be zero
 * - ``1401``\ 
   - | ``nonlocal_vla_not_allowed``\ :
     | a local class cannot reference a variable-length array type from an
       enclosing function
 * - ``1402``\ 
   - | ``class_and_inherited_member_instance_have_dll_interface``\ :
     | member *entity-kind "entity"*\  (declared at line *xxxx*\ ) already
       has an explicit dllexport/dllimport specifier
 * - ``1403``\ 
   - | ``vla_in_return_type``\ :
     | a variable-length array is not allowed in a function return type
 * - ``1404``\ 
   - | ``ptr_to_member_of_vla_type``\ :
     | variable-length array type is not allowed in pointer to member of
       type *"type"*\ 
 * - ``1405``\ 
   - | ``statement_expr_with_vla_type``\ :
     | the result of a statement expression cannot have a type involving a
       variable-length array
 * - ``1406``\ 
   - | ``trigraph_ignored``\ :
     | support for trigraphs is disabled
 * - ``1407``\ 
   - | ``attribute_requires_external_linkage``\ :
     | the *"xxxx"*\  attribute can only appear on functions and variables
       with external linkage
 * - ``1408``\ 
   - | ``cl_strict_mode_incompatible_with_ignore_std``\ :
     | strict mode is incompatible with treating namespace std as an alias
       for the global namespace
 * - ``1414``\ 
   - | ``invalid_symbolic_asm_operand_name``\ :
     | invalid symbolic operand name *"xxxx"*\ 
 * - ``1415``\ 
   - | ``match_limit_for_symbolic_asm_operand``\ :
     | a symbolic match constraint must refer to one of the first ten
       operands
 * - ``1416``\ 
   - | ``if_exists_not_allowed``\ :
     | use of \__if_exists is not supported in this context
 * - ``1417``\ 
   - | ``if_exists_not_closed``\ :
     | __if_exists block not closed in the same scope in which it was opened
 * - ``1418``\ 
   - | ``bad_init_for_thread_local``\ :
     | thread-local variable cannot be dynamically initialized
 * - ``1419``\ 
   - | ``unaligned_qualifier_dropped``\ :
     | conversion drops "\__unaligned" qualifier
 * - ``1420``\ 
   - | ``insufficient_enum_range``\ :
     | some enumerator values cannot be represented by the integral type
       underlying the enum type
 * - ``1421``\ 
   - | ``friend_class_template_default_arg_not_allowed``\ :
     | default argument is not allowed on a friend class template declaration
 * - ``1422``\ 
   - | ``multi_char_literal``\ :
     | multicharacter character literal (potential portability problem)
 * - ``1423``\ 
   - | ``exp_class_type``\ :
     | expected a class, struct, or union type
 * - ``1424``\ 
   - | ``offsetof_nonfield``\ :
     | second operand of offsetof must be a field
 * - ``1425``\ 
   - | ``offsetof_bit_field``\ :
     | second operand of offsetof may not be a bit field
 * - ``1426``\ 
   - | ``offsetof_virtual_base_member``\ :
     | cannot apply offsetof to a member of a virtual base
 * - ``1427``\ 
   - | ``offset_in_non_POD_nonstandard``\ :
     | offsetof applied to a type other than a standard-layout class
 * - ``1428``\ 
   - | ``default_arg_on_member_friend``\ :
     | default arguments are not allowed on a friend declaration of a member
       function
 * - ``1429``\ 
   - | ``default_arg_requires_friend_to_be_definition``\ :
     | default arguments are not allowed on friend declarations that are not
       definitions
 * - ``1430``\ 
   - | ``redeclaration_of_friend_with_default_args``\ :
     | redeclaration of *entity-kind "entity"*\  (declared at line *xxxx*\ )
       previously declared as a friend with default arguments is not allowed
 * - ``1431``\ 
   - | ``bad_qualifier_for_nested_class_decl``\ :
     | invalid qualifier for *"type"*\  (a derived class is not allowed here)
 * - ``1432``\ 
   - | ``bad_qualifier_for_delayed_class_definition``\ :
     | invalid qualifier for definition of class *"type"*\ 
 * - ``1433``\ 
   - | ``no_prior_push_macro``\ :
     | no prior push_macro for *"xxxx"*\ 
 * - ``1434``\ 
   - | ``wide_string_not_allowed``\ :
     | wide string literal not allowed
 * - ``1436``\ 
   - | ``feature_requires_c``\ :
     | *"xxxx"*\  is only allowed in C
 * - ``1437``\ 
   - | ``microsoft_ptr_width_must_follow_star``\ :
     | __ptr32 and \__ptr64 must follow a "\*"
 * - ``1438``\ 
   - | ``microsoft_ptr_width_conflict``\ :
     | __ptr32 and \__ptr64 cannot both apply
 * - ``1439``\ 
   - | ``name_must_be_prototype_instantiation``\ :
     | template argument list of *"xxxx"*\  must match the parameter list
 * - ``1440``\ 
   - | ``incomplete_class_type``\ :
     | an incomplete class type is not allowed
 * - ``1441``\ 
   - | ``complex_integral_type``\ :
     | complex integral types are not supported
 * - ``1442``\ 
   - | ``real_and_imag_require_complex_argument``\ :
     | __real and \__imag can only be applied to complex values
 * - ``1443``\ 
   - | ``real_and_imag_applied_to_real_value``\ :
     | __real/\__imag applied to real value
 * - ``1444``\ 
   - | ``deprecated_entity_with_custom_message``\ :
     | *entity-kind "entity"*\  was declared deprecated (*"xxxx"*\ )
 * - ``1446``\ 
   - | ``dll_interface_in_unnamed_namespace``\ :
     | dllimport/dllexport applied to a member of an unnamed namespace
 * - ``1447``\ 
   - | ``thiscall_requires_nonstatic_member``\ :
     | __thiscall can only appear on nonstatic member function declarations
 * - ``1448``\ 
   - | ``vararg_thiscall``\ :
     | __thiscall not allowed on function with ellipsis parameter
 * - ``1449``\ 
   - | ``specialization_of_referenced_entity_pos``\ :
     | explicit specialization of *entity-kind "entity"*\  must precede its
       first use (at line *xxxx*\ )
 * - ``1450``\ 
   - | ``sealed_base_class``\ :
     | a sealed class type cannot be used as a base class
 * - ``1451``\ 
   - | ``duplicate_class_modifier``\ :
     | duplicate class modifier
 * - ``1452``\ 
   - | ``function_modifiers_abstract_and_sealed``\ :
     | a member function cannot have both the "abstract" and "sealed"
       modifiers
 * - ``1453``\ 
   - | ``pure_specifier_on_sealed_member``\ :
     | a sealed member cannot be pure virtual
 * - ``1454``\ 
   - | ``function_modifier_requires_virtual_function``\ :
     | nonvirtual function cannot be declared with "abstract" or "sealed"
       modifier
 * - ``1455``\ 
   - | ``override_member_does_not_override``\ :
     | member function declared with "override" does not override a base
       class member
 * - ``1456``\ 
   - | ``override_of_sealed_function``\ :
     | cannot override sealed *entity-kind "entity"*\  (declared at line
       *xxxx*\ )
 * - ``1457``\ 
   - | ``type_is_declared_abstract``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) was declared
       with the class modifier "abstract"
 * - ``1534``\ 
   - | ``duplicate_function_modifier``\ :
     | duplicate function modifier
 * - ``1535``\ 
   - | ``no_char16_t_representation``\ :
     | invalid character for char16_t literal
 * - ``1537``\ 
   - | ``cl_unrecognized_calling_convention``\ :
     | unrecognized calling convention *xxxx*\ , must be one of:
 * - ``1541``\ 
   - | ``enum_base_type_must_be_integral``\ :
     | underlying type of enum type must be an integral type
 * - ``1542``\ 
   - | ``enum_base_type_too_limited``\ :
     | some enumerator constants cannot be represented by *"type"*\ 
 * - ``1543``\ 
   - | ``feature_not_allowed_in_current_mode``\ :
     | *"xxxx"*\  not allowed in current mode
 * - ``1544``\ 
   - | ``cl_type_traits_helpers_option_only_in_cplusplus``\ :
     | type traits helpers option can be used only when compiling C++
 * - ``1545``\ 
   - | ``gnu_sentinel_attribute_requires_ellipsis``\ :
     | attribute "sentinel" requires an ellipsis parameter
 * - ``1546``\ 
   - | ``invalid_gnu_sentinel_argument``\ :
     | argument must be a constant null pointer value
 * - ``1547``\ 
   - | ``no_gnu_sentinel_argument``\ :
     | insufficient number of arguments for sentinel value
 * - ``1548``\ 
   - | ``gnu_sentinel_must_be_ellipsis_argument``\ :
     | sentinel argument must correspond to an ellipsis parameter
 * - ``1549``\ 
   - | ``implementation_key_outside_mapping_region``\ :
     | __declspec(implementation_key(...) can appear only between #pragma
       start_map_region and #pragma stop_map_region
 * - ``1550``\ 
   - | ``start_map_region_ignored``\ :
     | #pragma start_map_region already active: pragma ignored
 * - ``1551``\ 
   - | ``stop_map_region_ignored``\ :
     | no #pragma start_map_region is currently active: pragma ignored
 * - ``1552``\ 
   - | ``var_used_as_destructor``\ :
     | *entity-kind "entity"*\  cannot be used to name a destructor (a type
       name is required)
 * - ``1553``\ 
   - | ``empty_wide_character``\ :
     | nonstandard empty wide character literal treated as L'\0'
 * - ``1554``\ 
   - | ``invalid_typename_specifier``\ :
     | "typename" may not be specified here
 * - ``1555``\ 
   - | ``no_default_delete_in_virtual_dtor``\ :
     | a non-placement operator delete must be visible in a class with a
       virtual destructor
 * - ``1556``\ 
   - | ``name_linkage_mismatch_for_variable``\ :
     | name linkage conflicts with previous declaration of *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``1557``\ 
   - | ``alias_loop``\ :
     | alias creates cycle of aliased entities
 * - ``1559``\ 
   - | ``register_mapped_variable_cannot_have_initializer``\ :
     | a variable with static storage duration allocated in a specific
       register cannot be declared with an initializer
 * - ``1560``\ 
   - | ``register_mapped_variable_must_be_POD``\ :
     | a variable allocated in a specific register must be trivially copyable
 * - ``1561``\ 
   - | ``predef_macro_redefined``\ :
     | predefined meaning of *"entity"*\  discarded
 * - ``1563``\ 
   - | ``designator_for_non_POD``\ :
     | class type not suitable for use with designators
 * - ``1565``\ 
   - | ``nonstandard_anonymous_union_qualifier``\ :
     | anonymous union qualifier is nonstandard
 * - ``1566``\ 
   - | ``anonymous_union_qualifier_ignored``\ :
     | anonymous union qualifier is ignored
 * - ``1568``\ 
   - | ``struct_declspec_ignored_in_C_mode``\ :
     | __declspec(*xxxx*\ ) ignored (it has no meaning for a C struct)
 * - ``1569``\ 
   - | ``nonstandard_secondary_decl_specifiers``\ :
     | specifiers after comma between declarations are nonstandard
 * - ``1570``\ 
   - | ``secondary_specifier_ignored``\ :
     | nonstandard specifier ignored
 * - ``1571``\ 
   - | ``enum_attribute_ignored``\ :
     | attributes are ignored on an enum declaration that is not also a
       definition
 * - ``1572``\ 
   - | ``reference_declared_mutable``\ :
     | declaring a reference with "mutable" is nonstandard
 * - ``1573``\ 
   - | ``array_condition_always_true``\ :
     | a condition declaration for an array is always true
 * - ``1574``\ 
   - | ``static_assert``\ :
     | static assertion failed with *"xxxx"*\ 
 * - ``1575``\ 
   - | ``gnu_visibility_conflict``\ :
     | visibility attribute ignored because it conflicts with a previous
       declaration
 * - ``1576``\ 
   - | ``pcc_field_ambiguity``\ :
     | field name resolves to more than one offset -- see *"entity"*\ 
       (declared at line *xxxx*\ ) and *"entity"*\  (declared at line
       *xxxx*\ )
 * - ``1577``\ 
   - | ``not_a_field_name``\ :
     | *"xxxx"*\  is not a field name
 * - ``1578``\ 
   - | ``case_label_conflict``\ :
     | case label value has already appeared in this switch at line *xxxx*\ 
 * - ``1579``\ 
   - | ``member_cannot_have_internal_linkage``\ :
     | a member function cannot have internal linkage
 * - ``1580``\ 
   - | ``builtin_function_hidden``\ :
     | declaration hides built-in *entity-kind "entity"*\ 
 * - ``1581``\ 
   - | ``builtin_function_overloaded``\ :
     | declaration overloads built-in *entity-kind "entity"*\ 
 * - ``1582``\ 
   - | ``cl_list_macros_incompatible_with_multiple_trans_units``\ :
     | the option to list macro definitions may not be specified when
       compiling more than one translation unit
 * - ``1583``\ 
   - | ``lparen_after_function``\ :
     | unexpected parenthesis after declaration of *entity-kind "entity"*\ 
       (malformed parameter list or invalid initializer?)
 * - ``1584``\ 
   - | ``nonstandard_parenthesized_string_initializer``\ :
     | parentheses around a string initializer are nonstandard
 * - ``1586``\ 
   - | ``auto_variable_in_own_initializer``\ :
     | a variable declared with an auto type specifier cannot appear in its
       own initializer
 * - ``1587``\ 
   - | ``cannot_deduce_auto_type``\ :
     | cannot deduce "auto" type
 * - ``1588``\ 
   - | ``auto_brace_initialization_not_allowed``\ :
     | initialization with "{...}" is not allowed for "auto" type
 * - ``1589``\ 
   - | ``auto_type_in_array_type``\ :
     | "auto" type cannot appear in top-level array type
 * - ``1590``\ 
   - | ``auto_type_in_function_type``\ :
     | "auto" type cannot appear in top-level function type
 * - ``1591``\ 
   - | ``invalid_member_constant_type``\ :
     | a member of type *"type"*\  cannot have an in-class initializer
 * - ``1592``\ 
   - | ``member_constant_not_const``\ :
     | a member with an in-class initializer must be const
 * - ``1593``\ 
   - | ``auto_type_requires_initializer``\ :
     | cannot deduce "auto" type (initializer required)
 * - ``1594``\ 
   - | ``inconsistent_deduction_of_auto``\ :
     | "auto" type is *"type"*\  for this entity, but was previously implied
       to be *"type"*\ 
 * - ``1595``\ 
   - | ``bad_constructor_decl``\ :
     | invalid constructor declaration
 * - ``1596``\ 
   - | ``bad_type_qualifier``\ :
     | invalid use of a type qualifier
 * - ``1597``\ 
   - | ``abstract_or_sealed_on_union``\ :
     | a union cannot be abstract or sealed
 * - ``1598``\ 
   - | ``auto_not_allowed_here``\ :
     | "auto" is not allowed here
 * - ``1599``\ 
   - | ``unfinished_base_class``\ :
     | definition of base class type not completed yet
 * - ``1600``\ 
   - | ``static_extern_template``\ :
     | "extern template" cannot refer to a specialization of static
       *entity-kind "entity"*\  (declared at line *xxxx*\ )
 * - ``1601``\ 
   - | ``extern_template_follows_instantiation``\ :
     | "extern template" cannot follow explicit instantiation of
       *entity-kind "entity"*\ 
 * - ``1602``\ 
   - | ``bad_declspec_restrict_return``\ :
     | __declspec(restrict) requires a function returning a pointer type
 * - ``1603``\ 
   - | ``cl_report_gnu_extensions_requires_gnu_mode``\ :
     | the "report_gnu_extensions" option is only valid in GNU C and GNU C++
       modes
 * - ``1604``\ 
   - | ``vla_is_nonstandard``\ :
     | variable-length array types are nonstandard
 * - ``1605``\ 
   - | ``designator_is_nonstandard``\ :
     | designators are nonstandard
 * - ``1606``\ 
   - | ``extended_designator_is_gnu_extension``\ :
     | this designator syntax is a GNU extension
 * - ``1607``\ 
   - | ``compound_literal_is_nonstandard``\ :
     | compound literals are nonstandard
 * - ``1608``\ 
   - | ``statement_expression_is_gnu_extension``\ :
     | statement expressions are a GNU extension
 * - ``1609``\ 
   - | ``asm_name_after_definition``\ :
     | asm name ignored for previously defined entity
 * - ``1610``\ 
   - | ``attribute_is_gnu_extension``\ :
     | attributes are a GNU extension
 * - ``1611``\ 
   - | ``asm_operand_spec_is_gnu_extension``\ :
     | extended asm syntax is a GNU feature
 * - ``1612``\ 
   - | ``volatile_asm_is_gnu_extension``\ :
     | volatile asm declarations are a GNU extension
 * - ``1613``\ 
   - | ``asm_name_is_gnu_extension``\ :
     | asm name specifiers are a GNU extension
 * - ``1614``\ 
   - | ``gnu_restrict_is_nonstandard``\ :
     | the "\__restrict" qualifier is nonstandard
 * - ``1615``\ 
   - | ``typeof_is_gnu_extension``\ :
     | "typeof" is a GNU extension
 * - ``1616``\ 
   - | ``typedef_modification_is_nonstandard``\ :
     | modifying the size or signedness of a typedef is nonstandard
 * - ``1617``\ 
   - | ``zero_length_array_is_gnu_extension``\ :
     | zero-length arrays are a GNU extension
 * - ``1618``\ 
   - | ``flexible_array_is_nonstandard``\ :
     | flexible array members are nonstandard
 * - ``1619``\ 
   - | ``nonnull_on_nonpointer``\ :
     | attribute "nonnull" references nonpointer parameter
 * - ``1620``\ 
   - | ``nonnull_parameter_number_too_large``\ :
     | argument for attribute "nonnull" is larger than number of parameters
 * - ``1621``\ 
   - | ``no_pointer_parameters``\ :
     | no parameter has pointer type
 * - ``1622``\ 
   - | ``null_argument_for_nonnull_parameter``\ :
     | null argument provided for parameter marked with attribute "nonnull"
 * - ``1623``\ 
   - | ``access_prevents_dtor_generation``\ :
     | the destructor for *"type"*\  has been suppressed because the
       destructor for *"type"*\  is inaccessible
 * - ``1624``\ 
   - | ``suppressed_dtor_needed``\ :
     | the suppressed destructor for *"type"*\  is needed
 * - ``1625``\ 
   - | ``inline_gnu_noinline_conflict``\ :
     | routine is both "inline" and "noinline"
 * - ``1626``\ 
   - | ``invalid_cleanup_routine``\ :
     | invalid cleanup routine
 * - ``1627``\ 
   - | ``attribute_cleanup_requires_automatic_storage``\ :
     | attribute "cleanup" requires automatic storage duration
 * - ``1628``\ 
   - | ``attribute_cleanup_for_parameter``\ :
     | attribute "cleanup" does not apply to parameters
 * - ``1629``\ 
   - | ``bad_type_for_cleanup_routine``\ :
     | cleanup routine has invalid type
 * - ``1630``\ 
   - | ``nonstandard_conversion_for_cleanup``\ :
     | call of cleanup routine requires suspect conversion
 * - ``1631``\ 
   - | ``microsoft_ptr_signedness_must_follow_star``\ :
     | __sptr and \__uptr must follow a "\*"
 * - ``1632``\ 
   - | ``microsoft_ptr_signedness_conflict``\ :
     | __sptr and \__uptr cannot both be specified
 * - ``1633``\ 
   - | ``microsoft_ptr_sign_extension``\ :
     | widening pointer conversion from *"type"*\  to *"type"*\  extends
       sign bit
 * - ``1634``\ 
   - | ``microsoft_ptr_signedness_on_ptr_to_member``\ :
     | __sptr and \__uptr don't apply to pointer-to-member types
 * - ``1635``\ 
   - | ``const_mbr_suppresses_copy_asgn_decl``\ :
     | the declaration of the copy assignment operator for *"type"*\  has
       been suppressed because *entity-kind "entity"*\  is const
 * - ``1636``\ 
   - | ``ref_mbr_suppresses_copy_asgn_decl``\ :
     | the declaration of the copy assignment operator for *"type"*\  has
       been suppressed because *entity-kind "entity"*\  has reference type
 * - ``1637``\ 
   - | ``subobj_copy_asgn_decl_suppressed``\ :
     | the declaration of the copy assignment operator for *"type"*\  has
       been suppressed because that of *"type"*\  was suppressed
 * - ``1638``\ 
   - | ``ambig_suppresses_copy_asgn_decl``\ :
     | the declaration of the copy assignment operator for *"type"*\  has
       been suppressed because that of *"type"*\  is ambiguous
 * - ``1639``\ 
   - | ``access_suppresses_copy_asgn_decl``\ :
     | the declaration of the copy assignment operator for *"type"*\  has
       been suppressed because that of *"type"*\  is inaccessible
 * - ``1640``\ 
   - | ``subobj_copy_ctor_decl_suppressed``\ :
     | the declaration of the copy constructor for *"type"*\  has been
       suppressed because that of *"type"*\  was suppressed
 * - ``1641``\ 
   - | ``ambig_suppresses_copy_ctor_decl``\ :
     | the declaration of the copy constructor for *"type"*\  has been
       suppressed because that of *"type"*\  is ambiguous
 * - ``1642``\ 
   - | ``access_suppresses_copy_ctor_decl``\ :
     | the declaration of the copy constructor for *"type"*\  has been
       suppressed because that of *"type"*\  is inaccessible
 * - ``1643``\ 
   - | ``inaccessible_dtor_not_invoked``\ :
     | the destructor for *"type"*\  will not be called because it is
       inaccessible and the destructor for *"type"*\  was suppressed
 * - ``1644``\ 
   - | ``file_ends_with_unterminated_type_definition``\ :
     | definition at end of file not followed by a semicolon or a declarator
 * - ``1645``\ 
   - | ``bad_type_for_gnu_sync_function``\ :
     | first argument must be a pointer to integer or enum type
 * - ``1646``\ 
   - | ``invalid_gnu_sync_size``\ :
     | synchronized operations are valid only on objects of size 1, 2, 4, or
       8
 * - ``1647``\ 
   - | ``extra_arguments_ignored``\ :
     | extra arguments ignored
 * - ``1648``\ 
   - | ``equals_assumed_in_cmd_line_macro_def``\ :
     | '=' assumed following macro name *"xxxx"*\  in command-line definition
 * - ``1649``\ 
   - | ``white_space_required_after_macro_name``\ :
     | white space is required between the macro name *"xxxx"*\  and its
       replacement text
 * - ``1650``\ 
   - | ``call_result_should_be_used``\ :
     | result of call is not used
 * - ``1651``\ 
   - | ``warn_unused_result_with_void_return``\ :
     | attribute "warn_unused_result" is ignored for void return type
 * - ``1653``\ 
   - | ``dll_interface_ignored_on_qualified_declaration``\ :
     | dllimport/dllexport is ignored on redeclaration using a qualified name
 * - ``1654``\ 
   - | ``leading_character_ignored_in_char_literal``\ :
     | too many characters in character literal -- extra leading characters
       ignored
 * - ``1655``\ 
   - | ``first_inline_after_definition``\ :
     | *entity-kind "entity"*\  cannot be declared inline after its
       definition at line *xxxx*\ 
 * - ``1658``\ 
   - | ``type_with_no_linkage_in_template_arg``\ :
     | a template argument may not reference a type with no name linkage
 * - ``1659``\ 
   - | ``virtual_ignored``\ :
     | "virtual" is ignored here
 * - ``1660``\ 
   - | ``vla_type_in_template_arg``\ :
     | a template argument may not reference a variable-length array type
 * - ``1661``\ 
   - | ``UCN_names_surrogate_code_point``\ :
     | a universal character name cannot designate a surrogate code point
 * - ``1662``\ 
   - | ``include_next_in_primary_source_file``\ :
     | #include_next cannot be used in the primary source file
 * - ``1663``\ 
   - | ``bad_template_member_definition``\ :
     | *"entity"*\  cannot be specified in a template member definition --
       *"entity"*\  assumed instead
 * - ``1664``\ 
   - | ``local_function_attribute_ignored``\ :
     | attribute *"xxxx"*\  is ignored on local function declaration
 * - ``1665``\ 
   - | ``concat_yields_invalid_token``\ :
     | concatenation with *"xxxx"*\  in *entity-kind "entity"*\  does not
       create a valid token
 * - ``1666``\ 
   - | ``ambiguous_injected_template_name``\ :
     | *"entity"*\  is ambiguous (*entity-kind "entity"*\  assumed)
 * - ``1667``\ 
   - | ``function_qualifier_on_static_member``\ :
     | a type qualifier is not allowed on a static member function
 * - ``1668``\ 
   - | ``function_qualifier_on_ctor_or_dtor``\ :
     | a type qualifier is not allowed on a constructor or destructor
 * - ``1669``\ 
   - | ``function_qualifier_on_new_or_delete``\ :
     | a type qualifier is not allowed on operator new or operator delete
 * - ``1670``\ 
   - | ``function_qualifier_on_nonmember``\ :
     | a type qualifier is not allowed on a nonmember function
 * - ``1671``\ 
   - | ``assume_expression_discarded``\ :
     | argument to *xxxx*\  has side-effects but is unevaluated
 * - ``1672``\ 
   - | ``cl_unrecognized_unicode_source_kind``\ :
     | unrecognized Unicode source kind (must be one of UTF-8, UTF-16,
       UTF-16LE, UTF-16BE): *xxxx*\ 
 * - ``1673``\ 
   - | ``bad_unicode_char_in_pp_output``\ :
     | Unicode character with hex value *xxxx*\  not representable in
       preprocessing output
 * - ``1674``\ 
   - | ``ctor_dtor_priority_reserved``\ :
     | requested constructor/destructor priority is reserved for internal use
 * - ``1675``\ 
   - | ``unrecognized_gcc_pragma``\ :
     | unrecognized GCC pragma
 * - ``1676``\ 
   - | ``unrecognized_gcc_visibility_pragma``\ :
     | unrecognized GCC visibility pragma directive
 * - ``1677``\ 
   - | ``unrecognized_visibility``\ :
     | unrecognized visibility kind
 * - ``1678``\ 
   - | ``ELF_visibility_pop_mismatch``\ :
     | visibility pragma was still active
 * - ``1679``\ 
   - | ``ELF_visibility_stack_empty``\ :
     | no matching visibility push
 * - ``1680``\ 
   - | ``typeid_of_incomplete_type``\ :
     | typeid of incomplete type
 * - ``1682``\ 
   - | ``array_size_one_assumed``\ :
     | array *entity-kind "entity"*\  assumed to have one element
 * - ``1683``\ 
   - | ``vector_size_attribute_requires_integral_floating_or_enum_type``\ :
     | vector_size attribute requires an arithmetic or enum type
 * - ``1684``\ 
   - | ``vector_size_too_large``\ :
     | vector size is too large
 * - ``1685``\ 
   - | ``vector_size_must_be_power_of_two``\ :
     | the number of elements in a vector must be a power of two
 * - ``1686``\ 
   - | ``vector_size_must_be_multiple_of_element_size``\ :
     | vector size must be a multiple of the element size
 * - ``1687``\ 
   - | ``mixed_vector_scalar_operation``\ :
     | mixed vector-scalar operation not allowed
 * - ``1688``\ 
   - | ``vectors_must_have_same_size``\ :
     | operation requires two vectors of the same size
 * - ``1689``\ 
   - | ``dependent_vector_size``\ :
     | template-dependent vector size is not allowed
 * - ``1692``\ 
   - | ``vector_size_attribute_on_complex_type``\ :
     | vector_size attribute is not allowed with a complex element type
 * - ``1694``\ 
   - | ``vector_element_type_mismatch``\ :
     | vector operation requires identical element types
 * - ``1695``\ 
   - | ``vector_operation_requires_integer_vector``\ :
     | vector operation does not apply to vector with non-integral type
 * - ``1696``\ 
   - | ``cannot_open_file``\ :
     | cannot open *xxxx*\  file *"xxxx"*\ 
 * - ``1697``\ 
   - | ``cannot_open_file_reason``\ :
     | cannot open *xxxx*\  file *"xxxx"*\ : *xxxx*\ 
 * - ``1703``\ 
   - | ``file_write_error_errno``\ :
     | error while writing *xxxx*\  file: *xxxx*\ 
 * - ``1712``\ 
   - | ``il_output``\ :
     | IL output
 * - ``1713``\ 
   - | ``restrict_qualifier_dropped``\ :
     | conversion drops "\__restrict" qualifier
 * - ``1714``\ 
   - | ``unable_to_get_mapped_memory_reason``\ :
     | unable to obtain mapped memory for *"xxxx"*\ : *xxxx*\ 
 * - ``1715``\ 
   - | ``restrict_qualifier_ignored``\ :
     | restrict qualifier is ignored
 * - ``1717``\ 
   - | ``nonstandard_array_with_flexible_array_element``\ :
     | array of elements containing a flexible array member is nonstandard
 * - ``1718``\ 
   - | ``vector_template_parameter``\ :
     | a template parameter may not have a vector type
 * - ``1719``\ 
   - | ``out_of_order_ctor_init``\ :
     | the initialization of *entity-kind "entity"*\  will be done before
       that of *entity-kind "entity"*\ 
 * - ``1721``\ 
   - | ``inheritance_kind_ignored_on_enum``\ :
     | inheritance kind is ignored on an enum specifier
 * - ``1723``\ 
   - | ``extended_modifier_ignored_on_enum``\ :
     | modifier is ignored on an enum specifier
 * - ``1724``\ 
   - | ``non_unicode_char_in_ident``\ :
     | identifier character cannot be represented in Unicode
 * - ``1725``\ 
   - | ``non_unicode_char_in_header``\ :
     | header name contains characters that cannot be represented in Unicode
 * - ``1726``\ 
   - | ``invalid_locale``\ :
     | *"xxxx"*\  is not a valid locale name
 * - ``1727``\ 
   - | ``nonstd_template_void_param_list``\ :
     | declaring a void parameter list with a template parameter is
       nonstandard
 * - ``1728``\ 
   - | ``cl_lambdas_option_only_in_cplusplus``\ :
     | lambdas option can be used only when compiling C++
 * - ``1729``\ 
   - | ``capture_mode_matches_default``\ :
     | explicit capture matches default
 * - ``1730``\ 
   - | ``not_a_variable``\ :
     | *entity-kind "entity"*\  is not a variable
 * - ``1731``\ 
   - | ``capture_of_static_duration_variable``\ :
     | a variable with static storage duration cannot be captured in a lambda
 * - ``1732``\ 
   - | ``cannot_capture_this_by_reference``\ :
     | "this" cannot be captured by reference
 * - ``1733``\ 
   - | ``this_in_lambda``\ :
     | "this" cannot be used inside the body of this lambda
 * - ``1734``\ 
   - | ``anon_union_ref_in_lambda``\ :
     | a member of an outer-scope anonymous union cannot be referenced
       inside the body of a lambda
 * - ``1735``\ 
   - | ``not_captured_local_var_in_lambda``\ :
     | an enclosing-function local variable cannot be referenced in a lambda
       body unless it is in the capture list
 * - ``1736``\ 
   - | ``bad_local_var_in_lambda``\ :
     | invalid reference to an outer-scope local variable in a lambda body
 * - ``1737``\ 
   - | ``captured_local_var_not_in_innermost_function``\ :
     | a local variable outside the current function scope cannot be captured
 * - ``1738``\ 
   - | ``not_captured_this_in_lambda``\ :
     | the enclosing-function "this" cannot be referenced in a lambda body
       unless it is in the capture list
 * - ``1740``\ 
   - | ``captured_var_type_not_copyable``\ :
     | lambda captured variable of type *"type"*\  cannot be copied to
       closure class field of type *"type"*\ 
 * - ``1741``\ 
   - | ``cl_invalid_template_directory``\ :
     | invalid template directory: *xxxx*\ 
 * - ``1749``\ 
   - | ``enum_value_out_of_underlying_range``\ :
     | enumeration value is outside the range of its underlying type
       (*"type"*\ )
 * - ``1750``\ 
   - | ``not_a_line_splice``\ :
     | "\" followed by white space is not a line splice
 * - ``1751``\ 
   - | ``dynamic_cast_without_rtti``\ :
     | this dynamic_cast cannot be done without runtime type information,
       which is disabled
 * - ``1752``\ 
   - | ``ambiguous_cast_selects_direct_base``\ :
     | conversion to *"type"*\  is ambiguous; direct base selected
 * - ``1753``\ 
   - | ``requested_size_too_large``\ :
     | an internal buffer would be too large
 * - ``1754``\ 
   - | ``exception_handler_used``\ :
     | C++ exception handler used, but exception handling semantics have not
       been specified
 * - ``1755``\ 
   - | ``type_qualifier_ignored_on_constructor``\ :
     | type qualifier ignored on constructor
 * - ``1756``\ 
   - | ``lambda_capture_involves_variable_length_array``\ :
     | a variable captured by a lambda cannot have a type involving a
       variable-length array
 * - ``1757``\ 
   - | ``incompatible_vectors_conversion``\ :
     | conversion between incompatible vector types
 * - ``1758``\ 
   - | ``missing_lambda_body``\ :
     | expected a "{" introducing a lambda body
 * - ``1759``\ 
   - | ``cl_rvalue_references_option_only_in_cplusplus``\ :
     | rvalue references option can be used only when compiling C++
 * - ``1760``\ 
   - | ``type_qualifier_on_lambda``\ :
     | a type qualifier is not allowed on a lambda
 * - ``1761``\ 
   - | ``more_than_one_capture``\ :
     | a name cannot appear more than once in a capture-list
 * - ``1762``\ 
   - | ``explicit_template_args_ignored``\ :
     | explicit template arguments ignored
 * - ``1763``\ 
   - | ``bad_constant_lambda``\ :
     | a lambda is not allowed in a constant expression
 * - ``1764``\ 
   - | ``class_type_required``\ :
     | *"type"*\  is not a class type
 * - ``1765``\ 
   - | ``delete_of_array_type``\ :
     | "delete" applied to a pointer-to-array type treated as delete[]
 * - ``1766``\ 
   - | ``delete_of_array_type_nonstandard``\ :
     | "delete" applied to a pointer-to-array type is nonstandard; treated
       as delete[]
 * - ``1767``\ 
   - | ``function_does_not_match_arguments``\ :
     | *entity-kind "entity"*\  cannot be called with the given argument list
 * - ``1768``\ 
   - | ``rvalue_reference_bound_to_lvalue``\ :
     | an rvalue reference cannot be bound to an lvalue
 * - ``1769``\ 
   - | ``rvalue_ref_template_parameter``\ :
     | a nontype template parameter cannot have rvalue reference type
 * - ``1770``\ 
   - | ``type_qualifiers_ignored_on_reference``\ :
     | type qualifiers are ignored (underlying type is a reference)
 * - ``1771``\ 
   - | ``decl_with_local_type_but_not_defined``\ :
     | *entity-kind "entity"*\ , declared using a local type, must be
       defined in this translation unit
 * - ``1772``\ 
   - | ``decl_with_no_linkage_type_but_not_defined``\ :
     | *entity-kind "entity"*\ , declared using a type with no linkage, must
       be defined in this translation unit
 * - ``1773``\ 
   - | ``bad_rvalue_ref_dynamic_cast_operand``\ :
     | the operand of an rvalue reference dynamic_cast must have a complete
       class type
 * - ``1774``\ 
   - | ``invalid_function_to_be_defaulted``\ :
     | "= default" can only appear on default constructors, copy/move
       constructors, copy/move assignment operators, and destructors
 * - ``1775``\ 
   - | ``deleted_function_definition_must_be_first_declaration``\ :
     | "= delete" can only appear on the first declaration of a function
 * - ``1776``\ 
   - | ``deleted_function``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) cannot be
       referenced -- it is a deleted function
 * - ``1777``\ 
   - | ``bad_unevaluated_lambda``\ :
     | a lambda is not allowed in an unevaluated expression
 * - ``1778``\ 
   - | ``bad_function_for_gnu_va_arg_pack``\ :
     | __builtin_va_arg_pack/\__builtin_va_arg_pack_len can appear only in
       an inline function with an ellipsis parameter
 * - ``1779``\ 
   - | ``function_defaulted_in_friend_decl``\ :
     | "= default" cannot be specified on a friend declaration
 * - ``1780``\ 
   - | ``exp_cpp_keyword``\ :
     | expected a C++ keyword
 * - ``1782``\ 
   - | ``nonconstant_offsetof``\ :
     | offset is not constant
 * - ``1783``\ 
   - | ``unrecognized_microsoft_comment_pragma_type``\ :
     | unrecognized #pragma comment type *"xxxx"*\ 
 * - ``1784``\ 
   - | ``cl_auto_type_option_only_in_cplusplus``\ :
     | option to control whether "auto" is a type specifier can be used only
       when compiling C++
 * - ``1785``\ 
   - | ``cl_auto_storage_option_only_in_cplusplus``\ :
     | option to control whether "auto" is a storage class can be used only
       when compiling C++
 * - ``1786``\ 
   - | ``cl_auto_cannot_be_disabled``\ :
     | the type specifier and storage class specifier meanings of "auto"
       cannot both be disabled
 * - ``1787``\ 
   - | ``bad_pragma_comment_string``\ :
     | invalid string in #pragma comment
 * - ``1788``\ 
   - | ``deleted_function_overrides_nondeleted_function``\ :
     | deleted function overrides nondeleted *entity-kind "entity"*\ 
 * - ``1789``\ 
   - | ``nondeleted_function_overrides_deleted_function``\ :
     | nondeleted function overrides deleted *entity-kind "entity"*\ 
 * - ``1790``\ 
   - | ``deleted_default_constructor``\ :
     | the default constructor of *"type"*\  cannot be referenced -- it is a
       deleted function
 * - ``1791``\ 
   - | ``rvalue_reference_catch_type``\ :
     | an rvalue reference is not allowed as a catch type
 * - ``1792``\ 
   - | ``default_args_incompatible``\ :
     | default arguments of *entity-kind "entity"*\  is incompatible with a
       declaration in another translation unit
 * - ``1793``\ 
   - | ``default_arg_differs_in_other_trans_unit``\ :
     | default arguments of *entity-kind "entity"*\  were different during
       compilation of *"xxxx"*\ 
 * - ``1795``\ 
   - | ``initializers_incompatible``\ :
     | initializer for *entity-kind "entity"*\  is different in another
       translation unit
 * - ``1796``\ 
   - | ``initializer_differs_in_other_trans_unit``\ :
     | initializer for *entity-kind "entity"*\  was different during
       compilation of *"xxxx"*\ 
 * - ``1797``\ 
   - | ``designator_for_template_dependent_type``\ :
     | a designator into a template-dependent type is not allowed
 * - ``1798``\ 
   - | ``invalid_pragma_conform_kind``\ :
     | unrecognized conformance kind
 * - ``1799``\ 
   - | ``exp_on_or_off``\ :
     | expected "on" or "off"
 * - ``1800``\ 
   - | ``forScope_stack_empty``\ :
     | #pragma conform(forScope) stack is empty
 * - ``1801``\ 
   - | ``no_matching_forScope_stack_entry``\ :
     | no previous #pragma conform(forScope) entry matches *"xxxx"*\ 
 * - ``1802``\ 
   - | ``show_pragma_conform_forScope_is_nonstandard``\ :
     | forScope behavior is nonstandard
 * - ``1803``\ 
   - | ``show_pragma_conform_forScope_is_standard``\ :
     | forScope behavior is standard
 * - ``1804``\ 
   - | ``deleted_main``\ :
     | function "main" cannot be deleted
 * - ``1805``\ 
   - | ``useless_type_qualifiers_in_type_name``\ :
     | type qualifiers are meaningless here
 * - ``1806``\ 
   - | ``invalid_assignment_operator_to_be_defaulted``\ :
     | invalid type for defaulted assignment operator
 * - ``1807``\ 
   - | ``function_template_cannot_be_defaulted``\ :
     | function templates cannot be defaulted
 * - ``1808``\ 
   - | ``invalid_constructor_to_be_defaulted``\ :
     | invalid type for defaulted constructor
 * - ``1809``\ 
   - | ``call_requires_one_argument``\ :
     | function call requires one argument
 * - ``1810``\ 
   - | ``call_requires_floating_point_argument``\ :
     | function call requires a real floating-point argument
 * - ``1811``\ 
   - | ``copy_ctor_with_default_arg_cannot_be_defaulted``\ :
     | a copy constructor with a default argument cannot be defaulted
 * - ``1812``\ 
   - | ``predeclared_function_cannot_be_deleted``\ :
     | a predeclared function cannot be deleted
 * - ``1813``\ 
   - | ``empty_then_statement``\ :
     | empty dependent statement in if-statement
 * - ``1814``\ 
   - | ``empty_else_statement``\ :
     | empty dependent statement in "else" clause of if-statement
 * - ``1815``\ 
   - | ``deleted_elided_cctor``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ), required for
       copy that was eliminated, cannot be referenced -- it is a deleted
       function
 * - ``1816``\ 
   - | ``main_first_param_not_int``\ :
     | nonstandard first parameter *"type"*\  of "main", expected "int"
 * - ``1817``\ 
   - | ``main_wrong_num_params``\ :
     | nonstandard number of parameters for "main", expected zero or two
       parameters
 * - ``1818``\ 
   - | ``main_second_param_wrong_type``\ :
     | nonstandard second parameter *"type"*\  of "main", expected "char
       \*[]" or "char \*\*"
 * - ``1819``\ 
   - | ``incl_dir_both_sys_and_nonsys``\ :
     | *"xxxx"*\  was specified as both a system and non-system include
       directory -- the non-system entry will be ignored
 * - ``1820``\ 
   - | ``cl_rvalue_ctor_is_copy_ctor_option_only_in_cplusplus``\ :
     | option to control move constructors and move assignment operators can
       be used only when compiling C++
 * - ``1823``\ 
   - | ``trailing_return_type_requires_auto``\ :
     | a trailing return type requires the "auto" type specifier
 * - ``1824``\ 
   - | ``trailing_return_type_in_nested_declarator``\ :
     | a trailing return type cannot appear in a nested declarator
 * - ``1825``\ 
   - | ``trailing_return_type_function_without_simple_auto``\ :
     | a function declarator with a trailing return type must be preceded by
       a simple "auto" type specifier
 * - ``1826``\ 
   - | ``missing_trailing_return_type``\ :
     | "auto" function requires a trailing return type
 * - ``1827``\ 
   - | ``pure_virtual_member_template``\ :
     | a member template cannot have a pure specifier
 * - ``1828``\ 
   - | ``excess_characters_in_literal_ignored``\ :
     | string literal too long -- excess characters ignored
 * - ``1830``\ 
   - | ``nullptr_conv_to_bool``\ :
     | std::nullptr_t converted to bool
 * - ``1833``\ 
   - | ``invalid_empty_attribute_arg_list``\ :
     | attribute *"xxxx"*\  does not allow an empty argument list
 * - ``1834``\ 
   - | ``attr_twice_in_group``\ :
     | attribute appears more than once
 * - ``1835``\ 
   - | ``wrong_entity_for_attribute``\ :
     | attribute *"xxxx"*\  does not apply here
 * - ``1836``\ 
   - | ``attr_disallows_bit_field``\ :
     | attribute *"xxxx"*\  does not apply to bit fields
 * - ``1837``\ 
   - | ``attr_requires_bit_field``\ :
     | attribute *"xxxx"*\  requires a bit field
 * - ``1838``\ 
   - | ``attr_disallows_member_function``\ :
     | attribute *"xxxx"*\  does not apply to member functions
 * - ``1839``\ 
   - | ``attr_requires_member_function``\ :
     | attribute *"xxxx"*\  requires a member function
 * - ``1840``\ 
   - | ``attr_disallows_virtual_function``\ :
     | attribute *"xxxx"*\  does not apply to virtual functions
 * - ``1841``\ 
   - | ``attr_requires_virtual_function``\ :
     | attribute *"xxxx"*\  requires a virtual function
 * - ``1842``\ 
   - | ``attr_disallows_pure_virtual_function``\ :
     | attribute *"xxxx"*\  does not apply to pure virtual functions
 * - ``1843``\ 
   - | ``attr_requires_pure_virtual_function``\ :
     | attribute *"xxxx"*\  requires a pure virtual function
 * - ``1844``\ 
   - | ``attr_disallows_register_storage``\ :
     | attribute *"xxxx"*\  does not apply to register variables
 * - ``1845``\ 
   - | ``attr_requires_register_storage``\ :
     | attribute *"xxxx"*\  requires a register variable
 * - ``1846``\ 
   - | ``attr_must_also_appear_in_first_declaration``\ :
     | attribute *"xxxx"*\  did not appear on original declaration
 * - ``1847``\ 
   - | ``invalid_attribute_location``\ :
     | attributes are not allowed here
 * - ``1848``\ 
   - | ``attr_must_appear_in_class_definition``\ :
     | attribute *"xxxx"*\  must appear in a class definition
 * - ``1849``\ 
   - | ``pure_final_virtual``\ :
     | "final" applied to a pure virtual function
 * - ``1850``\ 
   - | ``override_of_final_function``\ :
     | cannot override "final" *entity-kind "entity"*\  (declared at line
       *xxxx*\ )
 * - ``1851``\ 
   - | ``undefined_static_function_treated_as_extern``\ :
     | static *entity-kind "entity"*\  treated as extern because it was
       referenced but not defined
 * - ``1852``\ 
   - | ``cl_gnu_c89_inlining_option_only_in_C``\ :
     | option to enable GNU-C89-style inlining can be used only when
       compiling C
 * - ``1853``\ 
   - | ``first_decl_not_gnu_inline``\ :
     | function was previously declared without the "gnu_inline" attribute
 * - ``1854``\ 
   - | ``gnu_inline_requires_inline``\ :
     | the "gnu_inline" attribute is ignored on non-inline functions
 * - ``1855``\ 
   - | ``carries_dependency_not_on_first_decl``\ :
     | *entity-kind "entity"*\  previously declared without the
       carries_dependency attribute
 * - ``1856``\ 
   - | ``bad_array_member_initialization``\ :
     | invalid initializer for array *entity-kind "entity"*\ 
 * - ``1857``\ 
   - | ``cl_must_specify_cpp11_mode``\ :
     | must specify C++11 or C++14 mode when building runtime library
 * - ``1858``\ 
   - | ``attr_disallows_function_type``\ :
     | attribute *"xxxx"*\  does not apply to function types
 * - ``1859``\ 
   - | ``attr_requires_function_type``\ :
     | attribute *"xxxx"*\  requires a function type
 * - ``1860``\ 
   - | ``attribute_ignored_on_nonstatic_member_function``\ :
     | attribute *"xxxx"*\  does not apply to nonstatic member functions
 * - ``1861``\ 
   - | ``attr_disallows_automatic_storage``\ :
     | attribute *"xxxx"*\  does not apply to automatic variables
 * - ``1862``\ 
   - | ``attr_requires_automatic_storage``\ :
     | attribute *"xxxx"*\  requires an automatic variable
 * - ``1863``\ 
   - | ``attr_disallows_external_linkage``\ :
     | attribute *"xxxx"*\  does not apply to a variable or function with
       external linkage
 * - ``1864``\ 
   - | ``attr_requires_local_variable``\ :
     | attribute *"xxxx"*\  requires a local variable
 * - ``1865``\ 
   - | ``attributes_ignored``\ :
     | attributes ignored here
 * - ``1866``\ 
   - | ``unattached_attribute``\ :
     | attribute does not apply to any entity
 * - ``1867``\ 
   - | ``bad_attribute_template_substitution``\ :
     | bad attribute argument substitution
 * - ``1868``\ 
   - | ``bad_tls_model_attr_arg``\ :
     | the argument of the "tls_model" attribute must be "global-dynamic",
       "local-dynamic", "initial-exec", or "local-exec"
 * - ``1869``\ 
   - | ``inconsistent_tls_model_attr_arg``\ :
     | the declaration at line *xxxx*\  specified a different "tls_model"
       argument
 * - ``1870``\ 
   - | ``attr_disallows_inline``\ :
     | attribute *"xxxx"*\  does not apply to inline functions
 * - ``1871``\ 
   - | ``attr_requires_inline``\ :
     | attribute *"xxxx"*\  requires a inline function
 * - ``1872``\ 
   - | ``include_kind_mismatch``\ :
     | both file names in an include_alias pragma must use the same
       delimiter characters
 * - ``1873``\ 
   - | ``signed_unsigned_comparison``\ :
     | comparison between signed and unsigned operands
 * - ``1874``\ 
   - | ``attribute_ignored_on_unnamed_type``\ :
     | attribute *"xxxx"*\  ignored on unnamed type
 * - ``1875``\ 
   - | ``attribute_ignored_on_nondefinition``\ :
     | attribute *"xxxx"*\  ignored because no definition follows
 * - ``1876``\ 
   - | ``incompatible_thread_locality``\ :
     | thread locality is incompatible with a previous declaration of
       *entity-kind "entity"*\  (declared at line *xxxx*\ )
 * - ``1877``\ 
   - | ``no_implicit_capture_on_enclosing_lambda``\ :
     | this enclosing-function local variable cannot be referenced in this
       lambda body because an enclosing lambda does not allow implicit
       captures
 * - ``1878``\ 
   - | ``unbalanced_attribute_argument``\ :
     | this attribute argument contains unmatched parentheses, brackets, or
       braces
 * - ``1879``\ 
   - | ``invalid_builtin_fpclassify_args``\ :
     | a call to \__builtin_fpclassify requires five integral arguments
       followed by one floating-point argument
 * - ``1880``\ 
   - | ``bad_final_builtin_fpclassify_arg``\ :
     | the last argument in a call to \__builtin_fpclassify must have a real
       floating-point type
 * - ``1881``\ 
   - | ``invalid_alignment_reducing_attr``\ :
     | alignment cannot be set to less than the default alignment
 * - ``1882``\ 
   - | ``attribute_on_explicit_instantiation``\ :
     | attributes are not allowed on explicit instantiations
 * - ``1883``\ 
   - | ``attr_disallows_definition``\ :
     | attribute *"xxxx"*\  does not apply to a definition
 * - ``1884``\ 
   - | ``attr_requires_definition``\ :
     | attribute *"xxxx"*\  requires a definition
 * - ``1885``\ 
   - | ``friend_attribute_requires_definition``\ :
     | standard attributes cannot appear on friend declarations that are not
       definitions
 * - ``1886``\ 
   - | ``inconsistent_alignment``\ :
     | specified alignment (*xxxx*\ ) is different from alignment (*xxxx*\ )
       specified on a previous declaration
 * - ``1887``\ 
   - | ``variable_align_attr_not_on_definition``\ :
     | alignment attribute must also appear on definition at line *xxxx*\ 
 * - ``1888``\ 
   - | ``alias_used_in_type``\ :
     | *entity-kind "entity"*\  may not be used in the type-id of the
       alias-declaration
 * - ``1890``\ 
   - | ``transparent_union_cannot_have_floating_first_field``\ :
     | *"type"*\  cannot be transparent because its first field has a
       floating-point type
 * - ``1891``\ 
   - | ``transparent_union_cannot_have_bit_field_first``\ :
     | *"type"*\  cannot be transparent because its first field is a bit
       field
 * - ``1892``\ 
   - | ``missing_override_attr_in_base_check_class``\ :
     | virtual function of a "base_check" class overrides a base class
       member but lacks the "override" attribute
 * - ``1893``\ 
   - | ``hiding_attr_on_nonhiding_member``\ :
     | "hiding" attribute specified on a declaration that does not hide a
       base class declaration
 * - ``1894``\ 
   - | ``hiding_attr_on_unhidden_member``\ :
     | "hiding" attribute specified on a declaration referred to by the
       using-declaration at line *xxxx*\ 
 * - ``1895``\ 
   - | ``hiding_attr_required``\ :
     | attribute "hiding" is required on a declaration (in a "base_check"
       class) that hides *entity-kind "entity"*\  (declared at line *xxxx*\ )
 * - ``1896``\ 
   - | ``undefined_decl_using_local_type``\ :
     | *entity-kind "entity"*\  is not defined in this translation unit but
       depends on a local type
 * - ``1897``\ 
   - | ``undefined_decl_using_no_linkage_type``\ :
     | *entity-kind "entity"*\  is not defined in this translation unit but
       depends on a type with no linkage
 * - ``1898``\ 
   - | ``missing_attribute_in_other_translation_unit``\ :
     | attribute *"xxxx"*\  is missing in another translation unit
 * - ``1899``\ 
   - | ``conflicting_attribute_in_other_translation_unit``\ :
     | attribute *"xxxx"*\  conflicts with another translation unit
 * - ``1900``\ 
   - | ``cl_nonstd_gnu_keywords_requires_gnu_mode``\ :
     | the "nonstd_gnu_keywords" option is only valid in GNU C and GNU C++
       modes
 * - ``1901``\ 
   - | ``const_var_in_C_const_expr``\ :
     | use of a const variable in a constant expression is nonstandard in C
 * - ``1902``\ 
   - | ``cannot_init_auto_flexible_array_member``\ :
     | an initializer cannot be specified for a flexible array member with
       automatic storage duration
 * - ``1904``\ 
   - | ``final_base_class``\ :
     | a "final" class type cannot be used as a base class
 * - ``1905``\ 
   - | ``export_removed``\ :
     | exported templates are no longer in the standard C++ language
 * - ``1906``\ 
   - | ``template_dependent_designator``\ :
     | a template-dependent designator is not allowed
 * - ``1907``\ 
   - | ``offsetof_ref_field``\ :
     | second operand of offsetof may not be a field with reference type
 * - ``1908``\ 
   - | ``cl_long_lifetime_temps_incompat_with_newer_features``\ :
     | long lifetime temporaries are incompatible with other requested newer
       language features
 * - ``1909``\ 
   - | ``wide_deprecation_string``\ :
     | wide character string literal will not be quoted in diagnostics
 * - ``1910``\ 
   - | ``missing_attribute_arguments``\ :
     | missing arguments for attribute *"xxxx"*\ 
 * - ``1911``\ 
   - | ``sfinae_requires_newer_abi_version``\ :
     | options "c++11" and "c++11_sfinae" require a different compiler
       configuration
 * - ``1912``\ 
   - | ``template_param_pack_not_at_end``\ :
     | template parameter pack not at end of parameter list
 * - ``1913``\ 
   - | ``parameter_pack_decl_not_allowed``\ :
     | a parameter pack declaration is not allowed here
 * - ``1914``\ 
   - | ``param_pack_cannot_have_default``\ :
     | a parameter pack cannot have a default
 * - ``1915``\ 
   - | ``cl_cppcli_only_in_microsoft_cplusplus``\ :
     | C++/CLI can be enabled only in Microsoft C++ mode
 * - ``1916``\ 
   - | ``reserved_enumerator_name``\ :
     | "value\__" cannot be used as the name of an enumerator constant (it
       is a reserved name in this context)
 * - ``1917``\ 
   - | ``cppcli_enumerator_requires_explicit_value``\ :
     | an explicit enumerator value is required in an enum type with boolean
       underlying type
 * - ``1919``\ 
   - | ``pack_not_expanded``\ :
     | parameter pack *"xxxx"*\  was referenced but not expanded
 * - ``1920``\ 
   - | ``expansion_contains_no_packs``\ :
     | pack expansion does not make use of any argument packs
 * - ``1921``\ 
   - | ``pack_length_mismatch``\ :
     | pack *"xxxx"*\  does not have the same number of elements as
       *"xxxx"*\ 
 * - ``1923``\ 
   - | ``vector_size_attribute_on_enum_type``\ :
     | vector_size attribute is not allowed with an enum type
 * - ``1924``\ 
   - | ``virtual_static_property``\ :
     | a property cannot be both static and virtual
 * - ``1925``\ 
   - | ``trivial_indexed_property``\ :
     | an indexed property cannot be trivial
 * - ``1926``\ 
   - | ``invalid_property_accessor_decl``\ :
     | this declaration cannot appear in a property definition
 * - ``1927``\ 
   - | ``qualified_cli_accessor``\ :
     | a qualified function type cannot be used to declare an accessor
       function
 * - ``1928``\ 
   - | ``ellipsis_cli_accessor``\ :
     | an accessor function cannot have an ellipsis parameter
 * - ``1929``\ 
   - | ``property_get_already_declared``\ :
     | a "get" accessor was already declared for this property at line
       *xxxx*\ 
 * - ``1930``\ 
   - | ``property_set_already_declared``\ :
     | a "set" accessor was already declared for this property at line
       *xxxx*\ 
 * - ``1931``\ 
   - | ``property_get_cannot_have_parameter``\ :
     | a "get" accessor cannot have a parameter
 * - ``1932``\ 
   - | ``bad_property_get_return``\ :
     | return type of "get" accessor does not match property type
 * - ``1933``\ 
   - | ``bad_property_set_return``\ :
     | return type of "set" accessor must be void
 * - ``1934``\ 
   - | ``empty_property_indices``\ :
     | a property cannot declare an empty list of indices
 * - ``1935``\ 
   - | ``void_property_index_type``\ :
     | a property index cannot have type void
 * - ``1936``\ 
   - | ``property_set_index_type_mismatch``\ :
     | index type does not match the corresponding parameter in the "set"
       accessor
 * - ``1937``\ 
   - | ``property_get_index_type_mismatch``\ :
     | index type does not match the corresponding parameter in the "get"
       accessor
 * - ``1938``\ 
   - | ``property_set_index_type_missing``\ :
     | index type is missing in the "set" accessor
 * - ``1939``\ 
   - | ``property_get_index_type_missing``\ :
     | index type is missing in the "get" accessor
 * - ``1940``\ 
   - | ``property_set_missing_value_parameter``\ :
     | "set" accessor is missing its value parameter
 * - ``1941``\ 
   - | ``extra_property_accessor_parameters``\ :
     | accessor function has too many parameters
 * - ``1942``\ 
   - | ``property_set_value_parameter_mismatch``\ :
     | the last parameter of the "set" accessor does not match the property
       type
 * - ``1943``\ 
   - | ``cppcli_not_enabled``\ :
     | *"xxxx"*\  requires C++/CLI mode
 * - ``1944``\ 
   - | ``win32_api_error``\ :
     | error in Win32 API "*xxxx*\ ": *xxxx*\ 
 * - ``1945``\ 
   - | ``using_not_at_file_scope``\ :
     | #using may only be used at global scope
 * - ``1947``\ 
   - | ``member_name_reserved_by_property``\ :
     | member name *"xxxx"*\  is reserved by *entity-kind "entity"*\ 
       (declared at line *xxxx*\ )
 * - ``1948``\ 
   - | ``exp_lbracket``\ :
     | expected a "["
 * - ``1949``\ 
   - | ``cl_microsoft_version_insufficient_for_cppcli``\ :
     | C++/CLI mode requires microsoft_version >= 1600
 * - ``1950``\ 
   - | ``static_default_indexed_property``\ :
     | a default-indexed property cannot be static
 * - ``1951``\ 
   - | ``virtual_static_property_accessor``\ :
     | a property accessor cannot be both static and virtual
 * - ``1952``\ 
   - | ``visibility_specifier_on_nested_type``\ :
     | a top-level visibility specifier cannot appear on a nested type
       declaration
 * - ``1953``\ 
   - | ``visibility_specifier_requires_definition``\ :
     | a top-level visibility specifier requires a type definition
 * - ``1954``\ 
   - | ``trivial_reference_property``\ :
     | a trivial property cannot have a reference type
 * - ``1955``\ 
   - | ``trivial_const_or_volatile_property``\ :
     | a trivial property cannot have a const or volatile type
 * - ``1956``\ 
   - | ``incompatible_enum_kinds``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) was previously
       declared as a different kind of enum type
 * - ``1957``\ 
   - | ``lambda_captures_managed_class_type``\ :
     | a variable captured by a lambda cannot have a managed class type
 * - ``1958``\ 
   - | ``covariant_override_in_managed_class``\ :
     | virtual function overriding with a covariant return type is not
       allowed in a managed class
 * - ``1959``\ 
   - | ``array_of_handle``\ :
     | array of handles is not allowed
 * - ``1960``\ 
   - | ``handle_to_array``\ :
     | handle to array is not allowed
 * - ``1961``\ 
   - | ``handle_to_function``\ :
     | handle to function is not allowed
 * - ``1962``\ 
   - | ``handle_to_void``\ :
     | handle to void is not allowed
 * - ``1963``\ 
   - | ``handle_to_address_type``\ :
     | handle to handle, pointer, or reference is not allowed
 * - ``1964``\ 
   - | ``tracking_reference_to_function``\ :
     | tracking reference to function is not allowed
 * - ``1966``\ 
   - | ``field_cannot_be_tracking_reference``\ :
     | a field cannot be a tracking reference
 * - ``1967``\ 
   - | ``invalid_ref_tracking_ref_combination``\ :
     | a tracking reference cannot be combined with an ordinary reference in
       this way
 * - ``1968``\ 
   - | ``static_storage_variable_with_ref_class_type``\ :
     | a variable with static storage duration cannot have a ref class type
 * - ``1969``\ 
   - | ``unnamed_cli_managed_class_type``\ :
     | a managed class cannot be unnamed
 * - ``1970``\ 
   - | ``local_cli_managed_class_type``\ :
     | a managed class cannot be local
 * - ``1971``\ 
   - | ``conflicting_cli_class_type_kinds``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) was previously
       declared as a different kind of class
 * - ``1972``\ 
   - | ``conflicting_cli_class_template_kinds``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) was previously
       declared as a different kind of class template
 * - ``1973``\ 
   - | ``literal_requires_managed_class``\ :
     | literal data members can only be members of managed classes
 * - ``1974``\ 
   - | ``literal_without_initializer``\ :
     | a literal data member must be initialized
 * - ``1975``\ 
   - | ``invalid_literal_type``\ :
     | a literal data member of type *"type"*\  is not allowed
 * - ``1976``\ 
   - | ``literal_const_has_no_effect``\ :
     | const has no effect on a literal data member
 * - ``1977``\ 
   - | ``initonly_requires_managed_class``\ :
     | initonly data members can only be members of managed classes
 * - ``1978``\ 
   - | ``initonly_const_has_no_effect``\ :
     | const has no effect on an initonly data member
 * - ``1979``\ 
   - | ``cli_get_accessor_missing``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) has no "get"
       accessor
 * - ``1980``\ 
   - | ``cli_set_accessor_missing``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) has no "set"
       accessor
 * - ``1981``\ 
   - | ``static_constructor_with_params``\ :
     | a static constructor cannot have parameters
 * - ``1982``\ 
   - | ``static_constructor_member_template``\ :
     | a static constructor cannot be a member template
 * - ``1983``\ 
   - | ``compound_lvalue_as_asm_operand``\ :
     | a compound lvalue is not allowed as an asm output operand
 * - ``1984``\ 
   - | ``property_requires_managed_class``\ :
     | properties can only be members of managed classes
 * - ``1985``\ 
   - | ``qualifier_not_allowed_on_managed_member_function``\ :
     | a type qualifier is not allowed on a member function of a managed
       class
 * - ``1986``\ 
   - | ``pointer_to_ref_or_interface_class``\ :
     | an ordinary pointer to a C++/CLI ref class or interface class is not
       allowed
 * - ``1987``\ 
   - | ``reference_to_ref_or_interface_class``\ :
     | an ordinary reference to a C++/CLI ref class or interface class is
       not allowed
 * - ``1988``\ 
   - | ``override_name_must_be_a_base_class_member_function``\ :
     | override specifier does not name a base class member function
 * - ``1989``\ 
   - | ``override_name_nonvirtual``\ :
     | override specifier designates a nonvirtual member *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``1990``\ 
   - | ``multiple_overrides``\ :
     | member function overrides *entity-kind "entity"*\  (declared at line
       *xxxx*\ ) which is already overridden by *entity-kind "entity"*\ 
       (declared at line *xxxx*\ )
 * - ``1991``\ 
   - | ``multiple_visibility_specifiers``\ :
     | at most one visibility specifier is allowed
 * - ``1992``\ 
   - | ``invalid_delegate_type``\ :
     | type *"type"*\  used for delegate definition is not a function type
 * - ``1993``\ 
   - | ``delegate_requires_managed_class``\ :
     | delegate member types can only be members of managed classes
 * - ``1994``\ 
   - | ``tracking_reference_to_delegate``\ :
     | a tracking reference to a delegate type is not allowed
 * - ``1995``\ 
   - | ``bad_use_of_delegate_type``\ :
     | a delegate type is not allowed here
 * - ``1996``\ 
   - | ``empty_pack_expansion``\ :
     | this pack expansion produced an empty list of expressions, and an
       expression is needed here
 * - ``1997``\ 
   - | ``virtual_static_event``\ :
     | an event cannot be both static and virtual
 * - ``1998``\ 
   - | ``event_requires_managed_class``\ :
     | events can only be members of managed classes
 * - ``1999``\ 
   - | ``invalid_event_accessor_decl``\ :
     | this declaration cannot appear in an event definition
 * - ``2000``\ 
   - | ``invalid_event_type``\ :
     | event type must be a handle-to-delegate type
 * - ``2001``\ 
   - | ``event_add_already_declared``\ :
     | an "add" accessor was already declared for this event at line *xxxx*\ 
 * - ``2002``\ 
   - | ``event_remove_already_declared``\ :
     | a "remove" accessor was already declared for this event at line
       *xxxx*\ 
 * - ``2003``\ 
   - | ``event_raise_already_declared``\ :
     | a "raise" accessor was already declared for this event at line
       *xxxx*\ 
 * - ``2004``\ 
   - | ``virtual_static_event_accessor``\ :
     | an event accessor cannot be both static and virtual
 * - ``2005``\ 
   - | ``bad_event_add_or_remove_return``\ :
     | return type of "add" and "remove" accessors must be void
 * - ``2006``\ 
   - | ``event_accessor_missing_value_parameter``\ :
     | event accessor is missing its value parameter
 * - ``2007``\ 
   - | ``extra_event_accessor_parameters``\ :
     | accessor function has too many parameters
 * - ``2008``\ 
   - | ``event_accessor_value_parameter_mismatch``\ :
     | the type *"type"*\  of the parameter of the event accessor does not
       match the event type (*"type"*\ )
 * - ``2009``\ 
   - | ``event_raise_type_mismatch``\ :
     | the type of the "raise" accessor does not match the event's delegate
       invocation type
 * - ``2010``\ 
   - | ``missing_add_or_remove_accessor``\ :
     | an event definition must include both "add" and "remove" accessors
 * - ``2011``\ 
   - | ``static_conversion_function_must_have_one_parameter``\ :
     | a static conversion function must accept exactly one argument
 * - ``2012``\ 
   - | ``bad_parameter_type_for_static_member_operator``\ :
     | static operator must have a parameter type T, T&, T%, or T^ with T =
       *"type"*\ 
 * - ``2013``\ 
   - | ``sizeof_operand_not_parameter_pack``\ :
     | the operand of sizeof... must be a parameter pack name
 * - ``2014``\ 
   - | ``sizeof_pack_in_non_variadic_context``\ :
     | the sizeof... operator can be used only in a variadic template
 * - ``2015``\ 
   - | ``event_name_not_allowed``\ :
     | event name cannot appear here
 * - ``2016``\ 
   - | ``handle_to_standard_class_type``\ :
     | a handle to a non-managed class is not allowed
 * - ``2017``\ 
   - | ``handle_to_unscoped_enum_type``\ :
     | a handle to an unscoped enum type is not allowed
 * - ``2018``\ 
   - | ``property_attribute_in_managed_class``\ :
     | "property" attribute not allowed in managed class
 * - ``2019``\ 
   - | ``pure_virtual_definition``\ :
     | a pure specifier ("= 0") followed by a definition is nonstandard
 * - ``2020``\ 
   - | ``managed_nullptr_not_allowed``\ :
     | the managed nullptr type cannot be used here
 * - ``2021``\ 
   - | ``addr_of_ref_class``\ :
     | the "&" operator cannot be used to take the address of an object with
       a ref class type
 * - ``2022``\ 
   - | ``array_of_managed_class``\ :
     | array of managed class is not allowed
 * - ``2023``\ 
   - | ``static_storage_variable_with_handle_or_tracking_ref_type``\ :
     | a variable with static storage duration cannot have a handle or
       tracking reference type
 * - ``2024``\ 
   - | ``lambda_captures_handle_or_tracking_ref``\ :
     | a variable captured by a lambda cannot be a handle or tracking
       reference
 * - ``2025``\ 
   - | ``invalid_param_array_type``\ :
     | a C++/CLI parameter array requires a handle to a one-dimensional
       cli::array type
 * - ``2026``\ 
   - | ``cannot_import_metadata``\ :
     | could not import metadata from file *"xxxx"*\ 
 * - ``2027``\ 
   - | ``namespace_cli_cannot_be_extended``\ :
     | the cli namespace cannot be extended
 * - ``2028``\ 
   - | ``cli_array_invalid_element_type``\ :
     | the element type of a cli::array must be a handle or value type
 * - ``2029``\ 
   - | ``cli_array_invalid_number_of_dimensions``\ :
     | invalid number of dimensions for cli::array type
 * - ``2030``\ 
   - | ``invalid_type_pointed_to_for_interior_ptr_or_pin_ptr``\ :
     | a cli::interior_ptr/cli::pin_ptr must point to a standard class, a
       value class, an integer, a handle, or a standard pointer
 * - ``2031``\ 
   - | ``type_cannot_be_class_member``\ :
     | *"type"*\  cannot be a class member
 * - ``2032``\ 
   - | ``pin_ptr_param_not_allowed``\ :
     | a parameter of type cli::pin_ptr is not allowed
 * - ``2033``\ 
   - | ``bad_finalizer_decl``\ :
     | invalid finalizer declaration
 * - ``2034``\ 
   - | ``too_many_params_for_finalizer``\ :
     | a finalizer may not have parameters
 * - ``2035``\ 
   - | ``function_qualifier_on_finalizer``\ :
     | a type qualifier is not allowed on a finalizer
 * - ``2036``\ 
   - | ``return_type_on_finalizer``\ :
     | a return type may not be specified on a finalizer
 * - ``2037``\ 
   - | ``no_finalizer_using_declaration``\ :
     | a using-declaration may not name a finalizer
 * - ``2038``\ 
   - | ``finalizer_name_must_be_qualified``\ :
     | a finalizer name must be qualified
 * - ``2039``\ 
   - | ``finalizer_qualifier_type_mismatch``\ :
     | qualifier of finalizer name *"type"*\  does not match type *"type"*\ 
 * - ``2040``\ 
   - | ``var_used_as_finalizer``\ :
     | *entity-kind "entity"*\  cannot be used to name a finalizer (a type
       name is required)
 * - ``2041``\ 
   - | ``invalid_finalizer_name``\ :
     | invalid finalizer name for type *"type"*\ 
 * - ``2042``\ 
   - | ``ambiguous_finalizer``\ :
     | finalizer reference is ambiguous -- both *entity-kind "entity"*\  and
       *entity-kind "entity"*\  could be used
 * - ``2043``\ 
   - | ``finalizer_requires_reference_type``\ :
     | a finalizer can only be a member of a ref class
 * - ``2045``\ 
   - | ``finalizer_type_mismatch``\ :
     | type used as finalizer name does not match type *"type"*\ 
 * - ``2046``\ 
   - | ``finalizer_does_not_exist``\ :
     | a finalizer does not exist for this type
 * - ``2047``\ 
   - | ``handle_of_non_managed``\ :
     | the "%" operator can be used only on an object with a managed class
       type
 * - ``2048``\ 
   - | ``ptr_handle_or_ref_to_interior_ptr``\ :
     | a pointer, handle, or reference to a cli::interior_ptr is not allowed
 * - ``2049``\ 
   - | ``ptr_handle_or_ref_to_pin_ptr``\ :
     | a pointer, handle, or reference to a cli::pin_ptr is not allowed
 * - ``2050``\ 
   - | ``ptr_or_ref_to_cli_array``\ :
     | a pointer or reference to a C++/CLI array is not allowed
 * - ``2051``\ 
   - | ``bad_use_of_cli_array_type``\ :
     | a C++/CLI array type is not allowed here
 * - ``2052``\ 
   - | ``invalid_ref_class_base``\ :
     | a C++/CLI ref class can only derive from another ref class or from
       interface classes
 * - ``2053``\ 
   - | ``invalid_value_class_base``\ :
     | a C++/CLI value class can only derive from interface classes
 * - ``2054``\ 
   - | ``invalid_interface_class_base``\ :
     | a C++/CLI interface class can only derive from interface classes
 * - ``2055``\ 
   - | ``ref_class_has_multiple_ref_bases``\ :
     | a ref class can have at most one direct ref base class (*"type"*\  is
       already such a base)
 * - ``2056``\ 
   - | ``managed_base_for_standard_class``\ :
     | a standard class cannot derive from a managed class
 * - ``2057``\ 
   - | ``virtual_base_for_managed_class``\ :
     | a managed class cannot have a virtual base
 * - ``2058``\ 
   - | ``managed_class_type_cannot_have_private_or_protected_base``\ :
     | a managed class cannot have a "private" or "protected" base
 * - ``2059``\ 
   - | ``override_requires_virtual``\ :
     | the "override" modifier requires a virtual function declaration with
       an explicit "virtual" keyword
 * - ``2060``\ 
   - | ``abstract_requires_virtual``\ :
     | the "abstract" modifier requires a virtual function declaration with
       an explicit "virtual" keyword
 * - ``2061``\ 
   - | ``sealed_requires_virtual``\ :
     | the "sealed" modifier requires a virtual function declaration with an
       explicit "virtual" keyword
 * - ``2062``\ 
   - | ``named_override_requires_virtual``\ :
     | a named override specifier requires a virtual function declaration
       with an explicit "virtual" keyword
 * - ``2063``\ 
   - | ``pin_ptr_return_type_not_allowed``\ :
     | a cli::pin_ptr return type is not allowed
 * - ``2064``\ 
   - | ``cppcli_attribute_only``\ :
     | attribute *"xxxx"*\  applies in C++/CLI mode only
 * - ``2065``\ 
   - | ``normal_ref_bound_to_gc_lvalue``\ :
     | a simple (non-tracking) reference cannot be bound to an entity on the
       managed heap
 * - ``2067``\ 
   - | ``cli_entity_not_loaded``\ :
     | "*xxxx*\ " not loaded from default assemblies
 * - ``2068``\ 
   - | ``list_initializer_nonstandard_in_current_mode``\ :
     | list initialization syntax is a C++11 feature
 * - ``2069``\ 
   - | ``sizeof_ref_or_interface_class``\ :
     | operand of sizeof may not be a ref class type or interface class type
 * - ``2070``\ 
   - | ``cli_array_invalid_number_of_subscripts``\ :
     | invalid number of subscripts for this cli::array type
 * - ``2071``\ 
   - | ``ptr_to_member_of_managed_class``\ :
     | a pointer-to-member is not valid for a managed class
 * - ``2072``\ 
   - | ``private_virtual_member_function_not_sealed``\ :
     | private virtual member function of managed class is not "sealed"
 * - ``2073``\ 
   - | ``modifier_not_allowed_on_destructor``\ :
     | modifier is not allowed on a destructor
 * - ``2074``\ 
   - | ``modifier_not_allowed_on_finalizer``\ :
     | modifier is not allowed on a finalizer
 * - ``2075``\ 
   - | ``virtual_has_no_effect``\ :
     | "virtual" has no effect on a destructor of a managed class
 * - ``2076``\ 
   - | ``new_or_override_required``\ :
     | "new" or "override" is required because this declaration matches
       *entity-kind "entity"*\  (declared at line *xxxx*\ )
 * - ``2077``\ 
   - | ``new_or_virtual_required``\ :
     | "new" or "virtual" is required because this declaration matches
       *entity-kind "entity"*\  (declared at line *xxxx*\ )
 * - ``2078``\ 
   - | ``override_for_interface_member``\ :
     | "new" or "override" are not valid here because the matching
       *entity-kind "entity"*\  (declared at line *xxxx*\ ) is a member of
       an interface
 * - ``2079``\ 
   - | ``new_requires_matching_base_member``\ :
     | "new" modifier without a matching base ref class member
 * - ``2080``\ 
   - | ``overriding_reduces_accessibility_in_managed_type``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) overridden with
       reduced access
 * - ``2081``\ 
   - | ``bad_tracking_ref_init``\ :
     | a reference of type *"type"*\  cannot be initialized with a value of
       type *"type"*\ 
 * - ``2082``\ 
   - | ``copy_constructor_in_value_class_type``\ :
     | a copy constructor cannot be declared in a value class
 * - ``2083``\ 
   - | ``default_constructor_in_value_class_type``\ :
     | a default constructor cannot be declared in a value class
 * - ``2084``\ 
   - | ``destructor_in_value_class_type``\ :
     | a destructor cannot be declared in a value class
 * - ``2085``\ 
   - | ``assignment_in_value_class_type``\ :
     | an assignment operator cannot be declared in a value class
 * - ``2086``\ 
   - | ``nonvalue_class_type_cannot_be_value_class_member``\ :
     | non-value class *"type"*\  cannot be the type of a member of a value
       class
 * - ``2087``\ 
   - | ``cppcli_requires_newer_abi_version``\ :
     | option "cppcli" requires a different compiler configuration
 * - ``2088``\ 
   - | ``managed_member_exception_spec``\ :
     | exception specifications are not allowed on member functions of
       managed classes
 * - ``2089``\ 
   - | ``managed_class_cannot_have_friend``\ :
     | a managed class cannot declare a friend
 * - ``2092``\ 
   - | ``local_class_in_managed_member_function``\ :
     | a local class definition is not allowed in a member function of a
       managed class
 * - ``2093``\ 
   - | ``local_lambda_in_managed_member_function``\ :
     | a local lambda is not allowed in a member function of a managed class
 * - ``2094``\ 
   - | ``cli_interface_member_function_definition``\ :
     | a member function of a C++/CLI interface class type cannot have a
       definition
 * - ``2095``\ 
   - | ``missing_get_and_set_accessors``\ :
     | a property definition must include at least one accessor ("get" or
       "set")
 * - ``2096``\ 
   - | ``subscript_mechanism_conflict``\ :
     | default-indexed property conflicts with *entity-kind "entity"*\ 
       (declared at line *xxxx*\ )
 * - ``2097``\ 
   - | ``unusable_pack``\ :
     | *entity-kind "entity"*\  cannot be used because it follows a
       parameter pack and cannot be deduced from the parameters of
       *entity-kind "entity"*\ 
 * - ``2098``\ 
   - | ``excess_pack_expansion``\ :
     | this pack expansion produced more than one expression, and a single
       expression is needed here
 * - ``2099``\ 
   - | ``cli_enum_base_has_no_system_counterpart``\ :
     | type must correspond to System::Boolean, System::Byte, System::SByte,
       System::Int16, System::UInt16, System::Int32, System::UInt32,
       System::Int64, or System::UInt64
 * - ``2100``\ 
   - | ``bad_call_of_handle``\ :
     | call of an object of a handle type without appropriate operator() or
       conversion functions to pointer-to-function type
 * - ``2101``\ 
   - | ``abstract_declarator_pack_is_nested``\ :
     | an unnamed parameter pack declaration cannot be parenthesized
 * - ``2102``\ 
   - | ``cl_variadic_templates_only_in_cplusplus``\ :
     | variadic templates can be enabled only when compiling C++
 * - ``2103``\ 
   - | ``conflicting_properties``\ :
     | property definition conflicts with *entity-kind "entity"*\  (declared
       at line *xxxx*\ )
 * - ``2106``\ 
   - | ``generic_param_cannot_have_default``\ :
     | a generic parameter cannot have a default
 * - ``2107``\ 
   - | ``bad_param_kind_for_generic``\ :
     | a generic can only have type parameters
 * - ``2108``\ 
   - | ``for_each_missing_function``\ :
     | to be used with "for each" statements, type *"type"*\  must provide
       nonstatic member function *"xxxx"*\ 
 * - ``2109``\ 
   - | ``for_each_static_function``\ :
     | "for each" cannot use member *entity-kind "entity"*\  because it is
       static
 * - ``2110``\ 
   - | ``for_each_no_matching_overload``\ :
     | in this "for each" statement, no instance of *"entity"*\  is callable
       with an empty argument list
 * - ``2111``\ 
   - | ``for_each_invalid_return_type_for_move_next``\ :
     | "for each" cannot use member function "MoveNext" because the return
       type is invalid
 * - ``2112``\ 
   - | ``for_each_incompatible_type``\ :
     | a "for each" statement cannot operate on an expression of type
       *"type"*\ 
 * - ``2113``\ 
   - | ``for_each_missing_field``\ :
     | to be used with "for each" statements, type *"type"*\  must provide a
       non-indexed property *"xxxx"*\ 
 * - ``2115``\ 
   - | ``for_each_getenumerator_return_type_invalid``\ :
     | in this "for each" statement, *"type"*\  is not a valid enumerator
       (returned by "GetEnumerator" of *"type"*\ )
 * - ``2116``\ 
   - | ``exp_in``\ :
     | expected "in"
 * - ``2117``\ 
   - | ``no_suitable_synthesis_assignment_operator``\ :
     | class *"type"*\  has no suitable assignment operator (after operator
       synthesis)
 * - ``2118``\ 
   - | ``not_a_generic_param``\ :
     | *"xxxx"*\  is not a generic parameter
 * - ``2119``\ 
   - | ``not_generic_param_of_curr_decl``\ :
     | *"xxxx"*\  is not a generic parameter of the innermost generic
       parameter list
 * - ``2120``\ 
   - | ``invalid_constraint``\ :
     | invalid generic constraint
 * - ``2121``\ 
   - | ``invalid_event_use``\ :
     | invalid use of event member (only subscription, unsubscription, and
       invocation are allowed)
 * - ``2122``\ 
   - | ``event_without_raise_invoked``\ :
     | invoking an event with no "raise" accessor is invalid
 * - ``2123``\ 
   - | ``bad_event_compound_assignment``\ :
     | only "+=" and "-=" are valid for events
 * - ``2124``\ 
   - | ``typeid_of_managed_type``\ :
     | typeid of a managed type is not allowed
 * - ``2125``\ 
   - | ``cli_typeid_of_managed_pointer``\ :
     | typeid of a managed pointer type is not allowed
 * - ``2126``\ 
   - | ``name_before_typeid_not_type``\ :
     | name followed by "::typeid" must be a type name
 * - ``2127``\ 
   - | ``reserved_dispose``\ :
     | a member *"xxxx"*\  of this type is reserved within a managed class
       -- destructor intended?
 * - ``2128``\ 
   - | ``reserved_finalize``\ :
     | a member *"xxxx"*\  of this type is reserved within a managed class
       -- finalizer intended?
 * - ``2129``\ 
   - | ``invalid_idisposable_dispose``\ :
     | System::IDisposable::Dispose is missing or invalid
 * - ``2130``\ 
   - | ``invalid_object_finalize``\ :
     | System::Object::Finalize is missing or invalid
 * - ``2131``\ 
   - | ``finalize_does_not_override_object_finalize``\ :
     | *entity-kind "entity"*\  does not override System::Object::Finalize
 * - ``2132``\ 
   - | ``bad_handle_dynamic_cast_operand``\ :
     | the operand of a handle dynamic_cast must be a handle to a complete
       class type
 * - ``2133``\ 
   - | ``bad_tracking_ref_dynamic_cast_operand``\ :
     | the operand of a tracking-reference dynamic_cast must be an lvalue of
       a complete class type
 * - ``2134``\ 
   - | ``bad_cli_dynamic_cast_type``\ :
     | the type in a dynamic_cast to a handle or tracking reference type
       must refer to a complete class
 * - ``2135``\ 
   - | ``cast_interior_ptr_to_ptr``\ :
     | an interior pointer cannot be cast to a native pointer
 * - ``2136``\ 
   - | ``cppcli_explicit_conversion_only_in_ref_and_value_classes``\ :
     | explicit conversion operators can only be declared in ref and value
       class types
 * - ``2137``\ 
   - | ``cppcli_explicit_conversion_is_virtual``\ :
     | explicit conversion operator cannot be virtual
 * - ``2138``\ 
   - | ``expr_not_arithmetic_or_unscoped_enum``\ :
     | expression must have arithmetic or unscoped enum type
 * - ``2139``\ 
   - | ``expr_not_arithmetic_or_unscoped_enum_or_pointer``\ :
     | expression must have arithmetic, unscoped enum, or pointer type
 * - ``2140``\ 
   - | ``expr_not_integral_or_unscoped_enum``\ :
     | expression must have integral or unscoped enum type
 * - ``2141``\ 
   - | ``expr_not_integral_or_unscoped_enum_or_fixed_point``\ :
     | expression must have integral, unscoped enum, or fixed-point type
 * - ``2142``\ 
   - | ``scoped_enum_operation_type_mismatch``\ :
     | a built-in binary operator applied to a scoped enumeration requires
       two operands of the same type
 * - ``2143``\ 
   - | ``invalid_gcnew_type``\ :
     | gcnew cannot allocate an entity of type *"type"*\ 
 * - ``2144``\ 
   - | ``gcnew_used_with_placement_syntax``\ :
     | placement syntax cannot be used with gcnew
 * - ``2145``\ 
   - | ``new_used_on_unsuitable_value_type``\ :
     | new can only be used with simple value types
 * - ``2146``\ 
   - | ``new_used_on_managed_class_type``\ :
     | new cannot be used on a managed class (gcnew should be used instead)
 * - ``2147``\ 
   - | ``new_used_on_handle_or_tracking_reference_type``\ :
     | new cannot be used on a handle type
 * - ``2148``\ 
   - | ``cli_array_must_have_new_or_array_init``\ :
     | gcnew for a C++/CLI array must have a new initializer or an array
       initializer
 * - ``2149``\ 
   - | ``gcnew_bad_type_used_with_array_init``\ :
     | an array initializer can only be used to initialize a C++/CLI array
       type
 * - ``2150``\ 
   - | ``gcnew_used_with_auto_syntax``\ :
     | gcnew does not allow auto
 * - ``2151``\ 
   - | ``too_many_array_bounds``\ :
     | too many array bounds
 * - ``2152``\ 
   - | ``too_few_array_bounds``\ :
     | too few array bounds
 * - ``2153``\ 
   - | ``too_few_generic_args``\ :
     | too few arguments for *entity-kind "entity"*\ 
 * - ``2154``\ 
   - | ``too_many_generic_args``\ :
     | too many arguments for *entity-kind "entity"*\ 
 * - ``2156``\ 
   - | ``no_matching_arity``\ :
     | no declaration of *entity-kind "entity"*\  accepts the number of
       generic arguments supplied
 * - ``2157``\ 
   - | ``bad_function_for_delegate``\ :
     | invalid delegate initializer -- must be a function
 * - ``2158``\ 
   - | ``ambiguous_function_for_delegate``\ :
     | invalid delegate initializer -- more than one function matches the
       delegate type
 * - ``2159``\ 
   - | ``mismatched_function_for_delegate``\ :
     | invalid delegate initializer -- function does not match the delegate
       type
 * - ``2160``\ 
   - | ``missing_delegate_object``\ :
     | invalid delegate initializer -- an object is needed in addition to a
       function
 * - ``2161``\ 
   - | ``nonmanaged_function_for_delegate``\ :
     | invalid delegate initializer -- function is not a member of a managed
       class
 * - ``2162``\ 
   - | ``superfluous_delegate_object``\ :
     | invalid delegate initializer -- object is not needed for the
       specified function
 * - ``2163``\ 
   - | ``incompatible_delegate_object``\ :
     | invalid delegate initializer -- object has type *"type"*\  but type
       *"type"*\  is expected
 * - ``2164``\ 
   - | ``address_of_managed_member_function``\ :
     | taking the address of a member function of a managed class is not
       allowed
 * - ``2165``\ 
   - | ``bad_delegate_init_list``\ :
     | invalid delegate initializer -- expected either
       "(<function-address>)" or "(<object-handle>, <member-address>)"
 * - ``2166``\ 
   - | ``interface_not_implemented``\ :
     | class fails to implement interface member *entity-kind "entity"*\ 
       (declared at line *xxxx*\ )
 * - ``2167``\ 
   - | ``gcnew_of_native_array``\ :
     | gcnew cannot be used to allocate a native array
 * - ``2168``\ 
   - | ``cli_interface_cannot_have_assignment``\ :
     | a C++/CLI interface class cannot declare an assignment operator
 * - ``2169``\ 
   - | ``sealed_cli_interface``\ :
     | a C++/CLI interface class cannot be sealed
 * - ``2171``\ 
   - | ``destructor_or_finalizer_with_named_override``\ :
     | a destructor or finalizer declaration cannot include a named override
       specifier
 * - ``2172``\ 
   - | ``override_name_is_destructor_or_finalizer``\ :
     | an override specifier cannot designate a destructor or finalizer
 * - ``2173``\ 
   - | ``named_override_requires_managed_type``\ :
     | a named override specifier is allowed only in a managed class
 * - ``2174``\ 
   - | ``named_override_type_mismatch``\ :
     | no member designated by the named override specifier matches the type
       of this member
 * - ``2175``\ 
   - | ``static_constructor_with_named_override``\ :
     | a static constructor declaration cannot include a named override
       specifier
 * - ``2176``\ 
   - | ``unnamed_scoped_enum``\ :
     | a scoped enum type must have a name
 * - ``2177``\ 
   - | ``branch_into_finally``\ :
     | transfer of control into a finally block is not allowed
 * - ``2178``\ 
   - | ``return_from_finally``\ :
     | return statement inside a finally block is not allowed
 * - ``2179``\ 
   - | ``missing_finally``\ :
     | try block requires at least one handler or finally clause
 * - ``2180``\ 
   - | ``managed_object_not_thrown_by_handle``\ :
     | a managed object must be thrown by handle
 * - ``2181``\ 
   - | ``managed_object_not_caught_by_handle``\ :
     | a managed object must be caught by handle
 * - ``2182``\ 
   - | ``break_cannot_be_in_finally_block``\ :
     | a break statement cannot be used in a finally block
 * - ``2183``\ 
   - | ``continue_cannot_be_in_finally_block``\ :
     | a continue statement cannot be used in a finally block
 * - ``2184``\ 
   - | ``no_overloaded_subscript_with_offsetof``\ :
     | builtin offsetof cannot be used when subscripting is overloaded
 * - ``2185``\ 
   - | ``duplicate_constraint``\ :
     | duplicate constraint
 * - ``2186``\ 
   - | ``multiple_class_constraints``\ :
     | more than one class constraint: *"type"*\  and *"type"*\ 
 * - ``2187``\ 
   - | ``multiple_constraint_clauses``\ :
     | more than one constraint clause for *entity-kind "entity"*\ 
 * - ``2188``\ 
   - | ``initonly_static_data_member_not_initialized``\ :
     | initonly static data members must have an initializer or be
       initialized in a static constructor
 * - ``2189``\ 
   - | ``gnu_attr_on_template_redecl``\ :
     | GNU attributes on a template redeclaration have no effect
 * - ``2190``\ 
   - | ``gnu_attr_on_template_redecl_but_original_kept``\ :
     | GNU attributes on a template redeclaration have no effect (the
       attributes of the original declaration at line *xxxx*\  apply instead)
 * - ``2191``\ 
   - | ``cli_param_array_must_be_last_parameter``\ :
     | a C++/CLI parameter array must be the last parameter
 * - ``2192``\ 
   - | ``default_arg_used_in_param_array_function``\ :
     | a function with a C++/CLI parameter array cannot have default
       arguments
 * - ``2193``\ 
   - | ``ellipsis_after_param_array``\ :
     | a C++/CLI parameter array cannot be followed by an ellipsis parameter
 * - ``2194``\ 
   - | ``parameter_array_on_operator_function``\ :
     | a C++/CLI parameter array is not allowed in an operator function
       parameter list
 * - ``2195``\ 
   - | ``microsoft_inline_not_allowed_here``\ :
     | __inline and \__forceinline are not allowed here
 * - ``2196``\ 
   - | ``data_member_with_interface_type``\ :
     | a data member cannot have a C++/CLI interface class type
 * - ``2197``\ 
   - | ``variable_with_interface_type``\ :
     | a variable cannot have a C++/CLI interface class type
 * - ``2198``\ 
   - | ``parameter_with_interface_type``\ :
     | a parameter cannot have a C++/CLI interface class type
 * - ``2199``\ 
   - | ``return_type_is_interface``\ :
     | a function return type cannot be a C++/CLI interface class type
 * - ``2200``\ 
   - | ``array_of_generic_param``\ :
     | an array of generic parameter type is not allowed
 * - ``2201``\ 
   - | ``ptr_handle_or_ref_to_generic_param``\ :
     | a pointer, handle, or reference to a generic parameter type is not
       allowed
 * - ``2202``\ 
   - | ``ref_class_initonly_field``\ :
     | an initonly field cannot have a ref class type
 * - ``2203``\ 
   - | ``ref_bound_to_initonly_field``\ :
     | a reference cannot be bound to an initonly field
 * - ``2204``\ 
   - | ``address_of_initonly_field``\ :
     | taking the address of an initonly field is not allowed
 * - ``2205``\ 
   - | ``modification_of_initonly_field``\ :
     | an initonly field can only be modified by the instance constructor of
       its containing class
 * - ``2206``\ 
   - | ``modification_of_static_initonly_field``\ :
     | a static initonly field can only be modified by the static
       constructor of its containing class
 * - ``2207``\ 
   - | ``member_function_call_on_initonly_field``\ :
     | member function will be invoked on a copy of the initonly field
 * - ``2208``\ 
   - | ``expr_not_pointer_nor_handle``\ :
     | expression must have pointer or handle type but it has type *"type"*\ 
 * - ``2209``\ 
   - | ``move_ctor_or_assign_copy_of_lvalue``\ :
     | a move constructor or move assignment operator is used to copy an
       lvalue here, which may destroy the source object
 * - ``2210``\ 
   - | ``generic_selection_with_points_to``\ :
     | member selection on a C++/CLI generic entity must use the "->"
       syntax, not "."
 * - ``2211``\ 
   - | ``invalid_specific_ref_class_base``\ :
     | a ref class type cannot derive from *"type"*\ 
 * - ``2212``\ 
   - | ``generic_class_must_be_managed``\ :
     | a generic class must be managed (i.e., a ref class, a value class, or
       an interface class)
 * - ``2213``\ 
   - | ``sealed_constraint``\ :
     | a sealed class cannot be used as a constraint
 * - ``2214``\ 
   - | ``dynamic_cast_to_value_generic``\ :
     | the type in a dynamic_cast cannot be a generic type that might be a
       value type
 * - ``2215``\ 
   - | ``UCN_names_invalid_code_point``\ :
     | a universal character name must designate a valid code point
 * - ``2216``\ 
   - | ``override_with_constraint_mismatch``\ :
     | generic constraints do not match those of *entity-kind "entity"*\ 
       (declared at line *xxxx*\ )
 * - ``2217``\ 
   - | ``bad_argument_for_underlying_type``\ :
     | __underlying_type only applies to enum types
 * - ``2218``\ 
   - | ``too_many_cast_operands``\ :
     | expected only one operand expression for this cast
 * - ``2219``\ 
   - | ``bad_unicode_char_in_string``\ :
     | Unicode character with hex value *xxxx*\  not representable in the
       system default code page
 * - ``2220``\ 
   - | ``conv_of_pm_to_func_ptr``\ :
     | nonstandard conversion of bound pointer-to-member to a function
       pointer
 * - ``2221``\ 
   - | ``deprecated_access_specifier``\ :
     | access specifier *xxxx*\  is deprecated -- use *xxxx*\  instead
 * - ``2222``\ 
   - | ``static_accessor_in_nonstatic_property_or_event``\ :
     | a static accessor function is not permitted in a nonstatic property
       or event definition
 * - ``2223``\ 
   - | ``both_ref_and_value_constraints``\ :
     | *"type"*\  has both a value class and ref class constraint
 * - ``2224``\ 
   - | ``circular_constraints``\ :
     | *"type"*\  and *"type"*\  involve circular naked type constraints
 * - ``2225``\ 
   - | ``invalid_type_constraint``\ :
     | *"type"*\  is not a valid type constraint
 * - ``2226``\ 
   - | ``pch_file_incomplete``\ :
     | precompiled header file *"xxxx"*\  not used (because it is incomplete)
 * - ``2227``\ 
   - | ``invalid_generic_arg``\ :
     | *"type"*\  is not a valid generic argument
 * - ``2228``\ 
   - | ``bad_assembly_info_attribute``\ :
     | assembly_info attribute applied to an invalid type
 * - ``2229``\ 
   - | ``ref_class_not_satisfied``\ :
     | *"type"*\  does not satisfy the ref class constraint of generic
       parameter *"type"*\ 
 * - ``2230``\ 
   - | ``value_class_not_satisfied``\ :
     | *"type"*\  does not satisfy the value class constraint of generic
       parameter *"type"*\ 
 * - ``2231``\ 
   - | ``gcnew_and_abstract``\ :
     | *"type"*\  does not satisfy the gcnew constraint of generic parameter
       *"type"*\  because it is abstract
 * - ``2232``\ 
   - | ``gcnew_and_no_ctor``\ :
     | *"type"*\  does not satisfy the gcnew constraint of generic parameter
       *"type"*\  because it does not have a public default constructor
 * - ``2233``\ 
   - | ``gcnew_and_no_gcnew``\ :
     | generic parameter *"type"*\  does not satisfy the gcnew constraint of
       generic parameter *"type"*\  because it does not have the gcnew
       constraint
 * - ``2234``\ 
   - | ``type_not_satisfied``\ :
     | *"type"*\  does not satisfy the *"type"*\  type constraint of generic
       parameter *"type"*\ 
 * - ``2235``\ 
   - | ``constraint_mismatch``\ :
     | constraint on generic parameter *"type"*\  differs from previous
       declaration (at line *xxxx*\ )
 * - ``2236``\ 
   - | ``standard_array_member_in_managed_class``\ :
     | a member of a managed class cannot be a standard array
 * - ``2237``\ 
   - | ``handle_member_in_standard_class``\ :
     | a member of a non-managed class cannot be a handle
 * - ``2238``\ 
   - | ``tracking_reference_member_in_standard_class``\ :
     | a member of a non-managed class cannot be a tracking reference
 * - ``2239``\ 
   - | ``reinterpret_cast_of_handle``\ :
     | unsafe reinterpret_cast of handle
 * - ``2240``\ 
   - | ``generic_type_in_template_arg``\ :
     | a template argument may not reference a generic type parameter
 * - ``2241``\ 
   - | ``comma_operator_in_cli_subscript``\ :
     | an expression list is not allowed in this subscript operation (use
       parentheses around a top-level comma operator)
 * - ``2242``\ 
   - | ``expr_not_pointer_or_array_handle``\ :
     | expression must have pointer-to-object or handle-to-C++/CLI-array
       type but it has type *"type"*\ 
 * - ``2243``\ 
   - | ``unrecognized_ms_attr``\ :
     | unrecognized attribute
 * - ``2244``\ 
   - | ``standard_class_member_in_managed_class``\ :
     | a member of a managed class cannot be of a non-managed class type
 * - ``2245``\ 
   - | ``ref_or_interface_class_member_in_standard_class``\ :
     | a member of a non-managed class cannot have a ref class type or
       interface class type
 * - ``2247``\ 
   - | ``template_delegate``\ :
     | a delegate may not be declared as a template
 * - ``2248``\ 
   - | ``invalid_generic_specialization``\ :
     | a generic cannot be explicitly specialized
 * - ``2249``\ 
   - | ``generic_in_template``\ :
     | a generic cannot be declared in a class template
 * - ``2250``\ 
   - | ``template_in_generic``\ :
     | a template cannot be declared in a generic class
 * - ``2251``\ 
   - | ``static_literal_field``\ :
     | a literal field cannot be declared "static"
 * - ``2252``\ 
   - | ``nonstandard_long_float``\ :
     | "long float" is a nonstandard extension -- use "double" instead
 * - ``2253``\ 
   - | ``standard_class_nested_in_managed_class``\ :
     | a standard class cannot be nested in a managed class
 * - ``2254``\ 
   - | ``clrcall_requires_cppcli``\ :
     | __clrcall is valid only in C++/CLI mode
 * - ``2255``\ 
   - | ``vararg_clrcall``\ :
     | __clrcall not allowed on function with ellipsis parameter
 * - ``2256``\ 
   - | ``bad_use_of_function_modifier``\ :
     | *"xxxx"*\  is not allowed here
 * - ``2257``\ 
   - | ``override_with_trivial_property_or_event``\ :
     | a trivial property or event cannot be used to override *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``2258``\ 
   - | ``exp_id_in_for_each_decl``\ :
     | expected an iterator variable name
 * - ``2259``\ 
   - | ``missing_notequal_on_for_each_type``\ :
     | the iterator type in this "for each" statement is *"type"*\ , which
       is not a pointer type or an iterator-like class type
 * - ``2260``\ 
   - | ``missing_incr_on_for_each_type``\ :
     | the iterator type in this "for each" statement is *"type"*\ , which
       is not a pointer type or an iterator-like class type
 * - ``2261``\ 
   - | ``missing_indirect_on_for_each_type``\ :
     | the iterator type in this "for each" statement is *"type"*\ , which
       is not a pointer type or an iterator-like class type
 * - ``2262``\ 
   - | ``no_packing_of_non_POD_field``\ :
     | packing attribute on the parent type is ignored for this field of
       class type *"type"*\  that is not standard-layout
 * - ``2263``\ 
   - | ``nonpublic_implicit_interface_match``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) not implemented
       because this declaration is not public and has no named override
       specifier
 * - ``2264``\ 
   - | ``missing_gnu_inline_attr_on_redeclaration``\ :
     | this declaration is missing the gnu_inline attribute specified in the
       previous declaration at line *xxxx*\ 
 * - ``2265``\ 
   - | ``managed_member_function_cannot_have_ellipsis_parameter``\ :
     | a member function of a managed class cannot have an ellipsis parameter
 * - ``2266``\ 
   - | ``invalid_prev_decl_iterator``\ :
     | previously-declared *entity-kind "entity"*\  invalid as iterator of
       "for each" statement
 * - ``2267``\ 
   - | ``generic_parameter_requires_clrcall``\ :
     | calling convention ignored because the function type involves a
       generic parameter; \__clrcall used instead
 * - ``2268``\ 
   - | ``generic_parameter_does_not_permit_varargs``\ :
     | a function type involving a generic parameter cannot have an ellipsis
       parameter
 * - ``2269``\ 
   - | ``virtual_required_for_base_override``\ :
     | "virtual" is required to override the matching *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``2270``\ 
   - | ``virtual_required_for_interface_implementation``\ :
     | "virtual" is required to implement the matching *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``2271``\ 
   - | ``initonly_volatile_not_allowed``\ :
     | an initonly data member cannot be volatile
 * - ``2272``\ 
   - | ``member_name_reserved_by_cli_operator``\ :
     | a member *"xxxx"*\  of this type is reserved within a managed class
       -- C++/CLI operators must be declared using the keyword "operator"
 * - ``2273``\ 
   - | ``tracking_ref_to_constant``\ :
     | a tracking reference to non-const cannot be bound to a constant
 * - ``2274``\ 
   - | ``attributes_with_no_decl``\ :
     | attributes ignored here because they do not apply to a declared entity
 * - ``2275``\ 
   - | ``tracking_reference_to_system_string``\ :
     | a tracking reference to System::String is not allowed
 * - ``2276``\ 
   - | ``use_of_generic_class_with_pending_constraint``\ :
     | invalid use of a generic class *"type"*\  with pending constraints
       (probably caused by an invalid metadata file)
 * - ``2277``\ 
   - | ``invalid_entity_for_pending_constraint``\ :
     | a pending constraint clause is only allowed for generic class
       declarations (but not generic class definitions)
 * - ``2278``\ 
   - | ``invalid_empty_initializer_list``\ :
     | empty initializer list not allowed here
 * - ``2279``\ 
   - | ``template_in_managed_class``\ :
     | a template cannot be declared in a managed class
 * - ``2280``\ 
   - | ``bad_generic_declaration_scope``\ :
     | a generic declaration is not allowed here
 * - ``2281``\ 
   - | ``interface_cannot_have_member_generics``\ :
     | interface types cannot have member generics
 * - ``2282``\ 
   - | ``character_not_latin_1``\ :
     | Unicode character not Latin-1, truncated to low-order byte
 * - ``2283``\ 
   - | ``range_based_for_missing_function``\ :
     | to be used with range-based "for" statements, type *"type"*\  must
       provide function *"xxxx"*\ 
 * - ``2284``\ 
   - | ``missing_notequal_on_range_based_for_type``\ :
     | the iterator type in this range-based "for" statement is *"type"*\ ,
       which is not a pointer type or an iterator-like class type
 * - ``2285``\ 
   - | ``missing_incr_on_range_based_for_type``\ :
     | the iterator type in this range-based "for" statement is *"type"*\ ,
       which is not a pointer type or an iterator-like class type
 * - ``2286``\ 
   - | ``missing_indirect_on_range_based_for_type``\ :
     | the iterator type in this range-based "for" statement is *"type"*\ ,
       which is not a pointer type or an iterator-like class type
 * - ``2287``\ 
   - | ``range_based_for_incomplete_array_type``\ :
     | a range-based "for" statement cannot operate on an array of unknown
       size or incomplete type *"type"*\ 
 * - ``2288``\ 
   - | ``begin_end_type_mismatch_in_range_based_for``\ :
     | return types for "begin" and "end" functions used in a range-based
       "for" statement must be the same ("begin" return type is *"type"*\ ,
       "end" return type is *"type"*\ )
 * - ``2289``\ 
   - | ``inaccessible_elided_dtor``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ), required to
       destroy temporary that was eliminated, is inaccessible
 * - ``2290``\ 
   - | ``range_based_for_no_matching_overload``\ :
     | in this range-based "for" statement, no instance of *"entity"*\ 
       matches the argument list
 * - ``2291``\ 
   - | ``range_based_for_undefined_identifier``\ :
     | this range-based "for" statement requires a suitable *"xxxx"*\ 
       function and none was found
 * - ``2292``\ 
   - | ``for_each_undefined_identifier``\ :
     | this "for each" statement requires a suitable *"xxxx"*\  function and
       none was found
 * - ``2293``\ 
   - | ``class_metadata_not_representable``\ :
     | *"type"*\  has a metadata representation not representable using
       C++/CLI
 * - ``2294``\ 
   - | ``exp_ellipsis``\ :
     | expected "..."
 * - ``2295``\ 
   - | ``implements_requires_interface``\ :
     | *"type"*\  in \__implements list is not an interface
 * - ``2296``\ 
   - | ``implements_must_precede_virtual_functions``\ :
     | an \__implements list must precede virtual function declarations
 * - ``2297``\ 
   - | ``missing_implements_list``\ :
     | *"type"*\  specified "\__implements ..." in its list of bases, but is
       missing a matching \__implements list
 * - ``2298``\ 
   - | ``unused_dereference_of_ref_class``\ :
     | the result of dereferencing a handle to a ref or interface class type
       must be used
 * - ``2300``\ 
   - | ``exp_rparen_and_pragma_ignored``\ :
     | expected a ")"; pragma ignored
 * - ``2301``\ 
   - | ``using_or_access_declaration_in_managed_class``\ :
     | a using-declaration or access declaration cannot appear in a managed
       class
 * - ``2302``\ 
   - | ``skipped_inaccessible_function``\ :
     | Note: *entity-kind "entity"*\  (declared at line *xxxx*\ ) could have
       been called but was not considered because it is inaccessible
 * - ``2303``\ 
   - | ``cli_abstract_member_function_definition``\ :
     | an abstract member function of a C++/CLI managed class cannot have a
       definition
 * - ``2304``\ 
   - | ``nonstatic_addressof_operator_in_managed_class``\ :
     | declaring this unary "operator\*" can change the meaning of
       dereferencing a handle (use static member operators to explicitly
       indicate applicable types)
 * - ``2319``\ 
   - | ``interface_nonstatic_data_member``\ :
     | an interface class cannot contain a nonstatic data member
 * - ``2320``\ 
   - | ``pragma_gcc_system_header_in_primary_file``\ :
     | #pragma GCC system_header cannot be used in the primary source file
 * - ``2321``\ 
   - | ``too_large_to_inline``\ :
     | *entity-kind "entity"*\  is too large to be inlined
 * - ``2323``\ 
   - | ``cl_gen_move_operations_option_only_in_cplusplus``\ :
     | option to control move operations can be used only when compiling C++
 * - ``2324``\ 
   - | ``cl_gen_move_operations_and_rvalue_ctor_is_copy_ctor``\ :
     | move operations cannot be generated when rvalue constructors are copy
       constructors
 * - ``2325``\ 
   - | ``cl_move_operations_require_rvalue_references``\ :
     | option to control move operations cannot be used when rvalue
       references are disabled
 * - ``2326``\ 
   - | ``final_managed_class``\ :
     | "final" cannot be used for managed classes (use "sealed" instead)
 * - ``2327``\ 
   - | ``cast_to_cli_interface_class``\ :
     | a cast to CLI interface class *"type"*\  is not allowed -- cast to
       handle intended?
 * - ``2328``\ 
   - | ``new_of_cli_interface_class``\ :
     | cannot create an object of a CLI interface class
 * - ``2329``\ 
   - | ``enum_type_replacement``\ :
     | this declaration hides the nonstandard declaration of *entity-kind
       "entity"*\  (declared at line *xxxx*\ ) because the underlying types
       are incompatible
 * - ``2330``\ 
   - | ``known_comparison_with_null``\ :
     | pointer comparison result is constant, because operand can never be
       null
 * - ``2331``\ 
   - | ``value_init_of_incomplete``\ :
     | an object of the incomplete type *"type"*\  cannot be
       value-initialized
 * - ``2332``\ 
   - | ``value_init_of_reference``\ :
     | a reference cannot be value-initialized
 * - ``2333``\ 
   - | ``exp_lparen_or_brace``\ :
     | expected a "(" or a "{"
 * - ``2334``\ 
   - | ``explicit_ctor_in_copy_list_init``\ :
     | copy-list-initialization cannot use a constructor marked "explicit"
 * - ``2335``\ 
   - | ``ptr_to_member_of_type_void``\ :
     | pointer to member of type void is not allowed
 * - ``2336``\ 
   - | ``ptr_to_member_of_reference_type``\ :
     | pointer to member of reference type is not allowed
 * - ``2337``\ 
   - | ``ptr_to_member_of_handle_type``\ :
     | pointer to member of handle type is not allowed
 * - ``2338``\ 
   - | ``braced_init_list_not_allowed``\ :
     | a brace-enclosed list is not allowed here
 * - ``2339``\ 
   - | ``arrow_star_operator_in_managed_class``\ :
     | an operator->\* member is not allowed in a managed class
 * - ``2340``\ 
   - | ``bad_assembly_index``\ :
     | assembly metadata refers to non-existent assembly
 * - ``2341``\ 
   - | ``attribute_conflict``\ :
     | attribute *"xxxx"*\  conflicts with earlier attribute *"xxxx"*\ 
 * - ``2342``\ 
   - | ``incompatible_enum_base_types``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) was previously
       declared with a different base type
 * - ``2343``\ 
   - | ``invalid_scoped_enum_elaboration``\ :
     | "enum class" and "enum struct" cannot be used here (use plain "enum"
       instead)
 * - ``2344``\ 
   - | ``extra_braces_on_simple_init``\ :
     | only one level of braces is allowed on an initializer for an object
       of type *"type"*\ 
 * - ``2345``\ 
   - | ``not_an_enum_type_name``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) cannot be used
       as an enum type name
 * - ``2347``\ 
   - | ``auto_new_with_braced_init``\ :
     | a braced-initializer cannot be used with "new auto"
 * - ``2348``\ 
   - | ``missing_initializer_list_ctor``\ :
     | the definition of std::initializer_list does not contain the expected
       constructor
 * - ``2349``\ 
   - | ``variable_hides_entity``\ :
     | declaration hides *entity-kind "entity"*\  (declared at line *xxxx*\ )
 * - ``2350``\ 
   - | ``invalid_std_initializer_list_parameter_list``\ :
     | invalid template parameter list for std::initializer_list (it should
       be one ordinary type parameter with no default)
 * - ``2351``\ 
   - | ``braced_list_passed_to_ellipsis``\ :
     | a brace-enclosed list cannot be passed for an ellipsis parameter
 * - ``2352``\ 
   - | ``initializer_list_not_included``\ :
     | an #include <initializer_list> is needed prior to a use of
       std::initializer_list, including an implicit use
 * - ``2353``\ 
   - | ``inline_on_alias``\ :
     | the "inline" keyword cannot be used on a namespace alias declaration
 * - ``2354``\ 
   - | ``prev_ns_not_inline``\ :
     | the previous declaration of *entity-kind "entity"*\  was not declared
       inline
 * - ``2355``\ 
   - | ``prev_ns_inline``\ :
     | *entity-kind "entity"*\  was previously declared inline
 * - ``2356``\ 
   - | ``first_arg_must_be_integer_constant``\ :
     | the first argument must be an integer constant
 * - ``2357``\ 
   - | ``designator_requires_aggregate_type``\ :
     | a designator cannot be used with a non-aggregate type *"type"*\ 
 * - ``2358``\ 
   - | ``indirect_anon_union_designator``\ :
     | a designator for an anonymous union member can only appear within
       braces corresponding to that anonymous union
 * - ``2359``\ 
   - | ``cl_func_prototype_tags_option_only_in_C``\ :
     | function prototype tags can only be enabled when compiling C
 * - ``2360``\ 
   - | ``cannot_elide_braces``\ :
     | braces cannot be omitted for this subobject initializer
 * - ``2361``\ 
   - | ``narrowing_conversion``\ :
     | invalid narrowing conversion from *"type"*\  to *"type"*\ 
 * - ``2362``\ 
   - | ``constant_narrowing_conversion``\ :
     | invalid narrowing conversion from *"type"*\  to *"type"*\ : constant
       value does not fit in destination type
 * - ``2363``\ 
   - | ``cast_to_incomplete_array_type``\ :
     | cast to incomplete array type *"type"*\  is not allowed
 * - ``2364``\ 
   - | ``constant_narrowing_conversion_to_float``\ :
     | invalid narrowing conversion from *"type"*\  to *"type"*\ : constant
       value cannot be represented exactly in destination type
 * - ``2365``\ 
   - | ``braced_init_in_paren_init``\ :
     | a parenthesized initializer for a non-class entity must be an
       expression, not a brace-enclosed list
 * - ``2366``\ 
   - | ``braced_list_for_implicit_lambda_type``\ :
     | a brace-enclosed list does not provide a return type for this lambda
 * - ``2367``\ 
   - | ``invalid_explicit_exception_specification``\ :
     | the declared exception specification is incompatible with the
       generated one
 * - ``2368``\ 
   - | ``scoped_enum_nonstandard_in_current_mode``\ :
     | scoped enum types are a C++11 feature
 * - ``2369``\ 
   - | ``value_init_of_function``\ :
     | a function type cannot be value-initialized
 * - ``2370``\ 
   - | ``list_init_of_incomplete``\ :
     | list-initialization of an object type *"type"*\  is not allowed
       because the type is incomplete
 * - ``2371``\ 
   - | ``std_initializer_list_has_dtor``\ :
     | std::initializer_list has a destructor, and is not supposed to --
       library is misconfigured
 * - ``2372``\ 
   - | ``explicit_enum_base_nonstandard_in_current_mode``\ :
     | explicit enum base types are a C++11 feature
 * - ``2373``\ 
   - | ``unconvertible_con_expr``\ :
     | this constant expression has type *"type"*\  instead of the required
       *"type"*\  type
 * - ``2374``\ 
   - | ``new_of_initializer_list``\ :
     | a "new" of an std::initializer_list object is unlikely to work as
       expected because the underlying array will be destroyed at the end of
       the full expression
 * - ``2377``\ 
   - | ``defined_always_false``\ :
     | "defined" is always false in a macro expansion in Microsoft mode
 * - ``2378``\ 
   - | ``init_list_element_type_not_complete_object``\ :
     | *"type"*\  cannot be the element type of an initializer list because
       it is not a complete object type
 * - ``2379``\ 
   - | ``invalid_default_arg``\ :
     | mismatched delimiters in default argument expression
 * - ``2380``\ 
   - | ``conv_of_unbound_pm_to_func_ptr``\ :
     | nonstandard conversion of pointer-to-member to a function pointer
 * - ``2381``\ 
   - | ``dynamic_exception_specifications_deprecated``\ :
     | dynamic exception specifications are deprecated
 * - ``2382``\ 
   - | ``bad_scope_for_partial_spec``\ :
     | *entity-kind "entity"*\  cannot be partially specialized in the
       current scope
 * - ``2383``\ 
   - | ``previous_constexpr_decl_conflict``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) was previously
       declared constexpr
 * - ``2384``\ 
   - | ``previous_nonconstexpr_decl_conflict``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) was previously
       not declared constexpr
 * - ``2385``\ 
   - | ``constexpr_variable_decl_must_be_definition``\ :
     | missing initializer for constexpr variable
 * - ``2386``\ 
   - | ``invalid_constexpr``\ :
     | "constexpr" is not valid here
 * - ``2387``\ 
   - | ``invalid_constexpr_body``\ :
     | a constexpr function must contain exactly one return statement
 * - ``2388``\ 
   - | ``invalid_statement_in_constexpr_function``\ :
     | statement may not appear in a constexpr function
 * - ``2389``\ 
   - | ``invalid_statement_in_constexpr_constructor``\ :
     | statement may not appear in a constexpr constructor
 * - ``2390``\ 
   - | ``constexpr_virtual_combination``\ :
     | a function cannot be both constexpr and virtual in this mode
 * - ``2391``\ 
   - | ``nonliteral_return_type_in_constexpr_function``\ :
     | a constexpr function cannot have a nonliteral return type *"type"*\ 
 * - ``2392``\ 
   - | ``nonliteral_param_type_in_constexpr_function``\ :
     | a constexpr function cannot have a parameter of nonliteral type
       *"type"*\ 
 * - ``2393``\ 
   - | ``unsequenced_use_of_variable``\ :
     | unsequenced uses of *entity-kind "entity"*\  in expression may
       produce undefined results
 * - ``2394``\ 
   - | ``3rd_arg_of_assume_aligned_must_be_integral``\ :
     | the optional third argument of a call to \__builtin_assumed_aligned
       must have integral type
 * - ``2395``\ 
   - | ``constexpr_destructor``\ :
     | a destructor cannot be constexpr
 * - ``2396``\ 
   - | ``invalid_mmap_address``\ :
     | address supplied for mmap must be aligned on a page boundary: *xxxx*\ 
 * - ``2397``\ 
   - | ``constexpr_constructor_with_function_try_block``\ :
     | the body of a constexpr constructor cannot be a function try block
 * - ``2398``\ 
   - | ``missing_initializer_on_fields_with_constexpr_ctor``\ :
     | constexpr *entity-kind "entity"*\  provides no initializer for:
 * - ``2400``\ 
   - | ``default_ctor_call_not_constant``\ :
     | calling the default constructor for *"type"*\  does not produce a
       constant value
 * - ``2401``\ 
   - | ``default_ctor_not_constexpr``\ :
     | the default constructor for *"type"*\  is not constexpr
 * - ``2402``\ 
   - | ``constexpr_variable_must_have_literal_type``\ :
     | a constexpr variable must have a literal type or a reference type
 * - ``2403``\ 
   - | ``constexpr_ctor_with_virtual_base``\ :
     | a constructor for a class with virtual bases cannot be constexpr
 * - ``2404``\ 
   - | ``bad_cpp11_constant_function_call``\ :
     | function call must have a constant value in a constant expression
 * - ``2405``\ 
   - | ``constexpr_main``\ :
     | function "main" may not be declared constexpr
 * - ``2407``\ 
   - | ``tag_defined_in_constexpr_body``\ :
     | a class or enum type definition cannot appear in a constexpr function
       or constructor body
 * - ``2408``\ 
   - | ``only_gnu_attributes_here``\ :
     | only GNU-style attributes are permitted here
 * - ``2409``\ 
   - | ``auto_used_two_ways``\ :
     | nonstandard use of "auto" to both deduce the type from an initializer
       and to announce a trailing return type
 * - ``2410``\ 
   - | ``nonstd_qualified_void_param_list``\ :
     | declaring a void parameter list with a qualified void type is
       nonstandard
 * - ``2411``\ 
   - | ``qualifier_ignored_on_local_declaration``\ :
     | the qualifier on this local declaration is ignored
 * - ``2412``\ 
   - | ``bad_conv_constant_expr_type``\ :
     | this constant expression has type *"type"*\  instead of the required
       *xxxx*\  type
 * - ``2413``\ 
   - | ``bad_argument_for_bases``\ :
     | an instantiation of \__bases or \__direct_bases requires a class type
 * - ``2414``\ 
   - | ``bad_prototype_argument_for_bases``\ :
     | the argument of \__bases and \__direct_bases must be a type template
       parameter
 * - ``2415``\ 
   - | ``bases_not_in_template``\ :
     | *xxxx*\  can only be used in template contexts
 * - ``2416``\ 
   - | ``constexpr_return_not_constant``\ :
     | constexpr function return is non-constant
 * - ``2417``\ 
   - | ``nonconstexpr_call_in_mem_initializer``\ :
     | constexpr constructor calls non-constexpr *entity-kind "entity"*\ 
 * - ``2418``\ 
   - | ``nonconstant_field_initializer_in_mem_initializer``\ :
     | constructor cannot be constexpr because the initializer of
       *entity-kind "entity"*\  is not a constant expression
 * - ``2419``\ 
   - | ``nonconstant_mem_init_for_constexpr_ctor``\ :
     | non-constant initializer for constexpr constructor
 * - ``2420``\ 
   - | ``generated_default_constructor_used_in_field_initializer``\ :
     | the generated default constructor for *"type"*\  cannot be used in an
       initializer for its own data member
 * - ``2421``\ 
   - | ``recursive_initializer_instantiation``\ :
     | instantiation of initializer of *entity-kind "entity"*\  depends on
       its own value
 * - ``2422``\ 
   - | ``defaulted_default_ctor_cannot_be_constexpr``\ :
     | defaulted default constructor cannot be constexpr because the
       corresponding implicitly declared default constructor would not be
       constexpr
 * - ``2424``\ 
   - | ``bad_binary_digit``\ :
     | invalid binary number
 * - ``2425``\ 
   - | ``multiple_union_field_initializers``\ :
     | a union can have at most one field initializer -- *entity-kind
       "entity"*\  (declared at line *xxxx*\ ) also has an initializer
 * - ``2427``\ 
   - | ``union_constexpr_constructor_initializes_no_field``\ :
     | constexpr constructor of a union must initialize one of its fields
 * - ``2428``\ 
   - | ``constexpr_constructor_initializes_no_variant_field``\ :
     | constexpr constructor fails to initialize an anonymous union (defined
       at line *xxxx*\ )
 * - ``2429``\ 
   - | ``constexpr_static_data_member_without_initializer``\ :
     | a constexpr static data member declaration requires an in-class
       initializer
 * - ``2430``\ 
   - | ``cl_max_constexpr_option_only_in_cplusplus``\ :
     | maximum constexpr depth/count options can be used only when compiling
       C++
 * - ``2431``\ 
   - | ``excessive_constexpr_complexity``\ :
     | expression not folded to a constant due to excessive constexpr
       function call complexity
 * - ``2432``\ 
   - | ``cl_unrestricted_unions_option_only_in_cplusplus``\ :
     | unrestricted union options can be used only when compiling C++
 * - ``2433``\ 
   - | ``constexpr_ctor_does_not_initialize_base``\ :
     | constexpr constructor must initialize direct base class *"type"*\ 
 * - ``2434``\ 
   - | ``field_initializer_list``\ :
     | creation of an std::initializer_list object in a field initializer is
       unlikely to work as expected because the underlying array will be
       destroyed at the end of the full expression
 * - ``2435``\ 
   - | ``this_not_constant``\ :
     | "this" cannot be used in a constant expression
 * - ``2437``\ 
   - | ``constexpr_explicit_instantiation``\ :
     | "constexpr" is not allowed on an explicit instantiation directive
 * - ``2438``\ 
   - | ``generated_default_ctor_exception_spec_circularity``\ :
     | cannot determine the exception specification of the default
       constructor due to a circular dependency
 * - ``2439``\ 
   - | ``anon_union_at_decl_position``\ :
     | anonymous union defined at line *xxxx*\ 
 * - ``2440``\ 
   - | ``unbounded_constexpr_ctor_init_recursion``\ :
     | this constructor uses the initializer of *entity-kind "entity"*\ 
       (declared at line *xxxx*\ ), which would result in unbounded recursion
 * - ``2442``\ 
   - | ``block_extern_initializer_not_allowed``\ :
     | an initializer is not allowed on a local declaration of an extern
       variable
 * - ``2443``\ 
   - | ``local_named_register_initializer_not_allowed``\ :
     | an initializer is not allowed on a local declaration of a named
       register variable
 * - ``2445``\ 
   - | ``cl_unrestricted_unions_in_microsoft_mode``\ :
     | unrestricted unions cannot be enabled in Microsoft mode
 * - ``2446``\ 
   - | ``delegation_loop``\ :
     | constructor delegates directly or indirectly to itself
 * - ``2447``\ 
   - | ``delegation_init_and_mem_init``\ :
     | a delegating constructor cannot have other mem-initializers
 * - ``2448``\ 
   - | ``ref_qualifier_not_allowed``\ :
     | a ref-qualifier is not allowed here
 * - ``2449``\ 
   - | ``same_param_types_with_and_without_ref_qualifiers``\ :
     | overloading two member functions with the same parameter types
       requires that they both have ref-qualifiers or both lack
       ref-qualifiers
 * - ``2450``\ 
   - | ``bad_raw_string_delim_char``\ :
     | invalid character in raw string delimiter
 * - ``2451``\ 
   - | ``missing_raw_string_delim_lparen``\ :
     | parenthesis terminating raw string delimiter not found within 16
       characters -- raw string indicator ignored
 * - ``2452``\ 
   - | ``missing_raw_string_delimiter``\ :
     | ending delimiter for raw string not found
 * - ``2453``\ 
   - | ``pack_not_last_arg``\ :
     | a parameter pack must be the final template argument in a partial
       specialization
 * - ``2454``\ 
   - | ``pm_call_obj_not_lvalue``\ :
     | a pointer-to-member function with type *"type"*\  can only be used
       with an lvalue object
 * - ``2455``\ 
   - | ``pm_call_obj_not_rvalue``\ :
     | a pointer-to-member function with type *"type"*\  can only be used
       with an rvalue object
 * - ``2456``\ 
   - | ``defaulted_copy_ctor_cannot_have_const_parameter``\ :
     | the parameter of this defaulted copy-constructor cannot be const
       because a base or member copy constructor parameter is non-const
 * - ``2457``\ 
   - | ``defaulted_assignment_cannot_have_const_parameter``\ :
     | the parameter of this defaulted assignment operator cannot be const
       because a base or member copy assignment parameter is non-const
 * - ``2458``\ 
   - | ``empty_anonymous_union``\ :
     | an anonymous union must contain at least one nonstatic data member
 * - ``2459``\ 
   - | ``delegating_constructor_requires_newer_abi_version``\ :
     | option "delegating_constructors" requires a different compiler
       configuration
 * - ``2460``\ 
   - | ``alignment_reduction_unconditionally_ignored``\ :
     | a reduction in alignment is ignored
 * - ``2461``\ 
   - | ``bad_rvalue_ref_const_cast_operand``\ :
     | the operand of a const_cast to an rvalue reference type cannot be a
       non-class prvalue
 * - ``2462``\ 
   - | ``expr_not_a_glvalue``\ :
     | expression must be an lvalue or xvalue
 * - ``2463``\ 
   - | ``lossy_conversion``\ :
     | conversion may change the value
 * - ``2464``\ 
   - | ``deprecated_string_conv``\ :
     | conversion from a string literal to "char \*" is deprecated
 * - ``2465``\ 
   - | ``deprecated_string_conv_gen``\ :
     | conversion from a string literal to pointer-to-character (non-const)
       is deprecated
 * - ``2466``\ 
   - | ``override_and_final_is_cpp11``\ :
     | "override" and "final" are C++11 features
 * - ``2467``\ 
   - | ``rvalue_reference_in_exception_specification``\ :
     | rvalue references are not allowed as exception specification types
 * - ``2468``\ 
   - | ``attr_disallows_handler_param``\ :
     | attribute *"xxxx"*\  does not apply to handler parameters
 * - ``2469``\ 
   - | ``attr_requires_handler_param``\ :
     | attribute *"xxxx"*\  requires a handler parameter
 * - ``2470``\ 
   - | ``wrong_entity_for_alignas``\ :
     | alignas does not apply here
 * - ``2471``\ 
   - | ``std_alignof_with_expr_arg``\ :
     | the standard "alignof" operator does not accept an expression
       argument (use a type instead)
 * - ``2472``\ 
   - | ``bad_qualifier_for_member_enum_decl``\ :
     | invalid qualifier for *"type"*\  (a derived class is not allowed here)
 * - ``2473``\ 
   - | ``always_inline_requires_inline``\ :
     | the "always_inline" attribute is ignored on non-inline functions
 * - ``2474``\ 
   - | ``inheriting_ctor_not_from_direct_base``\ :
     | inheriting constructors must be inherited from a direct base class
 * - ``2476``\ 
   - | ``exp_asm_label``\ :
     | expected a label
 * - ``2477``\ 
   - | ``missing_label_operand_number``\ :
     | expected an operand number after "%"
 * - ``2478``\ 
   - | ``label_operand_number_out_of_range``\ :
     | operand number for "%" does not refer to a valid label argument
 * - ``2479``\ 
   - | ``wide_string_invalid_in_asm``\ :
     | a wide string is invalid in an "asm" statement
 * - ``2480``\ 
   - | ``attribute_is_nonstandard``\ :
     | attribute is nonstandard
 * - ``2481``\ 
   - | ``decltype_is_not_base_class``\ :
     | not a base class of class *"type"*\ 
 * - ``2482``\ 
   - | ``bad_user_defined_suffix``\ :
     | non-identifier character in user-defined literal suffix
 * - ``2483``\ 
   - | ``multichar_ud_lit``\ :
     | a multicharacter literal cannot be part of a user-defined literal
 * - ``2484``\ 
   - | ``ud_string_suffix_mismatch``\ :
     | user-defined literal suffix does not match the earlier *"xxxx"*\ 
 * - ``2485``\ 
   - | ``invalid_literal_operator_id``\ :
     | invalid literal operator name
 * - ``2486``\ 
   - | ``literal_operator_not_found``\ :
     | user-defined literal operator not found
 * - ``2487``\ 
   - | ``ambig_literal_operator``\ :
     | ambiguous literal operators and/or literal operator template:
 * - ``2488``\ 
   - | ``udl_cannot_be_class_member``\ :
     | a literal operator cannot be a member of a class
 * - ``2489``\ 
   - | ``extern_c_literal_operator``\ :
     | a literal operator cannot have extern "C" name linkage
 * - ``2490``\ 
   - | ``no_parameter_for_literal_operator``\ :
     | at least one parameter expected for a literal operator
 * - ``2491``\ 
   - | ``too_many_parameters_for_literal_operator``\ :
     | too many parameters for this literal operator
 * - ``2492``\ 
   - | ``invalid_parameter_type_for_literal_operator``\ :
     | invalid parameter type *"type"*\  for literal operator
 * - ``2493``\ 
   - | ``invalid_integer_parameter_for_literal_operator``\ :
     | invalid integer parameter type (*"type"*\ ) for literal operator;
       expected a character type or unsigned long long
 * - ``2494``\ 
   - | ``invalid_float_parameter_for_literal_operator``\ :
     | invalid floating-point parameter type (*"type"*\ ) for literal
       operator; expected long double
 * - ``2495``\ 
   - | ``pointer_to_nonconst_for_literal_operator``\ :
     | invalid first parameter type (*"type"*\ ) for literal operator;
       pointer to non-const type is not allowed
 * - ``2496``\ 
   - | ``invalid_second_parameter_type_for_literal_operator``\ :
     | invalid second parameter type (*"type"*\ ) for literal operator; must
       be size_t
 * - ``2497``\ 
   - | ``invalid_pointer_parameter_for_literal_operator``\ :
     | invalid pointer parameter type (*"type"*\ ) for literal operator;
       expected a pointer to a character type
 * - ``2498``\ 
   - | ``ellipsis_parameter_for_literal_operator``\ :
     | a literal operator cannot have an ellipsis parameter
 * - ``2499``\ 
   - | ``invalid_parameter_for_literal_operator_template``\ :
     | a literal operator template cannot have any parameters
 * - ``2500``\ 
   - | ``invalid_template_parameter_for_literal_operator_template``\ :
     | a literal operator template must have a template parameter list
       equivalent to "<char ...>"
 * - ``2501``\ 
   - | ``thread_local_not_allowed``\ :
     | thread-local storage class is not valid here
 * - ``2502``\ 
   - | ``thread_local_follows_non_thread_local``\ :
     | thread-local declaration follows non-thread-local declaration
       (declared at line *xxxx*\ )
 * - ``2503``\ 
   - | ``non_thread_local_follows_thread_local``\ :
     | non-thread-local declaration follows thread-local declaration
       (declared at line *xxxx*\ )
 * - ``2504``\ 
   - | ``default_arg_for_literal_operator``\ :
     | a literal operator cannot have default arguments
 * - ``2505``\ 
   - | ``attribute_ignored_for_thread_local``\ :
     | attribute is ignored for thread-local variables
 * - ``2506``\ 
   - | ``lit_suffix_no_underscore``\ :
     | a user-provided literal suffix must begin with "_"
 * - ``2507``\ 
   - | ``rvalue_references_is_cpp11``\ :
     | rvalue references are a C++11 feature
 * - ``2508``\ 
   - | ``lambdas_is_cpp11``\ :
     | lambda expressions are a C++11 feature
 * - ``2509``\ 
   - | ``std_attributes_is_cpp11``\ :
     | standard attribute syntax is a C++11 feature
 * - ``2510``\ 
   - | ``delegating_constructor_is_cpp11``\ :
     | delegating constructors are a C++11 feature
 * - ``2511``\ 
   - | ``inheriting_constructor_is_cpp11``\ :
     | inheriting constructors are a C++11 feature
 * - ``2512``\ 
   - | ``field_initializers_is_cpp11``\ :
     | field initializers are a C++11 feature
 * - ``2513``\ 
   - | ``deleted_functions_is_cpp11``\ :
     | deleted functions are a C++11 feature
 * - ``2514``\ 
   - | ``defaulted_functions_is_cpp11``\ :
     | defaulted functions are a C++11 feature
 * - ``2515``\ 
   - | ``storage_class_not_allowed_in_specialization``\ :
     | a storage class is not allowed in an explicit specialization
 * - ``2517``\ 
   - | ``specialization_of_unscoped_enum``\ :
     | an unscoped enumeration must be opaque in order to be specialized
 * - ``2518``\ 
   - | ``nonmember_enum_template``\ :
     | an enumeration template declaration must refer to a previously
       declared member of a class template
 * - ``2519``\ 
   - | ``operand_must_be_vector``\ :
     | expected a vector operand
 * - ``2520``\ 
   - | ``incompatible_shuffle_source_operands``\ :
     | shuffle source operands have incompatible types *"type"*\  and
       *"type"*\ 
 * - ``2521``\ 
   - | ``nonintegral_shuffle_mask``\ :
     | shuffle mask (type *"type"*\ ) is not a vector of integers
 * - ``2522``\ 
   - | ``incompatible_shuffle_mask``\ :
     | shuffle mask (type *"type"*\ ) has a length different from the source
       operand (type *"type"*\ )
 * - ``2523``\ 
   - | ``bad_size_for_static_address_init``\ :
     | static initialization with an address value requires a destination of
       the same size as the address
 * - ``2524``\ 
   - | ``feature_test_macro_req_id``\ :
     | the argument to a feature-test macro must be a simple identifier
 * - ``2525``\ 
   - | ``has_include_next_in_primary_source_file``\ :
     | __has_include_next cannot be used in the primary source file
 * - ``2526``\ 
   - | ``absolute_file_name_in_has_include_next``\ :
     | absolute file name used in \__has_include_next
 * - ``2527``\ 
   - | ``attr_not_applied_to_function_type``\ :
     | attribute *"xxxx"*\  must be applied to a function type
 * - ``2529``\ 
   - | ``bad_c11_noreturn``\ :
     | _Noreturn is not allowed here
 * - ``2530``\ 
   - | ``operand_must_be_real_floating_value``\ :
     | expected an operand of real floating-point type (not *"type"*\ )
 * - ``2531``\ 
   - | ``incompatible_builtin_complex_types``\ :
     | __builtin_complex requires operands of compatible types (unlike
       *"type"*\  and *"type"*\ )
 * - ``2532``\ 
   - | ``default_association_appears_more_than_once``\ :
     | a default association already appeared in this _Generic selection
 * - ``2533``\ 
   - | ``variably_modified_type_not_allowed_here``\ :
     | a type involving a variable length array is not allowed here
 * - ``2534``\ 
   - | ``duplicate_type_in_c11_generic``\ :
     | duplicate association type (*"type"*\ ) in _Generic selection
 * - ``2535``\ 
   - | ``no_match_in_c11_generic``\ :
     | no association matches the selector type *"type"*\ 
 * - ``2536``\ 
   - | ``incompatible_ifunc_resolver_type``\ :
     | the type of *entity-kind "entity"*\  (*"type"*\ ) is incompatible
       with an ifunc resolver type
 * - ``2537``\ 
   - | ``ifunc_cant_be_alias``\ :
     | a function cannot have both ifunc and alias attributes
 * - ``2538``\ 
   - | ``ifunc_cant_be_weak``\ :
     | a function cannot have both ifunc and weak attributes
 * - ``2539``\ 
   - | ``call_requires_string_literal``\ :
     | call requires a string literal operand
 * - ``2540``\ 
   - | ``duplicate_inheriting_constructor``\ :
     | duplicate inheriting constructor declaration (previous at line
       *xxxx*\ )
 * - ``2541``\ 
   - | ``modified_decltype_auto_type``\ :
     | "decltype(auto)" must be a placeholder for the complete type of the
       variable (not for a component of that type)
 * - ``2542``\ 
   - | ``decltype_auto_not_allowed_here``\ :
     | decltype(auto) is not allowed here
 * - ``2543``\ 
   - | ``decltype_auto_type_requires_initializer``\ :
     | cannot deduce "decltype(auto)" (initializer required)
 * - ``2544``\ 
   - | ``cannot_deduce_decltype_auto_type``\ :
     | cannot deduce "decltype(auto)" type
 * - ``2545``\ 
   - | ``thread_local_must_include_static_or_extern``\ :
     | a block-scope thread-local declaration must include static or extern
 * - ``2546``\ 
   - | ``deduced_return_type_conflict``\ :
     | deduced return type *"type"*\  conflicts with previously deduced type
       *"type"*\ 
 * - ``2547``\ 
   - | ``use_of_undefined_function_with_deduced_return_type``\ :
     | cannot deduce the return type of *entity-kind "entity"*\  (declared
       at line *xxxx*\ ); it has not been defined
 * - ``2548``\ 
   - | ``virtual_function_cannot_have_deduced_return_type``\ :
     | a virtual function cannot have a deduced return type
 * - ``2549``\ 
   - | ``keyword_dropped``\ :
     | *entity-kind "entity"*\  will be treated as a context-sensitive
       keyword from this point
 * - ``2550``\ 
   - | ``global_ns_has_no_actual_member``\ :
     | the global namespace has no actual member *"xxxx"*\ 
 * - ``2551``\ 
   - | ``different_enum_comparison``\ :
     | comparison between two different enum types (*"type"*\  and
       *"type"*\ )
 * - ``2552``\ 
   - | ``unrecognized_target_attribute``\ :
     | target attribute not recognized
 * - ``2553``\ 
   - | ``gnu_mv_default_missing``\ :
     | missing "default" target function
 * - ``2554``\ 
   - | ``gnu_mv_only_one_arch``\ :
     | only one arch= target may be specified
 * - ``2555``\ 
   - | ``generic_class_cannot_be_custom_attribute``\ :
     | a generic class cannot be a custom attribute
 * - ``2556``\ 
   - | ``invalid_ms_attribute_target``\ :
     | invalid attribute target *"xxxx"*\ 
 * - ``2557``\ 
   - | ``ambiguous_ms_attribute``\ :
     | ambiguous attribute -- both *"type"*\  and *"type"*\  could be used
 * - ``2558``\ 
   - | ``cli_attribute_inaccessible_field``\ :
     | a named attribute argument can only reference a public nonstatic
       read/write field or scalar property
 * - ``2559``\ 
   - | ``cli_attribute_invalid_field``\ :
     | a named attribute argument can only reference a nonstatic field or
       scalar property of an attribute parameter type
 * - ``2560``\ 
   - | ``cli_attribute_invalid_argument``\ :
     | invalid attribute argument -- expression must be a constant of an
       attribute parameter type
 * - ``2561``\ 
   - | ``cli_typeid_of_generic_param_in_attribute``\ :
     | an attribute argument cannot use generic type parameters
 * - ``2562``\ 
   - | ``invalid_use_of_standalone_custom_ms_attr``\ :
     | *"type"*\  may only be used as a standalone attribute
 * - ``2563``\ 
   - | ``field_target_on_nontrivial_property_or_event``\ :
     | the 'field' attribute target cannot be used on a non-trivial
       property/event
 * - ``2564``\ 
   - | ``invalid_attribute_target_for_standalone_ms_attr``\ :
     | invalid attribute target for a standalone attribute
 * - ``2565``\ 
   - | ``invalid_attribute_target_for_ms_attr``\ :
     | invalid attribute target for this context
 * - ``2566``\ 
   - | ``invalid_use_of_custom_ms_attr``\ :
     | *"type"*\  attribute cannot be used here
 * - ``2567``\ 
   - | ``cli_param_array_attribute_deprecated``\ :
     | *"type"*\  is deprecated; use '...' to specify a parameter array
 * - ``2568``\ 
   - | ``namespace_default_cannot_be_extended``\ :
     | the default namespace cannot be extended
 * - ``2569``\ 
   - | ``cppcx_box_invalid_type``\ :
     | the boxed type must be a value class or enum
 * - ``2570``\ 
   - | ``tracking_reference_to_value_class``\ :
     | tracking reference to value class is not allowed
 * - ``2571``\ 
   - | ``handle_to_value_class``\ :
     | handle to value class is not allowed
 * - ``2572``\ 
   - | ``tracking_reference_to_enum``\ :
     | tracking reference to enum is not allowed
 * - ``2573``\ 
   - | ``handle_to_enum``\ :
     | handle to enum is not allowed
 * - ``2574``\ 
   - | ``cppcx_public_native_type``\ :
     | a public native type is not allowed
 * - ``2575``\ 
   - | ``public_nested_type_in_cppcx_type``\ :
     | a public nested type is not allowed
 * - ``2576``\ 
   - | ``cppcx_generic_type_not_allowed``\ :
     | generic types are not permitted in C++/CX
 * - ``2577``\ 
   - | ``cppcx_generic_method_not_allowed``\ :
     | generic methods are not permitted in C++/CX
 * - ``2578``\ 
   - | ``cppcx_generic_constraints_not_allowed``\ :
     | generic constraints are not allowed
 * - ``2579``\ 
   - | ``nonpublic_data_member_in_public_cppcx_value_type``\ :
     | non-public data members are not allowed in public C++/CX value types
 * - ``2580``\ 
   - | ``public_nondata_member_in_public_cppcx_value_type``\ :
     | public non-data members are not allowed in public C++/CX value types
 * - ``2581``\ 
   - | ``cppcx_public_value_class_constructor``\ :
     | constructors are not allowed in public C++/CX value types
 * - ``2582``\ 
   - | ``bad_cppcx_event_add_return``\ :
     | the return type of the "add" accessor must be
       Windows::Foundation::EventRegistrationToken
 * - ``2583``\ 
   - | ``bad_cppcx_event_remove_return``\ :
     | the return type of the "remove" accessor must be void
 * - ``2584``\ 
   - | ``bad_cppcx_event_remove_parameter``\ :
     | the parameter type of the "remove" accessor must be
       Windows::Foundation::EventRegistrationToken
 * - ``2585``\ 
   - | ``cppcx_handle_or_ref_to_generic_param``\ :
     | a handle or reference to a generic parameter type is not allowed
 * - ``2586``\ 
   - | ``public_data_member_in_public_non_value_type``\ :
     | public data members are not allowed in non-value types
 * - ``2587``\ 
   - | ``cl_cppcx_only_in_microsoft_cplusplus``\ :
     | C++/CX can be enabled only in Microsoft C++ mode
 * - ``2588``\ 
   - | ``cl_cppcx_and_cppcli``\ :
     | C++/CLI and C++/CX modes cannot be combined
 * - ``2589``\ 
   - | ``cppcx_not_enabled``\ :
     | *"xxxx"*\  requires C++/CX mode
 * - ``2590``\ 
   - | ``cl_microsoft_version_insufficient_for_cppcx``\ :
     | C++/CX mode requires microsoft_version >= 1600
 * - ``2591``\ 
   - | ``literal_fields_disallowed_in_cppcx_mode``\ :
     | Literal fields are not allowed in C++/CX
 * - ``2592``\ 
   - | ``normal_ref_bound_to_cppcx_lvalue``\ :
     | a standard reference cannot be bound to a C++/CX type
 * - ``2593``\ 
   - | ``cppcx_enum_base_has_no_platform_counterpart``\ :
     | type must correspond to Platform::Boolean, default::uint8,
       default::int8, default::int16, default::uint16, default::int32,
       default::uint32, default::int64, or default::uint64
 * - ``2594``\ 
   - | ``event_in_cppcx_value_type``\ :
     | a C++/CX value type cannot have events
 * - ``2595``\ 
   - | ``bad_cppcx_dynamic_cast_type``\ :
     | a dynamic_cast to a handle type must refer to a complete class type
 * - ``2596``\ 
   - | ``cppcx_array_only_one_dimension_allowed``\ :
     | Platform::Array can only be one-dimensional
 * - ``2597``\ 
   - | ``cppcx_tracking_reference_on_standard_class_type``\ :
     | tracking reference to standard class type is not allowed
 * - ``2598``\ 
   - | ``cppcx_value_type_deriving_from_interface``\ :
     | a C++/CX value type cannot inherit from an interface
 * - ``2599``\ 
   - | ``cppcx_value_type_contains_virtual_function``\ :
     | a C++/CX value type cannot contain virtual functions
 * - ``2600``\ 
   - | ``partial_class_incorrect_type_or_location``\ :
     | "partial" can only be applied to "ref class" or "ref struct" at
       global scope or namespace scope
 * - ``2601``\ 
   - | ``cppcx_invalid_array_property_set_value_parameter``\ :
     | the parameter of the "set" accessor must be of type "const
       Platform::Array<T>^"
 * - ``2602``\ 
   - | ``cppcx_public_global_type``\ :
     | the definition of a public C++/CX type is not allowed at global scope
 * - ``2603``\ 
   - | ``cppcx_public_indexed_property``\ :
     | an indexed property with a public "get" or "set" accessor is not
       allowed
 * - ``2604``\ 
   - | ``cppcx_public_nested_delegate``\ :
     | a public nested delegate type is not allowed
 * - ``2605``\ 
   - | ``cppcx_bad_delegate_init_list``\ :
     | invalid delegate initializer -- expected either "(<function-address
       or functor-object> [, Platform::CallbackContext])" or "(<object
       handle>, <member-address> [, Platform::CallbackContext [, bool]])"
 * - ``2606``\ 
   - | ``cppcx_invalid_delegate_object``\ :
     | invalid delegate initializer -- object must be a handle to a managed
       class
 * - ``2607``\ 
   - | ``cppcx_non_const_array_parameter``\ :
     | C++/CX does not support 'in/out' arrays -- use "const
       Platform::Array<T>^" for 'in' and "Platform::WriteOnlyArray<T>^" or
       "Platform::Array<T>^\*" for 'out' on public APIs
 * - ``2608``\ 
   - | ``missing_target_attribute``\ :
     | missing "target" attribute for *entity-kind "entity"*\  (declared at
       line *xxxx*\ )
 * - ``2609``\ 
   - | ``no_matching_target_attribute``\ :
     | no declared member function matches "target" attributes for
       *entity-kind "entity"*\ 
 * - ``2610``\ 
   - | ``invalid_base_for_ms_attributes``\ :
     | Microsoft attributes in this location are only permitted for
       interface types
 * - ``2611``\ 
   - | ``resolver_routine_required``\ :
     | GNU function multiversion resolver routine required
 * - ``2612``\ 
   - | ``enum_in_managed_class_missing_definition``\ :
     | an enum type declared in a managed class must include a definition
 * - ``2613``\ 
   - | ``decltype_qualified_declared_name``\ :
     | a decltype-qualified name is nonstandard in this declaration context
 * - ``2614``\ 
   - | ``final_modifier_requires_virtual_function``\ :
     | nonvirtual function cannot be declared with "final" modifier
 * - ``2615``\ 
   - | ``target_on_special_function``\ :
     | "target" attribute on special function is not supported
 * - ``2616``\ 
   - | ``target_string_must_be_narrow``\ :
     | must be a narrow string literal
 * - ``2617``\ 
   - | ``target_unmatched_parens``\ :
     | unmatched parentheses
 * - ``2618``\ 
   - | ``gcc_pragma_nothing_to_pop``\ :
     | no corresponding "push_options"
 * - ``2619``\ 
   - | ``pragma_inside_function``\ :
     | this pragma is not allowed inside a function
 * - ``2620``\ 
   - | ``inline_new_or_delete_operator``\ :
     | declaring a new or delete operator "inline" is nonstandard
 * - ``2621``\ 
   - | ``pack_expansion_for_field_mem_init``\ :
     | a mem-initializer for a data member cannot be a pack expansion
 * - ``2622``\ 
   - | ``generic_lambda_cannot_capture``\ :
     | generic lambda expressions cannot have capture defaults in this mode
 * - ``2623``\ 
   - | ``friend_with_def_arg_must_be_definition``\ :
     | a default template argument in a friend declaration may only be
       specified in a definition
 * - ``2624``\ 
   - | ``friend_with_def_arg_must_be_only_decl``\ :
     | a friend template declaration with a default template argument must
       be the only declaration (first declared at line *xxxx*\ )
 * - ``2625``\ 
   - | ``non_autonomous_opaque_enum_decl``\ :
     | an opaque enum declaration cannot be part of another declaration
 * - ``2626``\ 
   - | ``nonstd_opaque_enum_decl_in_type_id``\ :
     | an opaque enum declaration is nonstandard in this context
 * - ``2627``\ 
   - | ``extended_friends_is_cpp11``\ :
     | extended friend syntax is a C++11 feature
 * - ``2628``\ 
   - | ``digit_separators_not_enabled``\ :
     | digit separators not enabled, apostrophe begins a character literal
 * - ``2629``\ 
   - | ``bad_digit_separator_pos``\ :
     | digit separator cannot appear here
 * - ``2630``\ 
   - | ``constexpr_ignored_on_microsoft_nonstatic_member``\ :
     | "constexpr" is ignored here in Microsoft mode
 * - ``2631``\ 
   - | ``bad_gnu_stmt_return``\ :
     | invalid expression for statement expression result
 * - ``2632``\ 
   - | ``macro_not_udl_suffix``\ :
     | identifier is a macro and not a literal suffix
 * - ``2633``\ 
   - | ``cannot_call_named_member_on_lvalue``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) cannot be called
       on an lvalue
 * - ``2634``\ 
   - | ``cannot_call_named_member_on_rvalue``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) cannot be called
       on an rvalue
 * - ``2635``\ 
   - | ``cannot_call_member_on_lvalue``\ :
     | member function cannot be called on an lvalue
 * - ``2636``\ 
   - | ``cannot_call_member_on_rvalue``\ :
     | member function cannot be called on an rvalue
 * - ``2637``\ 
   - | ``templ_param_list_too_long``\ :
     | the template parameter list is too long
 * - ``2638``\ 
   - | ``bad_alias_templ_redecl``\ :
     | alias template type *"type"*\  is incompatible with the previous type
       of *"type"*\  in the redeclaration of *entity-kind "entity"*\ 
       (declared at line *xxxx*\ )
 * - ``2639``\ 
   - | ``field_initializer_is_not_constant``\ :
     | the field initializer for *entity-kind "entity"*\  (declared at line
       *xxxx*\ ) is not a constant expression
 * - ``2640``\ 
   - | ``constraint_number_mismatch``\ :
     | the number of operand constraints must be the same in each constraint
       string
 * - ``2641``\ 
   - | ``too_many_constraints``\ :
     | the constraint string contains too many alternative constraints; not
       all constraints were checked
 * - ``2643``\ 
   - | ``decltype_auto_cannot_be_qualified``\ :
     | decltype(auto) cannot have added type qualifiers
 * - ``2644``\ 
   - | ``bad_init_capture_capture``\ :
     | init-capture *"entity"*\  (declared at line *xxxx*\ ) cannot be
       captured here
 * - ``2645``\ 
   - | ``invalid_nontype_template_argument``\ :
     | invalid nontype template argument of type *"type"*\ 
 * - ``2646``\ 
   - | ``abi_tag_ignored_in_C_mode``\ :
     | the abi_tag attribute is ignored (it has no meaning in C mode)
 * - ``2647``\ 
   - | ``abi_tag_redefinition``\ :
     | redeclaration adds abi_tag attribute "*xxxx*\ "
 * - ``2648``\ 
   - | ``abi_tag_ignored``\ :
     | abi_tag attribute is ignored (superseded by later abi_tag attribute)
 * - ``2649``\ 
   - | ``no_abi_tag_on_declaration``\ :
     | previous declaration of *entity-kind "entity"*\  (declared at line
       *xxxx*\ ) had no abi_tag attribute
 * - ``2651``\ 
   - | ``abi_tag_ignored_on_specialization``\ :
     | abi_tag attribute is ignored on specialization
 * - ``2652``\ 
   - | ``decltype_auto_return_must_be_standalone``\ :
     | decltype(auto) cannot appear under a pointer, reference, or
       pointer-to-member construct
 * - ``2653``\ 
   - | ``exp_class_or_typename``\ :
     | expected "class" or "typename"
 * - ``2654``\ 
   - | ``placement_new_refers_to_non_placement_delete``\ :
     | placement "new" expression refers to non-placement *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``2655``\ 
   - | ``cl_must_specify_cpp14_mode``\ :
     | must specify C++14 mode when building runtime library
 * - ``2659``\ 
   - | ``constexpr_not_const``\ :
     | constexpr non-static member function will not be implicitly 'const'
       in C++14
 * - ``2660``\ 
   - | ``nonliteral_var_in_constexpr_function``\ :
     | variable type *"type"*\  in constexpr function is not a literal type
 * - ``2661``\ 
   - | ``nonautomatic_var_in_constexpr_function``\ :
     | variable in constexpr function does not have automatic storage
       duration
 * - ``2662``\ 
   - | ``uninitialized_var_in_constexpr_function``\ :
     | variable in constexpr function is uninitialized
 * - ``2663``\ 
   - | ``auto_direct_list_init_requires_singleton``\ :
     | braced initialization of a variable declared with a placeholder type
       but without "=" requires exactly one element inside the braces
 * - ``2664``\ 
   - | ``cl_invalid_target``\ :
     | no "*xxxx*\ " --target configuration exists
 * - ``2665``\ 
   - | ``attribute_not_supported_in_x86_64``\ :
     | attribute is supported only in 32-bit x86 configurations
 * - ``2666``\ 
   - | ``cl_missing_argument``\ :
     | "*xxxx*\ " requires an argument
 * - ``2667``\ 
   - | ``special_member_coroutine``\ :
     | a constructor or destructor cannot be a coroutine
 * - ``2668``\ 
   - | ``main_coroutine``\ :
     | *entity-kind "entity"*\  cannot be a coroutine
 * - ``2669``\ 
   - | ``yield_in_catch``\ :
     | co_yield expressions are not permitted in a catch clause
 * - ``2674``\ 
   - | ``special_class_template_not_found``\ :
     | class template *"xxxx"*\  not found
 * - ``2675``\ 
   - | ``typename_needed``\ :
     | use the "typename" keyword to treat *entity-kind "entity"*\  as a
       type in a dependent context
 * - ``2676``\ 
   - | ``shufflevector_index_out_of_range``\ :
     | argument value must be less than the sum of the vector elements
 * - ``2677``\ 
   - | ``not_a_type_member``\ :
     | *"type"*\  has no member *"xxxx"*\ 
 * - ``2678``\ 
   - | ``braced_list_for_implicit_return_type``\ :
     | a brace-enclosed list does not provide a return type
 * - ``2679``\ 
   - | ``await_not_allowed_outside_function_scope``\ :
     | a co_await expression must appear in a function scope
 * - ``2680``\ 
   - | ``await_not_allowed_in_catch_clause``\ :
     | a co_await expression is not allowed inside a catch clause
 * - ``2681``\ 
   - | ``coroutine_with_ellipsis_parameter``\ :
     | a coroutine cannot have an ellipsis parameter
 * - ``2682``\ 
   - | ``cl_relaxed_constexpr_requires_bool``\ :
     | enabling C++14-style constexpr requires support for "bool"
 * - ``2683``\ 
   - | ``constexpr_function_undefined``\ :
     | constexpr *entity-kind "entity"*\  (declared at line *xxxx*\ ) is not
       defined
 * - ``2684``\ 
   - | ``constexpr_call_not_interpretable``\ :
     | this call cannot be evaluated because the target function
       *entity-kind "entity"*\  (declared at line *xxxx*\ ) is not constexpr
       or not completely defined yet
 * - ``2687``\ 
   - | ``anon_union_alias_member_template``\ :
     | invalid anonymous union -- alias member template is not allowed
 * - ``2688``\ 
   - | ``utf8_char_lit_too_long``\ :
     | a UTF-8 character literal value cannot occupy more than one code unit
 * - ``2689``\ 
   - | ``variable_not_constant_valued``\ :
     | the value of *entity-kind "entity"*\  (declared at line *xxxx*\ )
       cannot be used as a constant
 * - ``2690``\ 
   - | ``variable_not_constant_addressed``\ :
     | a pointer or reference to *entity-kind "entity"*\  (declared at line
       *xxxx*\ ) cannot be used as a constant
 * - ``2691``\ 
   - | ``constexpr_non_array_subscript``\ :
     | nonzero subscript for non-array object
 * - ``2692``\ 
   - | ``constexpr_out_of_bounds_array_access``\ :
     | cannot access position *n*\  in array of *n*\  elements
 * - ``2693``\ 
   - | ``constexpr_called_from``\ :
     | called from:
 * - ``2694``\ 
   - | ``constexpr_union_field_inactive``\ :
     | invalid access to inactive *entity-kind "entity"*\  of union
       (*entity-kind "entity"*\  is active)
 * - ``2695``\ 
   - | ``constexpr_goto``\ :
     | 'goto' cannot be executed in constexpr contexts
 * - ``2696``\ 
   - | ``constexpr_missing_return_value``\ :
     | missing return value
 * - ``2697``\ 
   - | ``constexpr_null_callee``\ :
     | callee is null
 * - ``2698``\ 
   - | ``constexpr_null_dereference``\ :
     | attempt to dereference a null pointer
 * - ``2699``\ 
   - | ``constexpr_access_one_past_array_end``\ :
     | attempt to access storage one position past the end of an array of
       *n*\  elements
 * - ``2700``\ 
   - | ``constexpr_access_to_expired_storage``\ :
     | attempt to access expired storage
 * - ``2701``\ 
   - | ``constexpr_access_to_runtime_storage``\ :
     | attempt to access run-time storage
 * - ``2703``\ 
   - | ``constexpr_call_to_nonconstexpr_function``\ :
     | cannot call non-constexpr *entity-kind "entity"*\  (declared at line
       *xxxx*\ )
 * - ``2704``\ 
   - | ``constexpr_vla``\ :
     | cannot use variable-length array during constexpr evaluation
 * - ``2705``\ 
   - | ``constexpr_negative_shift``\ :
     | cannot perform a negative shift
 * - ``2706``\ 
   - | ``constexpr_shift_excess``\ :
     | shift amount (*n*\ ) too large
 * - ``2707``\ 
   - | ``constexpr_integer_overflow``\ :
     | value exceeds range of *"type"*\ 
 * - ``2708``\ 
   - | ``constexpr_fp_error``\ :
     | floating-point error
 * - ``2709``\ 
   - | ``constexpr_null_ptr_to_member_data``\ :
     | attempt to dereference a null pointer-to-member (data member)
 * - ``2710``\ 
   - | ``comparison_of_pointers_to_void_and_function``\ :
     | comparing a pointer to void and a pointer to a function is nonstandard
 * - ``2711``\ 
   - | ``ms_metadata_init_failed``\ :
     | metadata initialization failed
 * - ``2712``\ 
   - | ``constexpr_bad_derived_class_cast``\ :
     | invalid base-to-derived cast (actual derived class type is *"type"*\ )
 * - ``2713``\ 
   - | ``constexpr_invalid_pm_access``\ :
     | invalid access to *entity-kind "entity"*\  in object of complete type
       *"type"*\ 
 * - ``2714``\ 
   - | ``bad_gnu_auto_type``\ :
     | *"xxxx"*\  not allowed here
 * - ``2715``\ 
   - | ``gnu_auto_type_with_secondary_declarator``\ :
     | *"xxxx"*\  does not permit multiple declarators
 * - ``2716``\ 
   - | ``auto_type_brace_initialization_not_allowed``\ :
     | initialization with "{...}" is not allowed for *"xxxx"*\ 
 * - ``2717``\ 
   - | ``modified_auto_type``\ :
     | *"xxxx"*\  must be a placeholder for the complete type of the
       variable (not for a component of that type)
 * - ``2718``\ 
   - | ``gnu_auto_type_without_initializer``\ :
     | a variable declared with *"xxxx"*\  requires an initializer
 * - ``2719``\ 
   - | ``constant_must_be_positive``\ :
     | integer constant must be greater than or equal to zero
 * - ``2720``\ 
   - | ``type_must_be_integral``\ :
     | type must be an integral type
 * - ``2721``\ 
   - | ``constexpr_expression_cannot_be_interpreted``\ :
     | expression cannot be interpreted
 * - ``2722``\ 
   - | ``constexpr_statement_cannot_be_interpreted``\ :
     | statement cannot be interpreted
 * - ``2723``\ 
   - | ``constexpr_interpreter_address``\ :
     | invalid use of address of interpreter storage
 * - ``2724``\ 
   - | ``constexpr_invalid_constant_kind``\ :
     | invalid constant kind for constant-expression
 * - ``2725``\ 
   - | ``constexpr_type_too_large``\ :
     | type *"type"*\  too large for constant-expression evaluation
 * - ``2726``\ 
   - | ``constexpr_type_invalid``\ :
     | invalid type *"type"*\  for constant-expression evaluation
 * - ``2727``\ 
   - | ``constexpr_invalid_type_conversion``\ :
     | conversion from *"type"*\  to *"type"*\  is invalid in
       constant-expression evaluation
 * - ``2728``\ 
   - | ``constexpr_fp_conversion_failed``\ :
     | floating-point conversion failed
 * - ``2730``\ 
   - | ``deduced_return_types_is_cpp14``\ :
     | deduced return types are a C++14 feature
 * - ``2731``\ 
   - | ``constexpr_ctor_with_dtor``\ :
     | cannot evaluate a constructor with an associated destructor
 * - ``2732``\ 
   - | ``constexpr_missing_initializer_for_field``\ :
     | *entity-kind "entity"*\  not initialized during constexpr evaluation
 * - ``2733``\ 
   - | ``constexpr_invalid_pdiff``\ :
     | invalid pointer difference in constexpr evaluation
 * - ``2734``\ 
   - | ``constexpr_non_array_pointer_arithmetic``\ :
     | invalid arithmetic on non-array pointer
 * - ``2735``\ 
   - | ``constexpr_pointer_ahead_of_array``\ :
     | cannot set pointer before the first array element
 * - ``2736``\ 
   - | ``coroutine_with_deduced_return_type``\ :
     | a coroutine with a deduced return type is invalid
 * - ``2737``\ 
   - | ``await_in_unevaluated_operand``\ :
     | expression not allowed in unevaluated context
 * - ``2740``\ 
   - | ``return_in_coroutine``\ :
     | "return" is not permitted in a coroutine (use "co_return" instead)
 * - ``2741``\ 
   - | ``invalid_co_return``\ :
     | "co_return" is only allowed in coroutines
 * - ``2742``\ 
   - | ``constexpr_fp_values_not_comparable``\ :
     | floating-point values cannot be compared
 * - ``2743``\ 
   - | ``constexpr_pointers_not_comparable``\ :
     | pointer values cannot be compared because they do not point into the
       same complete object or they point to subobjects with different
       accessibility
 * - ``2744``\ 
   - | ``ignoring_attribute_on_non_inline_namespace``\ :
     | ignoring abi_tag attribute on non-inline namespace
 * - ``2745``\ 
   - | ``ignoring_attribute_on_anonymous_namespace``\ :
     | ignoring abi_tag attribute on anonymous namespace
 * - ``2746``\ 
   - | ``complex_template_parameter``\ :
     | complex or imaginary template parameter type is nonstandard
 * - ``2747``\ 
   - | ``yield_outside_of_function``\ :
     | co_yield expression is not permitted outside a function scope
 * - ``2748``\ 
   - | ``thread_local_ignored``\ :
     | ignoring thread-local indication on anonymous union
 * - ``2751``\ 
   - | ``object_not_initialized``\ :
     | access to uninitialized object
 * - ``2752``\ 
   - | ``constexpr_volatile_fetch``\ :
     | attempt to read from volatile storage
 * - ``2753``\ 
   - | ``constexpr_no_active_union_field``\ :
     | invalid access to inactive *entity-kind "entity"*\  of union (no
       field is active)
 * - ``2754``\ 
   - | ``label_in_constexpr_function``\ :
     | label definitions cannot appear in constexpr functions
 * - ``2755``\ 
   - | ``constexpr_equality_past_the_end_address``\ :
     | cannot compare a pointer past the end of an array with a pointer to
       another complete object
 * - ``2756``\ 
   - | ``variable_templ_function_type``\ :
     | function type *"type"*\  is an invalid type for a variable template
       instantiation
 * - ``2757``\ 
   - | ``incomplete_var_type``\ :
     | variable cannot have incomplete type *"type"*\ 
 * - ``2758``\ 
   - | ``field_subobject_not_initialized``\ :
     | access to uninitialized subobject (*entity-kind "entity"*\ )
 * - ``2759``\ 
   - | ``base_subobject_not_initialized``\ :
     | access to uninitialized subobject (base class *"type"*\ )
 * - ``2760``\ 
   - | ``constexpr_vacuous_dtor_call``\ :
     | a pseudo-destructor call is not permitted in a constant-expression
 * - ``2761``\ 
   - | ``constexpr_modifying_const_storage``\ :
     | attempt to modify const storage
 * - ``2764``\ 
   - | ``constexpr_access_past_object``\ :
     | attempt to access storage one position past an object treated as an
       array of one element
 * - ``2765``\ 
   - | ``constexpr_reinterpret_cast``\ :
     | cannot use reinterpret_cast in constant-expression evaluation
 * - ``2766``\ 
   - | ``constexpr_invalid_null_ptr_operation``\ :
     | operation not allowed on null pointer
 * - ``2767``\ 
   - | ``star_this_not_constant_valued``\ :
     | the value of \*this cannot be used as a constant
 * - ``2768``\ 
   - | ``inline_on_nested_namespace``\ :
     | the "inline" keyword cannot be used on a nested namespace declaration
 * - ``2769``\ 
   - | ``carries_dependency_ignored``\ :
     | the "carries_dependency" attribute is ignored
 * - ``2770``\ 
   - | ``event_interface_cannot_have_definition``\ :
     | an "\__event \__interface" cannot have a definition here
 * - ``2771``\ 
   - | ``invalid_event_handler_type``\ :
     | an event handler must have a void or integral return type
 * - ``2772``\ 
   - | ``event_interface_must_be_previously_defined``\ :
     | an "\__event \__interface" must have been previously defined
 * - ``2773``\ 
   - | ``too_many_template_arguments``\ :
     | too many template arguments for *entity-kind "entity"*\ 
 * - ``2774``\ 
   - | ``enumerator_already_declared``\ :
     | enumerator already declared (see *entity-kind "entity"*\  (declared
       at line *xxxx*\ ))
 * - ``2775``\ 
   - | ``microsoft_version_doesnt_support_cpp14_mode``\ :
     | the version of Microsoft being emulated must be at least 1903 to use
       "--ms_c++14"
 * - ``2776``\ 
   - | ``microsoft_version_doesnt_support_cpplatest_mode``\ :
     | the version of Microsoft being emulated must be at least 1903 to use
       "--ms_c++latest"
 * - ``2777``\ 
   - | ``c11_atomic_array_or_function_type``\ :
     | type *"type"*\  cannot be _Atomic because it is an array or function
       type
 * - ``2778``\ 
   - | ``c11_atomic_specifier_with_qualified_type``\ :
     | the _Atomic(...) specifier cannot be applied to qualified type
       *"type"*\ 
 * - ``2779``\ 
   - | ``access_to_member_of_c11_atomic_object``\ :
     | access to member of _Atomic object
 * - ``2780``\ 
   - | ``c11_atomic_bit_field``\ :
     | a bit field cannot have an _Atomic type
 * - ``2782``\ 
   - | ``nonconstexpr_mem_init_ctor_for_constexpr_ctor``\ :
     | constexpr constructor calls non-constexpr constructor for subobject
       initialization
 * - ``2783``\ 
   - | ``terse_static_assert_not_enabled``\ :
     | expected a comma (the one-argument version of static_assert is not
       enabled in this mode)
 * - ``2784``\ 
   - | ``terse_static_assert``\ :
     | static assertion failed
 * - ``2785``\ 
   - | ``conflicting_nullability``\ :
     | at most one of the qualifiers _Nullable, _Nonnull, and
       _Null_unspecified can modify a type
 * - ``2786``\ 
   - | ``invalid_type_for_nullability``\ :
     | nullability qualifiers are only permitted on pointer and
       pointer-to-member types
 * - ``2787``\ 
   - | ``vector_length_too_large``\ :
     | vector length is too large
 * - ``2788``\ 
   - | ``invalid_vector_element_type``\ :
     | vector element type must be integral, enum, or real floating-point
       type
 * - ``2789``\ 
   - | ``builtin_needs_128_bit_integers``\ :
     | builtin function is not available because 128-bit integers are not
       supported
 * - ``2790``\ 
   - | ``builtin_needs_vector_types``\ :
     | builtin function is not available because vector types are not
       supported
 * - ``2791``\ 
   - | ``must_introduce_attribute``\ :
     | two consecutive left square brackets always introduce an attribute
       list but an attribute list cannot appear here
 * - ``2792``\ 
   - | ``invalid_target_attribute``\ :
     | unrecognized "target" attribute disqualifies this routine from being
       used by resolver routine
 * - ``2793``\ 
   - | ``vector_type_required``\ :
     | *"type"*\  is not a vector type
 * - ``2794``\ 
   - | ``vector_types_differ_in_length``\ :
     | vector types *"type"*\  and *"type"*\  must have the same length
 * - ``2795``\ 
   - | ``member_special_after_class_definition``\ :
     | added default arguments cannot result in declaring a default or copy
       constructor
 * - ``2796``\ 
   - | ``template_arg_cannot_point_to_subobject``\ :
     | a nontype template argument of reference type must bind to a function
       or to a complete object
 * - ``2797``\ 
   - | ``type_not_allowed_here``\ :
     | *"type"*\  not allowed here
 * - ``2798``\ 
   - | ``register_keyword_disallowed``\ :
     | use of the "register" storage class specifier is not allowed
 * - ``2799``\ 
   - | ``register_keyword_deprecated``\ :
     | use of the "register" storage class specifier is deprecated
 * - ``2800``\ 
   - | ``incr_of_bool_not_allowed``\ :
     | incrementing a bool value is not allowed
 * - ``2801``\ 
   - | ``redeclaration_of_range_iterator``\ :
     | *"xxxx"*\ , declared as iterator of range-based "for" statement, may
       not be redeclared in this scope
 * - ``2802``\ 
   - | ``namespace_not_allowed``\ :
     | an attribute namespace may not be used here (because a "using" prefix
       was specified)
 * - ``2803``\ 
   - | ``attribute_namespace_unrecognized``\ :
     | attribute namespace *"xxxx"*\  is unrecognized
 * - ``2804``\ 
   - | ``default_member_init_for_value_class``\ :
     | a default member initializer is not permitted in a value class
 * - ``2805``\ 
   - | ``cl_implicit_noexcept_requires_noexcept_support``\ :
     | "--implicit_noexcept" requires a mode that enables noexcept
 * - ``2806``\ 
   - | ``constexpr_virtual_base``\ :
     | cannot fold operation involving virtual base class (*"type"*\ )
 * - ``2807``\ 
   - | ``initializer_not_constant``\ :
     | initialization is not constant
 * - ``2808``\ 
   - | ``constexpr_incomplete_type``\ :
     | cannot evaluate value of incomplete *"type"*\ 
 * - ``2809``\ 
   - | ``nodiscard_routine``\ :
     | ignoring return value from routine declared with "nodiscard" attribute
 * - ``2810``\ 
   - | ``nodiscard_return_type``\ :
     | ignoring return value type with "nodiscard" attribute
 * - ``2811``\ 
   - | ``nodiscard_doesnt_apply``\ :
     | the "nodiscard" attribute does not apply to destructors or routines
       with void return type
 * - ``2812``\ 
   - | ``fallthrough_applies_to_null_statement``\ :
     | the "fallthrough" attribute only applies to null statements
 * - ``2813``\ 
   - | ``fallthrough_not_in_switch``\ :
     | the "fallthrough" attribute may only appear in an enclosing switch
       statement
 * - ``2814``\ 
   - | ``fallthrough_must_precede_switch_case``\ :
     | fallthrough statement must precede switch case label or default
 * - ``2815``\ 
   - | ``constexpr_expiring_temporary``\ :
     | reference or pointer to temporary with limited lifetime
 * - ``2816``\ 
   - | ``address_of_nontrue_enable_if_function``\ :
     | cannot take the address of a function with an "enable_if" attribute
       whose condition is not unconditionally true
 * - ``2817``\ 
   - | ``nonconstant_enable_if_attr``\ :
     | "enable_if" attributes with conditions that are not constant values
       are not currently supported
 * - ``2818``\ 
   - | ``attribute_declared_here``\ :
     | attribute was declared here
 * - ``2819``\ 
   - | ``has_include_not_in_if``\ :
     | __has_include cannot appear outside #if
 * - ``2820``\ 
   - | ``coclass_base_requirements_not_met``\ :
     | could not add CComCoClass base class
 * - ``2821``\ 
   - | ``constexpr_string_not_null_terminated``\ :
     | not a null-terminated string
 * - ``2822``\ 
   - | ``non_scalar_vacuous_dtor_call``\ :
     | non-scalar type *"type"*\  cannot be used in a pseudo-destructor call
 * - ``2823``\ 
   - | ``constexpr_weak_address``\ :
     | address of "weak" *entity-kind "entity"*\  is not constant
 * - ``2824``\ 
   - | ``excessive_rescan_depth``\ :
     | too many recursive substitutions of function template signatures
 * - ``2825``\ 
   - | ``invalid_struct_binding_specifier``\ :
     | invalid specifier for structured binding declaration
 * - ``2826``\ 
   - | ``invalid_struct_binding_syntax``\ :
     | invalid structured binding syntax
 * - ``2827``\ 
   - | ``missing_initializer``\ :
     | missing initializer
 * - ``2828``\ 
   - | ``invalid_struct_binding_type``\ :
     | type *"type"*\  has no components to bind to
 * - ``2829``\ 
   - | ``too_many_struct_bindings``\ :
     | too many identifiers
 * - ``2830``\ 
   - | ``missing_bindings``\ :
     | there are more elements than there are binding names
 * - ``2831``\ 
   - | ``missing_std_tuple_element``\ :
     | "std::tuple_element" not defined
 * - ``2832``\ 
   - | ``missing_std_tuple_element_instance``\ :
     | cannot instantiate "std::tuple_element" for <*xxxx*\ , *"type"*\ >
 * - ``2833``\ 
   - | ``failed_tuple_container_member_get``\ :
     | cannot call member function "get<*xxxx*\ >()" for type *"type"*\ 
 * - ``2834``\ 
   - | ``tuple_get_no_matching_overload``\ :
     | no instance of *"entity"*\  matches the argument list
 * - ``2835``\ 
   - | ``struct_binding_undefined_identifier``\ :
     | this structured binding requires a suitable *"xxxx"*\  function and
       none was found
 * - ``2836``\ 
   - | ``struct_binding_inline``\ :
     | a structured binding cannot be declared "inline"
 * - ``2837``\ 
   - | ``struct_binding_constexpr``\ :
     | a structured binding cannot be declared "constexpr"
 * - ``2838``\ 
   - | ``struct_binding_storage_class``\ :
     | a structured binding cannot declare an explicit storage class
 * - ``2839``\ 
   - | ``invalid_tuple_size``\ :
     | std::tuple_size<*"type"*\ >::value is not a valid integral
       constant-expression
 * - ``2840``\ 
   - | ``condition_does_not_declare_a_variable``\ :
     | a condition declaration must declare a variable
 * - ``2841``\ 
   - | ``condition_decl_must_have_initializer``\ :
     | a condition declaration must include an initializer
 * - ``2842``\ 
   - | ``parenthesized_init_not_allowed``\ :
     | a parenthesized initializer is not permitted for a condition
       declaration
 * - ``2843``\ 
   - | ``condition_with_multiple_declarators``\ :
     | a condition declaration can only declare one variable
 * - ``2844``\ 
   - | ``struct_binding_lambda``\ :
     | structured binding cannot bind to closure type
 * - ``2845``\ 
   - | ``struct_binding_private_member``\ :
     | cannot bind to non-public *entity-kind "entity"*\ 
 * - ``2846``\ 
   - | ``struct_binding_incomplete_type``\ :
     | cannot bind to incomplete type *"type"*\ 
 * - ``2847``\ 
   - | ``invalid_init_statement``\ :
     | this declaration is not valid here
 * - ``2848``\ 
   - | ``constexpr_function_with_function_try_block``\ :
     | the body of a constexpr function cannot be a function try block
 * - ``2849``\ 
   - | ``branch_into_constexpr_if``\ :
     | transfer of control into a constexpr if block is not allowed
 * - ``2850``\ 
   - | ``lambda_capture_structured_binding``\ :
     | structured binding cannot be captured
 * - ``2851``\ 
   - | ``microsoft_version_doesnt_support_cpp17_mode``\ :
     | the version of Microsoft being emulated must be at least 1911 to use
       "--ms_c++17"
 * - ``2852``\ 
   - | ``attempt_to_read_past_end_of_object``\ :
     | attempt to read past the end of the object
 * - ``2853``\ 
   - | ``constexpr_lambdas_not_enabled``\ :
     | constexpr lambdas are not enabled in this mode
 * - ``2854``\ 
   - | ``lambda_not_constant_expr``\ :
     | a constant expression cannot contain a lambda expression
 * - ``2855``\ 
   - | ``template_argument_index_out_of_bounds``\ :
     | value exceeds number of template arguments
 * - ``2856``\ 
   - | ``fold_expression_operator_mismatch``\ :
     | second operator in binary fold expression does not match first
 * - ``2857``\ 
   - | ``invalid_fold_expression_operator``\ :
     | invalid fold expression operator
 * - ``2858``\ 
   - | ``two_packs_in_fold_expression``\ :
     | a binary fold expression cannot apply to two parameter packs
 * - ``2859``\ 
   - | ``invalid_empty_fold_expression``\ :
     | empty expansion not valid for this fold expression
 * - ``2860``\ 
   - | ``inline_nonstatic_data_mem``\ :
     | a nonstatic data member cannot be declared as inline
 * - ``2861``\ 
   - | ``no_pack_in_fold_expression``\ :
     | fold expression does not refer to a parameter pack
 * - ``2862``\ 
   - | ``same_param_types_with_different_exception_specifications``\ :
     | two functions with the same parameter types but different exception
       specifications cannot be overloaded
 * - ``2863``\ 
   - | ``dynamic_exc_spec_not_permitted``\ :
     | dynamic exception specifications are not permitted in this mode
 * - ``2865``\ 
   - | ``invalid_noexcept_specifier_operand``\ :
     | invalid operand for noexcept specifier
 * - ``2866``\ 
   - | ``lambda_in_noexcept_specifier``\ :
     | lambda expression cannot appear in noexcept specifier of a template
 * - ``2867``\ 
   - | ``inaccessible_rvalue_dtor``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) is inaccessible
 * - ``2868``\ 
   - | ``bad_enum_template_decl``\ :
     | invalid specifier in enum template declaration
 * - ``2869``\ 
   - | ``no_float80``\ :
     | 80-bit floating-point types are not supported in this configuration
 * - ``2870``\ 
   - | ``no_float128``\ :
     | 128-bit floating-point types are not supported in this configuration
 * - ``2871``\ 
   - | ``invalid_enumerator_value``\ :
     | invalid enumerator value
 * - ``2872``\ 
   - | ``must_be_atomic_qualified_type``\ :
     | must be an _Atomic qualified type
 * - ``2873``\ 
   - | ``element_type_incomplete``\ :
     | type of array element must be complete
 * - ``2874``\ 
   - | ``always_inline_suppressed``\ :
     | the always_inline attribute has been suppressed for this function
 * - ``2875``\ 
   - | ``negative_value``\ :
     | a negative value is not permitted here
 * - ``2876``\ 
   - | ``integer_pack_element_for_type``\ :
     | an integer pack element cannot match *entity-kind "entity"*\ 
 * - ``2877``\ 
   - | ``integer_pack_element_for_template``\ :
     | an integer pack element cannot match *entity-kind "entity"*\ 
 * - ``2878``\ 
   - | ``unexpected_designator``\ :
     | unexpected designator
 * - ``2879``\ 
   - | ``cannot_evaluate_builtin_offsetof``\ :
     | cannot evaluate \__builtin_offsetof
 * - ``2880``\ 
   - | ``deduction_guide_def``\ :
     | deduction guide *"type"*\  cannot be defined
 * - ``2881``\ 
   - | ``bad_deduction_guide_scope``\ :
     | deduction guide must be declared in the same scope as *entity-kind
       "entity"*\ 
 * - ``2882``\ 
   - | ``invalid_specifier_for_deduction_guide``\ :
     | invalid specifier for deduction guide declaration (only "explicit" is
       permitted)
 * - ``2883``\ 
   - | ``constexpr_mutable_field_load``\ :
     | mutable *entity-kind "entity"*\  of a constant cannot be accessed in
       a constant expression
 * - ``2884``\ 
   - | ``member_function_modifier_on_static_member``\ :
     | function modifier does not apply to static member declaration
 * - ``2885``\ 
   - | ``overloadable_attribute_requires_prototype``\ :
     | the "overloadable" attribute requires a prototyped function
       declaration
 * - ``2886``\ 
   - | ``cannot_deduce_auto_templ_param``\ :
     | cannot deduce "auto" template parameter type *"type"*\  from
       *"type"*\ 
 * - ``2887``\ 
   - | ``modified_class_template_placeholder``\ :
     | class template name must be a placeholder for the complete type being
       initialized (not for a component of that type)
 * - ``2888``\ 
   - | ``alias_declaration_is_cpp11``\ :
     | alias declarations are a C++11 feature
 * - ``2889``\ 
   - | ``alias_template_is_cpp11``\ :
     | alias templates are a C++11 feature
 * - ``2890``\ 
   - | ``bad_deduction_guide_return_type``\ :
     | the return type must directly designate a specialization of the
       associated class template
 * - ``2891``\ 
   - | ``explicit_deduction_guide_in_copy_list_init``\ :
     | copy-list-initialization cannot use "explicit" *entity-kind
       "entity"*\ 
 * - ``2893``\ 
   - | ``invalid_udl_value``\ :
     | Invalid value for user-defined literal operator
 * - ``2894``\ 
   - | ``has_cpp_attrib_not_in_if``\ :
     | *xxxx*\  cannot appear outside of preprocessor directives
 * - ``2895``\ 
   - | ``bad_deduction_guide_access``\ :
     | deduction guide must be declared with the same accessibility as
       *entity-kind "entity"*\ 
 * - ``2896``\ 
   - | ``lambda_not_allowed_here``\ :
     | a lambda is not permitted in this context
 * - ``2897``\ 
   - | ``align_not_equivalent``\ :
     | specified alignment is not equivalent to previous declaration
 * - ``2898``\ 
   - | ``no_alignment_on_definition``\ :
     | no alignment specified on definition; previous declaration had
       specified an alignment
 * - ``2899``\ 
   - | ``builtin_needs_128_bit_floats``\ :
     | builtin function is not available because 128-bit floating-point
       types are not supported
 * - ``2900``\ 
   - | ``constexpr_shift_negative_value``\ :
     | left-shifting a negative value has undefined behavior
 * - ``2901``\ 
   - | ``no_array_designators_in_cpp_mode``\ :
     | array designators are nonstandard in C++
 * - ``2902``\ 
   - | ``no_chained_designators_in_cpp_mode``\ :
     | chained designators are nonstandard in C++
 * - ``2903``\ 
   - | ``no_mixed_init_in_cpp_mode``\ :
     | mixing designated and non-designated initializers is nonstandard in
       C++
 * - ``2904``\ 
   - | ``no_out_of_order_init_in_cpp_mode``\ :
     | out-of-order initializers are nonstandard in C++
 * - ``2905``\ 
   - | ``invalid_string_literal_operator_template``\ :
     | a string literal operator template must have a template parameter
       list equivalent to "<typename T, T ...>"
 * - ``2906``\ 
   - | ``duplicate_designator``\ :
     | duplicate designator is not allowed
 * - ``2907``\ 
   - | ``likely_unlikely_conflict``\ :
     | attribute conflicts with previous likely/unlikely attribute
 * - ``2908``\ 
   - | ``implicit_copy_this_capture_deprecated``\ :
     | the implicit by-copy capture of "this" is deprecated
 * - ``2909``\ 
   - | ``empty_lambda_template_param_list``\ :
     | an empty template parameter list is not allowed in a lambda expression
 * - ``2910``\ 
   - | ``microsoft_version_doesnt_support_cpp20_mode``\ :
     | the version of Microsoft being emulated must be at least 1920 to use
       "--ms_c++20"
 * - ``2911``\ 
   - | ``bad_stdc_pragma_arg_for_mode``\ :
     | STDC pragma argument not accepted in this mode
 * - ``2912``\ 
   - | ``if_constexpr_is_cpp17``\ :
     | constexpr if statements are a C++17 feature
 * - ``2913``\ 
   - | ``no_pack_expansion_in_designator``\ :
     | pack expansion is not allowed in a designated initializer list
 * - ``2914``\ 
   - | ``no_designator_value``\ :
     | field designator has no value
 * - ``2915``\ 
   - | ``multiple_union_field_initializers_empty``\ :
     | a union can have at most one field initializer
 * - ``2916``\ 
   - | ``bad_ordering_type``\ :
     | no valid std::*xxxx*\  type found (<compare> must be included)
 * - ``2917``\ 
   - | ``invalid_spaceship_types``\ :
     | invalid types (*"type"*\  and *"type"*\ ) for built-in operator<=>
 * - ``2918``\ 
   - | ``fold_expressions_nonstandard``\ :
     | fold expressions are nonstandard in this mode
 * - ``2919``\ 
   - | ``selection_initializer_nonstandard``\ :
     | C++17-style initializer is nonstandard in this mode
 * - ``2920``\ 
   - | ``star_this_capture_nonstandard``\ :
     | capturing \*this is nonstandard in this mode
 * - ``2921``\ 
   - | ``using_attribute_nonstandard``\ :
     | C++17-style "using" attribute prefix is nonstandard in this mode
 * - ``2922``\ 
   - | ``nested_namespace_nonstandard``\ :
     | C++17-style nested namespaces are nonstandard in this mode
 * - ``2923``\ 
   - | ``constexpr_and_consteval_specifiers``\ :
     | only one of "constexpr", "consteval", and "constinit" can appear on a
       declaration
 * - ``2924``\ 
   - | ``consteval_virtual_combination``\ :
     | a function cannot be both consteval and virtual in this mode
 * - ``2925``\ 
   - | ``consteval_explicit_instantiation``\ :
     | "consteval" is not allowed on an explicit instantiation directive
 * - ``2926``\ 
   - | ``invalid_consteval``\ :
     | "consteval" is not valid here
 * - ``2927``\ 
   - | ``consteval_destructor``\ :
     | a destructor cannot be consteval
 * - ``2928``\ 
   - | ``consteval_ctor_with_virtual_base``\ :
     | a constructor for a class with virtual bases cannot be consteval
 * - ``2929``\ 
   - | ``consteval_variable``\ :
     | "consteval" is not permitted on the declaration of a variable or
       static data member
 * - ``2930``\ 
   - | ``previous_consteval_decl_conflict``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) was previously
       declared consteval
 * - ``2931``\ 
   - | ``previous_nonconsteval_decl_conflict``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) was previously
       not declared consteval
 * - ``2932``\ 
   - | ``consteval_main``\ :
     | function "main" may not be declared consteval
 * - ``2933``\ 
   - | ``consteval_call_nonconstant``\ :
     | call to consteval *entity-kind "entity"*\  did not produce a valid
       constant expression
 * - ``2934``\ 
   - | ``address_of_consteval_function``\ :
     | address of consteval *entity-kind "entity"*\  in constant expression
       result
 * - ``2935``\ 
   - | ``consteval_overrides_nonconsteval``\ :
     | consteval member cannot override non-consteval *entity-kind
       "entity"*\ 
 * - ``2936``\ 
   - | ``nonconsteval_overrides_consteval``\ :
     | non-consteval member cannot override consteval *entity-kind
       "entity"*\ 
 * - ``2938``\ 
   - | ``constexpr_invalid_dynamic_cast``\ :
     | dynamic_cast to subobject of type *"type"*\  is invalid (most-derived
       type is *"type"*\ )
 * - ``2939``\ 
   - | ``VA_OPT_not_allowed``\ :
     | the identifier \__VA_OPT\__ can only appear in the replacement lists
       of variadic macros
 * - ``2940``\ 
   - | ``nested_VA_OPT``\ :
     | __VA_OPT\__ cannot appear in a \__VA_OPT\__ operand
 * - ``2941``\ 
   - | ``unclosed_VA_OPT``\ :
     | missing closing parenthesis for \__VA_OPT\__
 * - ``2942``\ 
   - | ``missing_VA_OPT_paren``\ :
     | __VA_OPT\__ must be followed by "("
 * - ``2943``\ 
   - | ``paste_cannot_be_first_in_VA_OPT``\ :
     | "##" may not be first in a \__VA_OPT\__ operand
 * - ``2944``\ 
   - | ``paste_cannot_be_last_in_VA_OPT``\ :
     | "##" may not be last in a \__VA_OPT\__ operand
 * - ``2945``\ 
   - | ``nested_inline_namespace_nonstandard``\ :
     | C++20-style nested inline namespaces are nonstandard in this mode
 * - ``2946``\ 
   - | ``derived_class_too_far``\ :
     | cannot convert pointer to base class *"type"*\  to pointer to derived
       class *"type"*\  -- attempt to point beyond the most-derived object
 * - ``2948``\ 
   - | ``invalid_variable_main``\ :
     | "main" cannot be used as a global variable name or given C language
       linkage
 * - ``2949``\ 
   - | ``linkage_main``\ :
     | function "main" cannot be declared in a linkage-specification
 * - ``2950``\ 
   - | ``struct_binding_in_condition``\ :
     | structured binding is not allowed in a condition
 * - ``2951``\ 
   - | ``missing_attr_namespace``\ :
     | an attribute namespace identifier is required before "::"
 * - ``2952``\ 
   - | ``multiple_attr_namespaces``\ :
     | only one attribute namespace is allowed
 * - ``2953``\ 
   - | ``bad_return``\ :
     | "return" not allowed here
 * - ``2954``\ 
   - | ``struct_binding_with_multiple_declarators``\ :
     | a structured binding cannot be combined with other declarators
 * - ``2955``\ 
   - | ``branch_out_of_constant``\ :
     | cannot branch out of a constant-evaluation context
 * - ``2956``\ 
   - | ``struct_binding_template``\ :
     | structured binding templates are not permitted
 * - ``2957``\ 
   - | ``braced_init_in_parens``\ :
     | a parenthesized initializer must be an expression, not a
       brace-enclosed list
 * - ``2958``\ 
   - | ``cannot_deduce_class_template_arguments``\ :
     | cannot deduce class template arguments
 * - ``2959``\ 
   - | ``consteval_new_or_delete_operator``\ :
     | a new or delete operator cannot be declared "consteval"
 * - ``2960``\ 
   - | ``address_of_consteval_function_leaked``\ :
     | the address of a consteval function cannot be used here
 * - ``2961``\ 
   - | ``alignof_function_type``\ :
     | the alignment of a function type (*"type"*\ ) is nonstandard
 * - ``2962``\ 
   - | ``alignof_incomplete_array``\ :
     | the alignment of an array of unspecified bound is nonstandard in C
 * - ``2963``\ 
   - | ``cannot_be_common_internal_linkage``\ :
     | a variable cannot have both "common" and "internal_linkage" attributes
 * - ``2964``\ 
   - | ``internal_linkage_not_on_prior_declaration``\ :
     | the "internal_linkage" attribute did not appear on a prior declaration
 * - ``2965``\ 
   - | ``no_class_template_guide``\ :
     | no viable template argument deduction candidate found for
       *entity-kind "entity"*\ 
 * - ``2966``\ 
   - | ``fully_qualified_constructor_call``\ :
     | a fully qualified constructor call is not allowed
 * - ``2967``\ 
   - | ``bad_scope_for_defaulted_comparison``\ :
     | a defaulted comparison operator must be a member or a friend of the
       class it applies to
 * - ``2968``\ 
   - | ``bad_param_type_for_defaulted_comparison``\ :
     | bad type *"type"*\  for parameter of defaulted comparison operator
       (must be "reference to const X" where X is the enclosing class type)
 * - ``2969``\ 
   - | ``return_type_of_default_comparison_must_be_bool``\ :
     | return type of defaulted comparison operator must be "bool"
 * - ``2970``\ 
   - | ``nonconst_defaulted_member_comparison``\ :
     | a defaulted member comparison operator must be "const"
 * - ``2972``\ 
   - | ``no_return_value_and_return_void``\ :
     | a coroutine's promise type *"type"*\  cannot have both "return_void"
       and "return_value" set
 * - ``2973``\ 
   - | ``return_value_at``\ :
     | "return_value" declared at line *xxxx*\ 
 * - ``2974``\ 
   - | ``return_void_at``\ :
     | "return_void" declared at line *xxxx*\ 
 * - ``2975``\ 
   - | ``implicit_co_return_with_no_return_void``\ :
     | missing co_return statement while *"type"*\  has no "return_void" at
       end of *entity-kind "entity"*\ 
 * - ``2976``\ 
   - | ``no_nothrow_global_new_for_coroutine``\ :
     | no nothrow variant of the global "operator new" found for coroutine
       state allocation
 * - ``2977``\ 
   - | ``no_viable_delete_for_coroutine``\ :
     | no viable "operator delete" found for coroutine state deallocation
 * - ``2978``\ 
   - | ``no_constexpr_coroutine``\ :
     | a constexpr function cannot be a coroutine
 * - ``2979``\ 
   - | ``await_operand_not_a_class``\ :
     | the operand to this *xxxx*\  expression resolves to non-class
       *"type"*\ 
 * - ``2980``\ 
   - | ``await_not_allowed_in_static_initializer``\ :
     | a co_await expression is not allowed in a static initializer
 * - ``2981``\ 
   - | ``final_suspend_cannot_throw``\ :
     | the co_await expression calling *entity-kind "entity"*\  must be
       non-throwing
 * - ``2982``\ 
   - | ``excessive_comparison_rewrites``\ :
     | too many recursive comparison rewrite operations
 * - ``2983``\ 
   - | ``invalid_placeholder_for_defaulted_spaceship_return``\ :
     | a deducible return type for a default operator<=> must be "auto"
 * - ``2984``\ 
   - | ``constexpr_implied_source_nonconstant``\ :
     | implicit copy of non-constant source
 * - ``2985``\ 
   - | ``struct_binding_restricted_storage_class``\ :
     | a structured binding cannot declare an explicit storage class other
       than static or thread_local
 * - ``2986``\ 
   - | ``defaulted_comparison_for_property``\ :
     | defaulted comparison operators are not supported for nontrivial
       Microsoft property fields
 * - ``2987``\ 
   - | ``invalid_std_comparison_type``\ :
     | standard comparison type (*"type"*\ ) must be a class type with a
       single nonstatic data member of integral type
 * - ``2988``\ 
   - | ``invalid_std_comparison_value``\ :
     | no constexpr static data member *"xxxx"*\  found in *"type"*\ 
 * - ``2989``\ 
   - | ``constexpr_alloc_too_large``\ :
     | number of elements (*n*\ ) too large for dynamic allocation
 * - ``2990``\ 
   - | ``constexpr_allocation_too_large``\ :
     | constexpr dynamic allocation request too large
 * - ``2991``\ 
   - | ``constexpr_bad_deallocation``\ :
     | deallocation of storage that was not dynamically allocated
 * - ``2992``\ 
   - | ``constexpr_bad_deallocation_size``\ :
     | deallocation size (*n*\ ) does not correspond to size allocated
       (*n*\ )
 * - ``2993``\ 
   - | ``constexpr_allocation_pos``\ :
     | allocation occurred here
 * - ``2994``\ 
   - | ``constexpr_bad_deallocation_type``\ :
     | deallocation type (*"type"*\ ) does not correspond to allocation type
       (*"type"*\ )
 * - ``2995``\ 
   - | ``constexpr_leftover_allocations``\ :
     | some dynamic allocations (total number = *n*\ ) were not deallocated
 * - ``2996``\ 
   - | ``constexpr_invalid_intrinsic_signature``\ :
     | intrinsic *entity-kind "entity"*\  declared with unexpected signature
       (type *"type"*\ )
 * - ``2997``\ 
   - | ``constexpr_begin_report``\ :
     | >> output from std::\__report_constexpr_value
 * - ``2998``\ 
   - | ``constexpr_end_report``\ :
     | >> end output from std::\__report_constexpr_value
 * - ``2999``\ 
   - | ``constexpr_dependent_array_size``\ :
     | cannot use array with dependent array size in constexpr evaluation
 * - ``3000``\ 
   - | ``nodiscard_routine_with_reason``\ :
     | ignoring return value from routine declared with "nodiscard"
       attribute (*"xxxx"*\ )
 * - ``3001``\ 
   - | ``nodiscard_return_type_with_reason``\ :
     | ignoring return value type with "nodiscard" attribute (*"xxxx"*\ )
 * - ``3002``\ 
   - | ``nodiscard_constructor``\ :
     | constructor used to create discarded object has "nodiscard" attribute
 * - ``3003``\ 
   - | ``nodiscard_constructor_with_reason``\ :
     | constructor used to create discarded object has "nodiscard" attribute
       (*"xxxx"*\ )
 * - ``3004``\ 
   - | ``nodiscard_object_type``\ :
     | type of discarded object has "nodiscard" attribute
 * - ``3005``\ 
   - | ``nodiscard_object_type_with_reason``\ :
     | type of discarded object has "nodiscard" attribute (*"xxxx"*\ )
 * - ``3006``\ 
   - | ``vacuous_destructor_not_called``\ :
     | a pseudo-destructor reference can only be used for a
       pseudo-destructor call
 * - ``3007``\ 
   - | ``constexpr_explicit_dtor_call``\ :
     | an explicit destructor call is not permitted in a constant-expression
 * - ``3008``\ 
   - | ``comma_operator_in_array_subscript_deprecated``\ :
     | an unparenthesized comma operator in an array subscript expression is
       deprecated
 * - ``3009``\ 
   - | ``constexpr_alloc_too_small``\ :
     | number of dynamically-allocated elements (*n*\ ) too small for
       initializer
 * - ``3011``\ 
   - | ``volatile_ass_deprecated``\ :
     | use of the result of an assignment to a volatile scalar object is
       deprecated
 * - ``3012``\ 
   - | ``volatile_op_ass_deprecated``\ :
     | a volatile destination type for a compound assignment expression is
       deprecated
 * - ``3013``\ 
   - | ``volatile_func_param_deprecated``\ :
     | a volatile function parameter is deprecated
 * - ``3014``\ 
   - | ``volatile_return_type_deprecated``\ :
     | a volatile return type is deprecated
 * - ``3015``\ 
   - | ``volatile_str_bind_deprecated``\ :
     | use of a volatile qualifier on a structured binding is deprecated
 * - ``3016``\ 
   - | ``ext_vector_type_invalid_size``\ :
     | the "ext_vector_type" argument must be between 1 and 2047
 * - ``3017``\ 
   - | ``ext_vector_type_not_in_typedef``\ :
     | the "ext_vector_type" attribute may appear only in a typedef
 * - ``3018``\ 
   - | ``ext_vector_type_requires_integral_floating_type``\ :
     | the "ext_vector_type" attribute applies only to integer or
       floating-point types
 * - ``3019``\ 
   - | ``feature_test_macro_ignored``\ :
     | this feature-test macro is ignored (and returns 0) in the current
       compilation mode
 * - ``3020``\ 
   - | ``constexpr_multiple_union_initializers``\ :
     | cannot evaluate an aggregate initializer with multiple elements for a
       union
 * - ``3021``\ 
   - | ``cmp_operator_does_not_return_bool``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) selected for
       operator rewrite does not return type bool
 * - ``3022``\ 
   - | ``constexpr_class_specific_new``\ :
     | a new-expression calling a class-specific allocation function cannot
       be constant-evaluated
 * - ``3023``\ 
   - | ``constexpr_placement_new``\ :
     | a placement new-expression cannot be constant-evaluated
 * - ``3024``\ 
   - | ``constexpr_nonvirtual_subobject_delete``\ :
     | deleting through a subobject pointer requires a virtual destructor
 * - ``3026``\ 
   - | ``invalid_intaddr_address``\ :
     | operand of \__INTADDR\__ must be offset from null pointer
 * - ``3027``\ 
   - | ``ambiguous_c11_generic``\ :
     | _Generic construct matches multiple types
 * - ``3028``\ 
   - | ``ambiguous_c11_generic_previous_match``\ :
     | the other match is *"type"*\ 
 * - ``3029``\ 
   - | ``availability_attribute_ignored``\ :
     | the "availability" attribute used here is ignored
 * - ``3030``\ 
   - | ``init_stmt_in_range_for_nonstandard``\ :
     | C++20-style initializer statement in a range-based "for" statement is
       nonstandard in this mode
 * - ``3031``\ 
   - | ``co_await_on_non_range_based_for``\ :
     | co_await can only apply to a range-based "for" statement
 * - ``3032``\ 
   - | ``cannot_deduce_type_in_range_based_for``\ :
     | cannot deduce type of range in range-based "for" statement
 * - ``3033``\ 
   - | ``inline_variables_nonstandard``\ :
     | inline variables are a C++17 feature
 * - ``3034``\ 
   - | ``destroying_delete_wrong_type``\ :
     | destroying operator delete requires *"type"*\  as first parameter
 * - ``3035``\ 
   - | ``destroying_delete_with_extra_params``\ :
     | a destroying operator delete cannot have parameters other than
       std::size_t and std::align_val_t
 * - ``3036``\ 
   - | ``cl_relaxed_abstract_checking_only_in_cplusplus``\ :
     | relaxed abstract class options can be used only when compiling C++
 * - ``3037``\ 
   - | ``invalid_start_of_requires_clause_expr``\ :
     | invalid start of expression in requires clause
 * - ``3038``\ 
   - | ``cast_in_requires_clause``\ :
     | a cast expression in a requires-clause must be parenthesized
 * - ``3039``\ 
   - | ``invalid_operator_in_requires_clause``\ :
     | this operator cannot appear at the top level (without parentheses) in
       a requires-clause
 * - ``3040``\ 
   - | ``nonbool_atomic_constraint``\ :
     | atomic constraint must have type bool
 * - ``3041``\ 
   - | ``atomic_constraint_substitution_failed``\ :
     | atomic constraint failed substitution
 * - ``3042``\ 
   - | ``atomic_constraint_evaluation_failed``\ :
     | atomic constraint not constant
 * - ``3043``\ 
   - | ``atomic_constraint_false``\ :
     | atomic constraint evaluates to false
 * - ``3044``\ 
   - | ``template_constraint_not_satisfied``\ :
     | template constraint not satisfied
 * - ``3045``\ 
   - | ``bad_scope_for_concept``\ :
     | concept definition cannot appear in this scope
 * - ``3046``\ 
   - | ``invalid_concept_redecl``\ :
     | invalid redeclaration of *entity-kind "entity"*\  (declared at line
       *xxxx*\ )
 * - ``3047``\ 
   - | ``concept_arg_list_substitution_failed``\ :
     | substitution of arguments *"<templ-args>"*\  for concept-id failed
 * - ``3048``\ 
   - | ``concept_failed``\ :
     | concept is false for arguments *"<templ-args>"*\ 
 * - ``3049``\ 
   - | ``trailing_requires_clause_not_on_template``\ :
     | a requires-clause is not allowed here (not a templated function)
 * - ``3051``\ 
   - | ``requires_incompatible_with_previous_decl``\ :
     | requires-clause incompatible with *entity-kind "entity"*\  (declared
       at line *xxxx*\ )
 * - ``3052``\ 
   - | ``expected_an_attribute``\ :
     | expected an attribute
 * - ``3054``\ 
   - | ``exp_type_name``\ :
     | expected a type name
 * - ``3055``\ 
   - | ``requires_expr_ellipsis_param``\ :
     | an ellipsis parameter is not permitted in a requires-expression
 * - ``3056``\ 
   - | ``unnamed_require_expr_param``\ :
     | unnamed parameter in requires-expression has no effect
 * - ``3057``\ 
   - | ``exp_concept_name``\ :
     | expected a concept name
 * - ``3058``\ 
   - | ``is_constant_evaluated_in_constant_expression``\ :
     | call to *xxxx*\  appearing in a constant expression always produces
       "true"
 * - ``3059``\ 
   - | ``is_constant_evaluated_in_consteval_context``\ :
     | call to *xxxx*\  appearing in a consteval context always produces
       "true"
 * - ``3060``\ 
   - | ``is_constant_evaluated_in_nonconstexpr_context``\ :
     | call to *xxxx*\  appearing in a non-constexpr function always
       produces "false"
 * - ``3061``\ 
   - | ``type_constraint_failed``\ :
     | type constraint failed for *"type"*\ 
 * - ``3062``\ 
   - | ``cl_export_template_option_incompatible_with_modules``\ :
     | option "export" cannot be used in modes where C++ modules are enabled
 * - ``3063``\ 
   - | ``glb_mod_fgmt_decl_must_come_first``\ :
     | a global module fragment declaration must precede any other
       declaration
 * - ``3064``\ 
   - | ``module_decl_only_after_glb_mod``\ :
     | a module declaration can only be preceded by a global module fragment
 * - ``3065``\ 
   - | ``pvt_mod_fgmt_only_after_module``\ :
     | a private module fragment must be preceded by a module declaration
 * - ``3066``\ 
   - | ``cannot_export_fgmt``\ :
     | a *xxxx*\  module fragment cannot be exported
 * - ``3067``\ 
   - | ``more_than_one_module_decl``\ :
     | cannot declare more than one module
 * - ``3068``\ 
   - | ``more_than_one_fgmt_decl``\ :
     | cannot declare more than one *xxxx*\  module fragment
 * - ``3069``\ 
   - | ``module_req_primary_name``\ :
     | a module must be declared with a non-empty name
 * - ``3070``\ 
   - | ``header_not_importable``\ :
     | *"xxxx"*\  is not an importable header
 * - ``3071``\ 
   - | ``cannot_import_module_with_no_name``\ :
     | cannot import a module with no name
 * - ``3072``\ 
   - | ``module_cannot_depend_on_itself``\ :
     | a module cannot have an interface dependency on itself
 * - ``3073``\ 
   - | ``module_already_imported``\ :
     | *module "module name"*\  has already been imported
 * - ``3075``\ 
   - | ``module_file_not_found``\ :
     | could not find module file for module *"xxxx"*\ 
 * - ``3076``\ 
   - | ``cannot_import_module``\ :
     | could not import module file *"xxxx"*\ 
 * - ``3079``\ 
   - | ``unknown_ifc_partition``\ :
     | unknown partition name *"xxxx"*\ 
 * - ``3085``\ 
   - | ``types_must_have_same_size``\ :
     | the type of the second operand *"type"*\  must have the same size as
       *"type"*\ 
 * - ``3086``\ 
   - | ``type_must_be_trivially_copyable``\ :
     | type must be trivially copyable
 * - ``3087``\ 
   - | ``unsupported_type_for_bit_cast``\ :
     | type *"type"*\  is currently not supported for constexpr evaluation
       of \__builtin_bit_cast
 * - ``3088``\ 
   - | ``bitfields_not_allowed``\ :
     | class types with bitfields *"type"*\  are not currently supported for
       constexpr evaluation of \__builtin_bit_cast
 * - ``3089``\ 
   - | ``reference_type_not_allowed``\ :
     | non-static data member of reference type *"type"*\  prevents
       constexpr evaluation of \__builtin_bit_cast
 * - ``3090``\ 
   - | ``volatile_type_not_allowed``\ :
     | a volatile type *"type"*\  prevents constexpr evaluation of
       \__builtin_bit_cast
 * - ``3091``\ 
   - | ``invalid_bit_cast_type``\ :
     | a union, pointer, or pointer-to-member type *"type"*\  prevents
       constexpr evaluation of \__builtin_bit_cast
 * - ``3093``\ 
   - | ``deleted_inh_def_constructor``\ :
     | the sub-object construction of *"type"*\  for inheriting constructors
       cannot be performed -- the associated constructor is deleted
 * - ``3094``\ 
   - | ``promise_type_returns_return_void``\ :
     | *entity-kind "entity"*\  must return void
 * - ``3095``\ 
   - | ``invalid_start_of_member_declaration``\ :
     | invalid start of member declaration
 * - ``3096``\ 
   - | ``exp_auto``\ :
     | expected "auto"
 * - ``3097``\ 
   - | ``operator_not_allowed_after_new``\ :
     | this operator is not allowed at this point; parenthesize the
       preceding new-expression
 * - ``3098``\ 
   - | ``invalid_use_of_concept``\ :
     | invalid use of concept
 * - ``3099``\ 
   - | ``rvalue_defaulted_member_comparison``\ :
     | a defaulted member comparison operator cannot be "&&"-qualified
 * - ``3100``\ 
   - | ``constexpr_comparison_calls_nonconstexpr_function``\ :
     | default constexpr comparison function calls non-constexpr function
       *entity-kind "entity"*\  (declared at line *xxxx*\ )
 * - ``3101``\ 
   - | ``invalid_constexpr_memcmp``\ :
     | constexpr memory comparison is only supported for integer or
       array-of-integer objects
 * - ``3102``\ 
   - | ``constraint_concept_template``\ :
     | a concept template cannot have associated constraints
 * - ``3103``\ 
   - | ``export_not_allowed``\ :
     | "export" is not allowed
 * - ``3104``\ 
   - | ``export_class_members``\ :
     | exporting individual class members is not allowed
 * - ``3105``\ 
   - | ``export_must_introduce_name``\ :
     | an exported declaration must introduce a name
 * - ``3106``\ 
   - | ``export_cannot_contain_export``\ :
     | an export declaration cannot contain an export declaration (previous
       declaration at line *xxxx*\ )
 * - ``3107``\ 
   - | ``export_cannot_contain_import``\ :
     | an export declaration cannot contain a module import declaration
 * - ``3108``\ 
   - | ``export_only_in_modules``\ :
     | an export declaration can only appear in a module interface unit
 * - ``3109``\ 
   - | ``export_internal_linkage``\ :
     | an export declaration cannot export a name with internal linkage
 * - ``3112``\ 
   - | ``empty_requires_expression``\ :
     | a requires-expression must specify at least one requirement
 * - ``3113``\ 
   - | ``invalid_constinit``\ :
     | "constinit" is not valid here
 * - ``3114``\ 
   - | ``constinit_variable_storage``\ :
     | "constinit" is only valid for declarations of variables with static
       or thread storage duration
 * - ``3115``\ 
   - | ``constinit_variable_has_dynamic_init``\ :
     | constinit variable requires dynamic initialization
 * - ``3116``\ 
   - | ``missing_constinit``\ :
     | variable was previously declared with "constinit" at line *xxxx*\ 
 * - ``3117``\ 
   - | ``use_of_non_prototype_func_declarator``\ :
     | use of non-prototype function declarator
 * - ``3118``\ 
   - | ``cannot_be_const_qualified``\ :
     | argument cannot have a const-qualified type
 * - ``3119``\ 
   - | ``ptr_to_mem_of_incomplete_class``\ :
     | a pointer-to-member of an incomplete type *"type"*\  is not allowed
 * - ``3120``\ 
   - | ``pack_init_capture_not_enabled``\ :
     | pack expansion in init-capture not enabled in this mode
 * - ``3121``\ 
   - | ``pack_init_capture_is_cpp20``\ :
     | pack expansion in init-capture is a C++20 feature
 * - ``3122``\ 
   - | ``comparison_defaulted_in_class_must_be_first_decl``\ :
     | a comparison operator defaulted in a class definition must be the
       first declaration of that comparison operator (*entity-kind
       "entity"*\  (declared at line *xxxx*\ ))
 * - ``3123``\ 
   - | ``pack_init_capture_in_non_variadic_context``\ :
     | a pack expansion in an init-capture can be used only in a variadic
       template
 * - ``3124``\ 
   - | ``type_constraint_requires_type_concept``\ :
     | type constraint uses *entity-kind "entity"*\  (declared at line
       *xxxx*\ ) that is not a type concept (i.e., a concept template whose
       first parameter is a type parameter)
 * - ``3125``\ 
   - | ``placeholder_type_failed_constraint``\ :
     | the deduced placeholder type *"type"*\  failed the type constraint
 * - ``3126``\ 
   - | ``ineligible_default_constructor``\ :
     | default constructor for *"type"*\  is not eligible
 * - ``3127``\ 
   - | ``ambiguous_destructor_constraints``\ :
     | destructor for *"type"*\  is ambiguous because of unordered
       constraints
 * - ``3128``\ 
   - | ``failed_destructor_constraints``\ :
     | destructor for *"type"*\  is ineligible because of failed constraints
 * - ``3129``\ 
   - | ``destructor_position``\ :
     | ambiguous destructor candidate
 * - ``3130``\ 
   - | ``trailing_requires_on_virtual_func``\ :
     | a virtual function cannot have a trailing requires clause
 * - ``3131``\ 
   - | ``function_ineligible``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) does not satisfy
       its constraints
 * - ``3132``\ 
   - | ``bad_decltype_qualifier``\ :
     | result of decltype qualifier *"type"*\  is not a class or enumeration
 * - ``3133``\ 
   - | ``cpp20_reversed_comparison_ambiguity``\ :
     | comparison is ambiguous in standard C++20 because the implied
       comparison operator with reversed parameters is an equally good match
       -- this is usually caused by a missing "const" qualifier on the
       comparison operator; see *"entity"*\  (declared at line *xxxx*\ )
 * - ``3134``\ 
   - | ``invalid_concept_id``\ :
     | invalid concept-id
 * - ``3135``\ 
   - | ``requires_clause_arg_list_substitution_failed``\ :
     | substitution of arguments *"<templ-args>"*\  for requires-clause
       failed
 * - ``3136``\ 
   - | ``ineligible_member_function``\ :
     | constraints for *entity-kind "entity"*\  (declared at line *xxxx*\ )
       are not satisfied
 * - ``3137``\ 
   - | ``var_with_virtual_bases_in_constexpr_function``\ :
     | variable type *"type"*\  in constexpr function has virtual base
       classes
 * - ``3138``\ 
   - | ``constexpr_object_with_virtual_base``\ :
     | a constant expression cannot allocate a virtual base subobject (for
       type *"type"*\ )
 * - ``3139``\ 
   - | ``template_parameter_has_nonstructural_class_type``\ :
     | a template parameter of class type must be of structural class type
 * - ``3140``\ 
   - | ``utf8_lit_no_ulit``\ :
     | support for UTF-8 literals requires u-literal support.
 * - ``3141``\ 
   - | ``duplicate_module_map``\ :
     | module file mapping for "*xxxx*\ " specified more than once
 * - ``3142``\ 
   - | ``duplicate_header_unit_map``\ :
     | header unit mapping for "*xxxx*\ " specified more than once
 * - ``3143``\ 
   - | ``missing_header_unit_map``\ :
     | no mapping specified for "*xxxx*\ "
 * - ``3145``\ 
   - | ``cannot_find_header_for_import``\ :
     | cannot find header "*xxxx*\ " to import
 * - ``3146``\ 
   - | ``multiple_module_matches``\ :
     | more than one file in the module file list matches "*xxxx*\ "
 * - ``3147``\ 
   - | ``module_file_mismatch``\ :
     | module file found for "*xxxx*\ " is for a different module
 * - ``3149``\ 
   - | ``module_read_error``\ :
     | unable to read module file
 * - ``3150``\ 
   - | ``builtin_needs_char8_t``\ :
     | builtin function is not available because the char8_t type is not
       supported with the current options
 * - ``3152``\ 
   - | ``nonstandard_use_of_explicit_default_constructor``\ :
     | nonstandard use of explicit constructor *"entity"*\  (declared at
       line *xxxx*\ ) for default aggregate element initialization
 * - ``3153``\ 
   - | ``constexpr_memcpy_operand_not_object``\ :
     | source or destination of memcpy-like intrinsic does not point to an
       object
 * - ``3154``\ 
   - | ``constexpr_memcpy_distinct_types``\ :
     | memcpy-like intrinsic attempts to copy representationally-distinct
       types *"type"*\  and *"type"*\ 
 * - ``3155``\ 
   - | ``constexpr_memcpy_nontrivial_type``\ :
     | memcpy-like intrinsic attempts to copy nontrivially-copyable type
       *"type"*\ 
 * - ``3156``\ 
   - | ``constexpr_memcpy_partial_object``\ :
     | memcpy-like intrinsic attempts to copy partial object
 * - ``3157``\ 
   - | ``constexpr_memcpy_overflow``\ :
     | memcpy-like intrinsic attempts to copy past array boundary
 * - ``3158``\ 
   - | ``constexpr_memcpy_overlap``\ :
     | memcpy-like intrinsic attempts to copy overlapping byte ranges (using
       corresponding memmove operation instead)
 * - ``3159``\ 
   - | ``nondefining_friend_requires_clause``\ :
     | a friend declaration with a trailing-requires-clause must be a
       definition
 * - ``3160``\ 
   - | ``type_not_scalar``\ :
     | expression must have arithmetic or pointer type but has type
       *"type"*\ 
 * - ``3161``\ 
   - | ``type_not_arithmetic_or_enum_or_pointer``\ :
     | expression must have arithmetic, enum, or pointer type but has type
       *"type"*\ 
 * - ``3162``\ 
   - | ``type_not_arithmetic_or_unscoped_enum_or_pointer``\ :
     | expression must have arithmetic, unscoped enum, or pointer type but
       has type *"type"*\ 
 * - ``3163``\ 
   - | ``type_not_pointer``\ :
     | expression must have pointer type but it has type *"type"*\ 
 * - ``3164``\ 
   - | ``member_access_requires_pointer``\ :
     | operator -> or ->\* applied to *"type"*\  instead of to a pointer type
 * - ``3166``\ 
   - | ``cannot_interpret_target_bits``\ :
     | cannot interpret bit layout for this compilation target
 * - ``3167``\ 
   - | ``ifc_no_corresponding_operator``\ :
     | no corresponding operator for IFC operator *"xxxx"*\ 
 * - ``3168``\ 
   - | ``ifc_no_corresponding_calling_conv``\ :
     | no corresponding calling convention for IFC calling convention
       *"xxxx"*\ 
 * - ``3169``\ 
   - | ``module_file_contains_unsupported_constructs``\ :
     | *module "module name"*\  contains unsupported constructs
 * - ``3170``\ 
   - | ``unhandled_ifc_construct``\ :
     | unsupported IFC construct: *"xxxx"*\ 
 * - ``3171``\ 
   - | ``is_signed_no_longer_a_keyword``\ :
     | __is_signed is no longer a keyword from this point
 * - ``3172``\ 
   - | ``dim_not_const_unsigned_int``\ :
     | an array dimension must have a constant unsigned integer value
 * - ``3174``\ 
   - | ``modules_not_enabled``\ :
     | modules are not enabled in this mode
 * - ``3175``\ 
   - | ``import_name_not_allowed``\ :
     | "import" is not allowed in a module name
 * - ``3176``\ 
   - | ``module_name_not_allowed``\ :
     | "module" is not allowed in a module name
 * - ``3179``\ 
   - | ``not_an_enum_type``\ :
     | *entity-kind "entity"*\  is not an enum type
 * - ``3180``\ 
   - | ``using_enum_conflicts``\ :
     | enumerator *"entity"*\  conflicts with *entity-kind "entity"*\ 
 * - ``3181``\ 
   - | ``using_enum_redecl``\ :
     | enumerator *"entity"*\  has already been declared in this scope at
       line *xxxx*\ 
 * - ``3182``\ 
   - | ``empty_throw_specification_not_cpp20``\ :
     | the "throw()" specification is not part of C++20 and later
 * - ``3183``\ 
   - | ``multiple_header_map_matches``\ :
     | more than entry in the header unit map matches "*xxxx*\ "
 * - ``3184``\ 
   - | ``exp_push_pop``\ :
     | #pragma diagnostic must have either 'push' or 'pop' argument
 * - ``3185``\ 
   - | ``no_corresponding_push``\ :
     | no '#pragma diagnostic push' was found to match this 'diagnostic pop'
 * - ``3186``\ 
   - | ``macro_in_pp_directive``\ :
     | *"xxxx"*\  cannot be a macro when used in an import or module
       directive
 * - ``3187``\ 
   - | ``not_global_namespace``\ :
     | this directive can only appear in the global namespace scope
 * - ``3188``\ 
   - | ``export_not_at_namespace_scope``\ :
     | an "export" declaration can appear only at global or namespace scope
 * - ``3189``\ 
   - | ``identifier_not_keyword``\ :
     | *"xxxx"*\  is parsed as an identifier rather than a keyword because
       the tokens that follow it do not match those of a preprocessor
       directive
 * - ``3190``\ 
   - | ``apparent_module_pp_directive``\ :
     | this appears to be the start of a preprocessor directive, but the
       lack of a ';' followed immediately by a newline prevents that
 * - ``3191``\ 
   - | ``module_directive_in_macro``\ :
     | this appears to be a modules preprocessing directive, but such
       directives cannot appear within a macro expansion
 * - ``3192``\ 
   - | ``module_directive_in_if``\ :
     | a "module" directive cannot appear within the scope of conditional
       inclusion (e.g., #if, #else, #elseif, etc.)
 * - ``3193``\ 
   - | ``import_skipped``\ :
     | the import of *"xxxx"*\  has been skipped
 * - ``3194``\ 
   - | ``bad_gro_on_alloc_fail``\ :
     | promise type *"type"*\  must declare
       get_return_object_on_allocation_failure as a static member function
       requiring no arguments
 * - ``3195``\ 
   - | ``expl_spec_alias_template``\ :
     | an alias template cannot be explicitly specialized
 * - ``3196``\ 
   - | ``matching_lbrace``\ :
     | to match this "{"
 * - ``3197``\ 
   - | ``macro_invocation``\ :
     | in this macro invocation
 * - ``3198``\ 
   - | ``call_with_unusable_argument_conversion``\ :
     | call requires an ambiguous argument conversion
 * - ``3199``\ 
   - | ``named_module_source_conflict``\ :
     | declaration owned by module *xxxx*\  conflicts with *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``3200``\ 
   - | ``global_module_source_conflict``\ :
     | declaration owned by global module conflicts with *entity-kind
       "entity"*\  (declared at line *xxxx*\ ) owned by a named module
 * - ``3201``\ 
   - | ``bad_malloc_attribute``\ :
     | the first argument to a "malloc" attribute must be a function
 * - ``3202``\ 
   - | ``cannot_capture_var``\ :
     | cannot capture *"entity"*\  (declared at line *xxxx*\ )
 * - ``3203``\ 
   - | ``cannot_capture_this``\ :
     | cannot capture "this"
 * - ``3204``\ 
   - | ``already_in_consteval_context``\ :
     | already in consteval context
 * - ``3205``\ 
   - | ``if_consteval_requires_braced_dependent_statement``\ :
     | "if consteval" and "if not consteval" require braced dependent
       statements
 * - ``3206``\ 
   - | ``if_consteval_in_nonconstexpr_function``\ :
     | "if consteval" and "if not consteval" are meaningless in a
       non-constexpr function
 * - ``3207``\ 
   - | ``branch_into_if_consteval``\ :
     | transfer of control into an "if consteval" or "if not consteval"
       statement is not allowed
 * - ``3208``\ 
   - | ``constexpr_local_static``\ :
     | constant-evaluation cannot go through the declaration of a variable
       with static or thread storage duration
 * - ``3209``\ 
   - | ``mutable_qualifier_on_explicit_this_lambda``\ :
     | the mutable qualifier is not allowed on a lambda with an explicit
       "this" parameter
 * - ``3210``\ 
   - | ``static_with_explicit_this``\ :
     | a member function declared with "static" cannot have an explicit
       "this" parameter
 * - ``3211``\ 
   - | ``explicit_this_param_must_be_first``\ :
     | an explicit "this" parameter must be the first declared parameter
 * - ``3212``\ 
   - | ``bad_this``\ :
     | "this" is not allowed here
 * - ``3214``\ 
   - | ``explicit_this_needs_this``\ :
     | an explicit "this" function requires a selector operand
 * - ``3215``\ 
   - | ``if_consteval_nonstandard``\ :
     | "if consteval" and "if not consteval" are not standard in this mode
 * - ``3216``\ 
   - | ``lambda_without_parameters_nonstandard``\ :
     | omitting "()" in a lambda declarator is nonstandard in this mode
 * - ``3217``\ 
   - | ``lambda_without_parameters_requires_clause``\ :
     | a trailing-requires-clause is not permitted when the lambda parameter
       list is omitted
 * - ``3218``\ 
   - | ``invalid_ifc_partition``\ :
     | *module "module name"*\  invalid partition requested
 * - ``3219``\ 
   - | ``undefined_ifc_partition``\ :
     | *module "module name"*\  undefined partition (believed to be
       *"xxxx"*\ ) requested
 * - ``3222``\ 
   - | ``invalid_overflowing_ifc_position``\ :
     | *module "module name"*\  file position *n*\  (relative position
       *n*\ ) requested for partition *"xxxx"*\  - which overflows the end
       of its partition
 * - ``3223``\ 
   - | ``invalid_misaligned_ifc_position``\ :
     | *module "module name"*\  file position *n*\  (relative position
       *n*\ ) requested for partition *"xxxx"*\  - which is misaligned with
       its partitions elements
 * - ``3224``\ 
   - | ``invalid_ifc_position_backtrace_field``\ :
     | from subfield *"xxxx"*\  (relative position to node *n*\ )
 * - ``3225``\ 
   - | ``invalid_ifc_position_backtrace_pos``\ :
     | from partition *"xxxx"*\  element *n*\  (file position *n*\ ,
       relative position *n*\ )
 * - ``3226``\ 
   - | ``nonstandard_lambda_attributes``\ :
     | attributes on lambdas are a C++23 feature
 * - ``3227``\ 
   - | ``confusable_identifier``\ :
     | identifier *"xxxx"*\  could be confused with a visually-similar one
       appearing at line *xxxx*\ 
 * - ``3228``\ 
   - | ``suspicious_comment_formatting``\ :
     | this comment contains suspicious Unicode formatting control characters
 * - ``3229``\ 
   - | ``suspicious_string_formatting``\ :
     | this string contains Unicode formatting control characters that could
       result in unexpected runtime behavior
 * - ``3230``\ 
   - | ``suppressed_module_warning_diag``\ :
     | *n*\  suppressed warning was encountered while processing *module
       "module name"*\ 
 * - ``3231``\ 
   - | ``suppressed_module_warnings_diag``\ :
     | *n*\  suppressed warnings were encountered while processing *module
       "module name"*\ 
 * - ``3232``\ 
   - | ``suppressed_module_error_diag``\ :
     | *n*\  suppressed error was encountered while processing *module
       "module name"*\ 
 * - ``3233``\ 
   - | ``suppressed_module_errors_diag``\ :
     | *n*\  suppressed errors were encountered while processing *module
       "module name"*\ 
 * - ``3236``\ 
   - | ``virtual_with_explicit_this``\ :
     | a virtual member function cannot have an explicit "this" parameter
 * - ``3237``\ 
   - | ``address_of_unqualified_explicit_this_function``\ :
     | taking the address of an explicit "this" function requires a
       qualified name
 * - ``3238``\ 
   - | ``implicit_address_of_explicit_this_function``\ :
     | forming the address of an explicit "this" function requires the "&"
       operator
 * - ``3239``\ 
   - | ``string_literal_cannot_initialize_flexible_array_member``\ :
     | a string literal cannot be used to initialize a flexible array member
 * - ``3246``\ 
   - | ``ifc_missing_function_definition``\ :
     | the IFC representation of the definition of function *"xxxx"*\  is
       missing
 * - ``3247``\ 
   - | ``member_function_modifier_on_template``\ :
     | function modifier does not apply to member template declaration
 * - ``3248``\ 
   - | ``constexpr_too_many_nested_anonymous_types``\ :
     | member selection involves too many nested anonymous types
 * - ``3249``\ 
   - | ``no_common_type``\ :
     | there is no common type between the operands
 * - ``3250``\ 
   - | ``exp_pointer_to_member``\ :
     | expected a pointer-to-member
 * - ``3251``\ 
   - | ``lone_flexible_array_member``\ :
     | a flexible array member cannot be declared in an otherwise-empty type
 * - ``3252``\ 
   - | ``missing_gnu_srcloc_type``\ :
     | expected "std::source_location::\__impl" to be defined to a class
       with only the data members "_M_function_name", "_M_file_name",
       "_M_column", "_M_line"
 * - ``3253``\ 
   - | ``srcloc_column_bounds``\ :
     | given column number is too large for "std::source_location"
       implementation
 * - ``3254``\ 
   - | ``srcloc_line_bounds``\ :
     | given line number is too large for "std::source_location"
       implementation
 * - ``3255``\ 
   - | ``utf16_char_lit_too_long``\ :
     | a UTF-16 character constant cannot occupy more than one code unit;
       value truncated
 * - ``3256``\ 
   - | ``both_arguments_must_have_same_type``\ :
     | both arguments must have the same type
 * - ``3257``\ 
   - | ``invalid_type_for_builtin``\ :
     | type *"type"*\  is invalid as an argument for this builtin
 * - ``3258``\ 
   - | ``constexpr_called_from_rout``\ :
     | called from *entity-kind "entity"*\  (declared at line *xxxx*\ ):
 * - ``3259``\ 
   - | ``qualified_unnamed_bit_field_type``\ :
     | a qualified type is nonstandard for anonymous bit fields
 * - ``3260``\ 
   - | ``bad_vector_conditional_size``\ :
     | the element type of the vector condition (*"type"*\ ) must have the
       same size as the element type of the result (*"type"*\ )
 * - ``3261``\ 
   - | ``bad_vector_float_operand_size``\ :
     | the floating-point vector operand type (*"type"*\ ) has no matching
       integer vector type
 * - ``3262``\ 
   - | ``mangling_for_requires``\ :
     | mangling for "requires" expressions is not yet implemented
 * - ``3263``\ 
   - | ``unavailable_attr``\ :
     | because of an "unavailable" attribute
 * - ``3264``\ 
   - | ``duplicate_asm_qualifier``\ :
     | duplicate 'asm' qualifier
 * - ``3265``\ 
   - | ``incomplete_enum_bit_field_or_bad_opaque_enum``\ :
     | either a bit field with an incomplete enum type or an opaque
       enumeration with an invalid base type
 * - ``3266``\ 
   - | ``ifc_partition_mismatch``\ :
     | attempted to construct an element from IFC partition *"xxxx"*\  using
       an index into IFC partition *"xxxx"*\ 
 * - ``3267``\ 
   - | ``ifc_partition_bad_entry_size``\ :
     | the partition *"xxxx"*\  specified its entry size as *n*\  when *n*\ 
       was expected
 * - ``3268``\ 
   - | ``ifc_requirement_failure``\ :
     | an unexpected IFC requirement was encountered while processing
       *module "module name"*\ 
 * - ``3269``\ 
   - | ``ifc_requirement_failure_fill_in``\ :
     | condition failed at line *n*\  in *xxxx*\ : *"xxxx"*\ 
 * - ``3270``\ 
   - | ``circular_constraint``\ :
     | atomic constraint depends on itself
 * - ``3271``\ 
   - | ``noreturn_with_return_type``\ :
     | "noreturn" function has non-void return type
 * - ``3272``\ 
   - | ``ifc_bad_function_param_name``\ :
     | a correction has been made by dropping the parameter *"xxxx"*\  (at
       relative index *n*\ )
 * - ``3273``\ 
   - | ``default_arg_on_member_template_def``\ :
     | a default template argument cannot be specified on the definition of
       a member template outside its class
 * - ``3274``\ 
   - | ``ifc_bad_identifier``\ :
     | invalid IFC identifier name *"xxxx"*\  encountered during entity
       reconstruction
 * - ``3276``\ 
   - | ``invalid_ifc_sort_value``\ :
     | *module "module name"*\  invalid sort value
 * - ``3277``\ 
   - | ``ifc_function_template_parse_failure``\ :
     | a function template loaded from an IFC module was incorrectly parsed
       as *entity-kind "entity"*\  (declared at line *xxxx*\ )
 * - ``3278``\ 
   - | ``ifc_entity_ref_failure``\ :
     | failed to load an IFC entity reference in *module "module name"*\ 
 * - ``3279``\ 
   - | ``ifc_entity_ref_failure_info``\ :
     | from partition *"xxxx"*\  element *n*\  (file position *n*\ ,
       relative position *n*\ )
 * - ``3280``\ 
   - | ``no_chained_designators_with_destructor``\ :
     | chained designators are not permitted for a class type with a
       nontrivial destructor
 * - ``3281``\ 
   - | ``explicit_specialization_friend``\ :
     | an explicit specialization declaration may not be a friend declaration
 * - ``3282``\ 
   - | ``std_float128_not_supported``\ :
     | the std::float128_t type is not supported; std::float64_t will be
       used instead
 * - ``3284``\ 
   - | ``alias_template_deduction_guide``\ :
     | a deduction guide may not be declared for alias template *"entity"*\ 
 * - ``3285``\ 
   - | ``unavailable_entity``\ :
     | *entity-kind "entity"*\  was declared declared unavailable
 * - ``3286``\ 
   - | ``unavailable_entity_with_custom_message``\ :
     | *entity-kind "entity"*\  was declared unavailable (*"xxxx"*\ )
 * - ``3287``\ 
   - | ``deprecated_attr``\ :
     | because of a "deprecated" attribute
 * - ``3288``\ 
   - | ``explicit_lambda_template_parameters_is_cpp20``\ :
     | explicit lambda template parameters are a C++20 feature
 * - ``3289``\ 
   - | ``c23_noreturn_deprecated``\ :
     | the use of "_Noreturn" has been obsoleted in C23; use "[[noreturn]]"
       instead
 * - ``3290``\ 
   - | ``c23_alignof_deprecated``\ :
     | the use of "_Alignof" has been obsoleted in C23; use "alignof" instead
 * - ``3291``\ 
   - | ``c23_alignas_deprecated``\ :
     | the use of "_Alignas" has been obsoleted in C23; use "alignas" instead
 * - ``3292``\ 
   - | ``c23_bool_deprecated``\ :
     | the use of "_Bool" has been obsoleted in C23; use "bool" instead
 * - ``3293``\ 
   - | ``c23_static_assert_deprecated``\ :
     | the use of "_Static_assert" has been obsoleted in C23; use
       "static_assert" instead
 * - ``3294``\ 
   - | ``c23_thread_local_deprecated``\ :
     | the use of "_Thread_local" has been obsoleted in C23; use
       "thread_local" instead
 * - ``3295``\ 
   - | ``ms_ifc_unavailable``\ :
     | Microsoft mode must be enabled to use the module file *"xxxx"*\  (a
       Microsoft Visual Studio IFC module)
 * - ``3296``\ 
   - | ``file_for_module_not_found``\ :
     | could not open module file *"xxxx"*\ 
 * - ``3297``\ 
   - | ``found_from_module_map``\ :
     | found in the module map for module *"xxxx"*\ 
 * - ``3298``\ 
   - | ``found_from_header_unit_map``\ :
     | found in the header unit map for *"xxxx"*\ 
 * - ``3299``\ 
   - | ``cl_invalid_output_mode``\ :
     | unrecognized output mode (must be one of text, sarif): *xxxx*\ 
 * - ``3300``\ 
   - | ``cl_c23_typeof_option_only_in_C``\ :
     | option "c23_typeof" can be used only when compiling C
 * - ``3301``\ 
   - | ``cl_invalid_clang_version``\ :
     | invalid clang version number: *xxxx*\ 
 * - ``3305``\ 
   - | ``constexpr_flexible_array_initializer``\ :
     | cannot evaluate an initializer for a flexible array member
 * - ``3306``\ 
   - | ``nonstandard_bit_field_initializer``\ :
     | a default bit-field initializer is a C++20 feature
 * - ``3307``\ 
   - | ``ifc_too_many_template_args``\ :
     | too many arguments in template argument list in *module "module
       name"*\ 
 * - ``3308``\ 
   - | ``ifc_too_many_template_args_info``\ :
     | detected for the template argument represented by *"xxxx"*\  element
       *n*\  (file position *n*\ , relative position *n*\ )
 * - ``3309``\ 
   - | ``ifc_too_few_template_args``\ :
     | too few arguments in template argument list in *module "module
       name"*\ 
 * - ``3310``\ 
   - | ``ifc_too_few_template_args_info``\ :
     | detected while processing the template argument list represented by
       *"xxxx"*\  element *n*\  (file position *n*\ , relative position
       *n*\ )
 * - ``3311``\ 
   - | ``nonstandard_conversion_from_scoped_enum``\ :
     | conversion from scoped enumeration type *"type"*\  is nonstandard
 * - ``3312``\ 
   - | ``constexpr_allocation_mismatch``\ :
     | deallocation does not match allocation kind (one is for an array and
       the other not)
 * - ``3313``\ 
   - | ``constexpr_address_unknown``\ :
     | comparison involves unknown address (e.g., the address of a weak
       variable)
 * - ``3314``\ 
   - | ``bad_argument_to_make_signed``\ :
     | __make_signed is only compatible with non-bool integer and enum types
 * - ``3315``\ 
   - | ``bad_argument_to_make_unsigned``\ :
     | __make_unsigned is only compatible with non-bool integer and enum
       types
 * - ``3316``\ 
   - | ``intrinsic_name_released``\ :
     | the intrinsic name *"xxxx"*\  will be treated as an ordinary
       identifier from here
 * - ``3317``\ 
   - | ``array_subobject_not_initialized``\ :
     | access to uninitialized subobject at index *n*\ 
 * - ``3318``\ 
   - | ``ifc_line_number_overflow``\ :
     | IFC line number (*n*\ ) overflows maximum allowed value (*n*\ )
       *module "module name"*\ 
 * - ``3319``\ 
   - | ``invalid_unrepresentable_ifc_position``\ :
     | *module "module name"*\  requested element *n*\  of partition
       *"xxxx"*\ , this file position exceeds the maximum representable value
 * - ``3320``\ 
   - | ``wrong_number_of_arguments``\ :
     | wrong number of arguments
 * - ``3321``\ 
   - | ``candidate_failed_constraint``\ :
     | constraint on candidate *entity-kind "entity"*\  not satisfied
 * - ``3322``\ 
   - | ``candidate_wrong_param_count``\ :
     | number of parameters of *entity-kind "entity"*\  does not match the
       call
 * - ``3323``\ 
   - | ``candidate_expl_templ_arg_subst_failed``\ :
     | substituting explicit template arguments *"<templ-args>"*\  for
       *entity-kind "entity"*\  failed
 * - ``3324``\ 
   - | ``impl_deleted_move_candidate_ignored``\ :
     | *entity-kind "entity"*\  is an implicitly "= delete" move function
       and thus ignored during overload resolution
 * - ``3325``\ 
   - | ``arg_for_empty_param_pack``\ :
     | *entity-kind "entity"*\  does not match because argument #*n*\  is
       provided for an empty parameter pack
 * - ``3326``\ 
   - | ``nonviable_because_arg_mismatch``\ :
     | *entity-kind "entity"*\  does not match because argument #*n*\  does
       not match parameter
 * - ``3327``\ 
   - | ``deduction_failed``\ :
     | candidate *entity-kind "entity"*\  failed deduction
 * - ``3328``\ 
   - | ``builtin_operator_nonviable_because_arg_mismatch``\ :
     | built-in operator*xxxx*\  does not match because argument #*n*\  does
       not match parameter
 * - ``3329``\ 
   - | ``integral_operand``\ :
     | <integral>
 * - ``3330``\ 
   - | ``promoted_integral_operand``\ :
     | <promoted integral>
 * - ``3331``\ 
   - | ``ptrdiff_t_operand``\ :
     | <ptrdiff_t>
 * - ``3332``\ 
   - | ``enum_operand``\ :
     | <enum>
 * - ``3333``\ 
   - | ``scoped_enum_operand``\ :
     | <scoped enum>
 * - ``3334``\ 
   - | ``arith_operand``\ :
     | <arithmetic>
 * - ``3335``\ 
   - | ``promoted_arith_operand``\ :
     | <promoted arithmetic>
 * - ``3336``\ 
   - | ``nonbool_arith_operand``\ :
     | <non-bool arithmetic>
 * - ``3337``\ 
   - | ``pointer_operand``\ :
     | <pointer>
 * - ``3338``\ 
   - | ``nullptr_operand``\ :
     | <nullptr>
 * - ``3339``\ 
   - | ``handle_operand``\ :
     | <handle>
 * - ``3340``\ 
   - | ``handle_to_cli_array_operand``\ :
     | <handle to CLI array>
 * - ``3341``\ 
   - | ``pointer_to_object_operand``\ :
     | <pointer to object>
 * - ``3342``\ 
   - | ``pointer_to_function_operand``\ :
     | <pointer to function>
 * - ``3343``\ 
   - | ``ptr_to_member_operand``\ :
     | <pointer-to-member>
 * - ``3344``\ 
   - | ``bool_operand``\ :
     | <bool>
 * - ``3345``\ 
   - | ``bool_equivalent_operand``\ :
     | <bool-like>
 * - ``3346``\ 
   - | ``class_operand``\ :
     | <class>
 * - ``3347``\ 
   - | ``auto_cast_is_cpp23``\ :
     | auto(<expr>) and auto{<expr>} are a C++23 feature
 * - ``3348``\ 
   - | ``anon_union_using_declaration``\ :
     | invalid anonymous union -- using declaration is not allowed
 * - ``3349``\ 
   - | ``ifc_file_incompatibility``\ :
     | IFC file *"xxxx"*\  cannot be processed
 * - ``3350``\ 
   - | ``unsupported_ifc_file_version_info``\ :
     | IFC version *n*\ .*n*\  is not supported
 * - ``3351``\ 
   - | ``unsupported_ifc_file_arch_info``\ :
     | IFC architecture *"xxxx"*\  is incompatible with the current target
       architecture
 * - ``3352``\ 
   - | ``unknown_ifc_partition_conversion``\ :
     | *module "module name"*\  requests index *n*\  of an unsupported
       partition corresponding to *"xxxx"*\ 
 * - ``3353``\ 
   - | ``param_cannot_be_completed``\ :
     | parameter number *n*\  of *entity-kind "entity"*\  has type
       *"type"*\  which cannot be completed
 * - ``3354``\ 
   - | ``param_is_incomplete``\ :
     | parameter number *n*\  of *entity-kind "entity"*\  has incomplete
       type *"type"*\ 
 * - ``3355``\ 
   - | ``param_is_abstract``\ :
     | parameter number *n*\  of *entity-kind "entity"*\  has abstract type
       *"type"*\ 
 * - ``3356``\ 
   - | ``struct_bindings_is_cpp17``\ :
     | structured bindings are a C++17 feature
 * - ``3357``\ 
   - | ``capturing_struct_bindings_is_cpp20``\ :
     | capturing structured bindings is a C++20 feature
 * - ``3358``\ 
   - | ``bad_splicer_operand``\ :
     | operand of splicer has type *"type"*\  instead of std::meta::info
 * - ``3359``\ 
   - | ``not_a_type_reflection``\ :
     | operand (reflection for *reflection-description*\ ) is not the
       reflection of a type
 * - ``3360``\ 
   - | ``nonconstant_splicer_operand``\ :
     | nonconstant operand of splicer
 * - ``3361``\ 
   - | ``bad_std_string_view``\ :
     | use of *"type"*\  instead of std::string_view (=
       std::basic_string_view<char>)
 * - ``3362``\ 
   - | ``inconsistent_std_string_view``\ :
     | std::string_view used here is inconsistent with use in other
       intrinsics
 * - ``3363``\ 
   - | ``invalid_std_string_view_for_reflection``\ :
     | definition of std::string_view does not match assumptions of
       reflection (no base classes and data members for pointer and length)
 * - ``3364``\ 
   - | ``constexpr_reflection_not_of_constant``\ :
     | reflection is not that of a constant value
 * - ``3368``\ 
   - | ``bad_reflection_kind_for_expression_splice``\ :
     | bad reflection (*reflection-description*\ ) for expression splice
 * - ``3369``\ 
   - | ``already_defined_with_pos``\ :
     | *entity-kind "entity"*\  has already been defined (previous
       definition at line *xxxx*\ )
 * - ``3371``\ 
   - | ``incompatible_std_meta_value_of_type``\ :
     | extract of type *"type"*\  is not compatible with the given
       reflection (entity with type *"type"*\ )
 * - ``3372``\ 
   - | ``reflection_of_overloaded_set``\ :
     | reflecting an overload set is not currently permitted
 * - ``3373``\ 
   - | ``intrinsic_requires_template_instance``\ :
     | this intrinsic requires a reflection for a template instance
 * - ``3374``\ 
   - | ``invalid_reflection_equality``\ :
     | incompatible types *"type"*\  and *"type"*\  for operator
 * - ``3375``\ 
   - | ``invalid_reflection_for_intrinsic``\ :
     | invalid reflection for intrinsic metafunction
 * - ``3376``\ 
   - | ``nonmember_reflection_for_intrinsic``\ :
     | intrinsic metafunction requires a reflection for a class member
 * - ``3377``\ 
   - | ``union_base``\ :
     | a class cannot not derive from a union
 * - ``3378``\ 
   - | ``base_with_flexible_array``\ :
     | cannot derive from a class with a flexible array member
 * - ``3379``\ 
   - | ``null_reflection``\ :
     | null reflection
 * - ``3380``\ 
   - | ``namespace_alias``\ :
     | namespace alias
 * - ``3381``\ 
   - | ``unspecified_reflection``\ :
     | reflection (details unavailable)
 * - ``3382``\ 
   - | ``std_meta_substitute_bad_arg_reflection``\ :
     | bad reflection (*reflection-description*\ ) for template argument in
       std::meta::substitute
 * - ``3383``\ 
   - | ``std_meta_substitute_failed``\ :
     | call to std::meta::substitute (for *reflection-description*\ ) failed
 * - ``3384``\ 
   - | ``expired_reflection_value``\ :
     | reflection value refers to inactive entity
 * - ``3385``\ 
   - | ``cannot_splice_general_expression``\ :
     | an expression splice must splice a constant value, a variable, or a
       function
 * - ``3386``\ 
   - | ``cannot_splice_member_access``\ :
     | a member access splice must splice a data member or a member function
 * - ``3387``\ 
   - | ``splice_is_not_a_member_of_class``\ :
     | member *entity-kind "entity"*\  (declared at line *xxxx*\ ) is not a
       direct or indirect member of *"type"*\ 
 * - ``3388``\ 
   - | ``unicode_name_not_found``\ :
     | the name *"xxxx"*\  does not designate a known Unicode character
 * - ``3389``\ 
   - | ``unterminated_unicode_name``\ :
     | unterminated named Unicode character escape
 * - ``3390``\ 
   - | ``invalid_char_in_unicode_name``\ :
     | character cannot appear in a Unicode name
 * - ``3391``\ 
   - | ``empty_unicode_name``\ :
     | empty named Unicode character escape
 * - ``3392``\ 
   - | ``exp_lsplice``\ :
     | expected a "[:"
 * - ``3393``\ 
   - | ``exp_rsplice``\ :
     | expected a ":]"
 * - ``3394``\ 
   - | ``lambda_mutable_and_static``\ :
     | a lambda expression cannot be both "mutable" and "static"
 * - ``3395``\ 
   - | ``static_lambda_nonstandard``\ :
     | a "static" lambda expression is nonstandard
 * - ``3396``\ 
   - | ``static_lambda_with_capture``\ :
     | a "static" lambda expression must have an empty capture specification
 * - ``3399``\ 
   - | ``ifc_creation_failure``\ :
     | an IFC file could not be produced for the current translation unit
 * - ``3400``\ 
   - | ``unsupported_il_to_ifc``\ :
     | one or more entities cannot currently be written to an IFC file
 * - ``3401``\ 
   - | ``nonstandard_explicit_bool``\ :
     | 'explicit(bool)' is a C++20 feature
 * - ``3402``\ 
   - | ``bad_type_for_atomic_fetch``\ :
     | first argument must be a pointer to integer, enum, or supported
       floating-point type
 * - ``3403``\ 
   - | ``module_use_in_multi_tu_mode``\ :
     | C++ modules cannot be used when compiling multiple translation units
 * - ``3404``\ 
   - | ``module_use_with_export_template``\ :
     | C++ modules cannot be used with the pre-C++11 "export" feature
 * - ``3405``\ 
   - | ``ifc_unsupported_token``\ :
     | the IFC token *"xxxx"*\  is not supported
 * - ``3406``\ 
   - | ``pass_object_size_not_in_function_decl``\ :
     | the "pass_object_size" attribute is only valid on parameters of
       function declarations
 * - ``3407``\ 
   - | ``attr_arg_out_of_small_integer_range``\ :
     | the argument of the *"xxxx"*\  attribute *n*\  must be a value
       between 0 and *n*\ 
 * - ``3408``\ 
   - | ``ref_qualifier_ignored``\ :
     | a ref-qualifier here is ignored
 * - ``3409``\ 
   - | ``invalid_neon_vector_element_type``\ :
     | invalid NEON vector element type *"type"*\ 
 * - ``3410``\ 
   - | ``invalid_neon_polyvector_element_type``\ :
     | invalid NEON polyvector element type *"type"*\ 
 * - ``3411``\ 
   - | ``invalid_scalable_vector_element_type``\ :
     | invalid scalable vector element type *"type"*\ 
 * - ``3412``\ 
   - | ``invalid_scalable_vector_tuple_elements``\ :
     | invalid number of tuple elements for scalable vector type
 * - ``3413``\ 
   - | ``invalid_neon_vector_size``\ :
     | a NEON vector or polyvector must be either 64 or 128 bits wide
 * - ``3414``\ 
   - | ``sizeless_type_not_allowed``\ :
     | sizeless type *"type"*\  is not allowed
 * - ``3415``\ 
   - | ``value_init_of_sizeless_type``\ :
     | an object of the sizeless type *"type"*\  cannot be value-initialized
 * - ``3416``\ 
   - | ``ifc_unexpected_null_scope_member``\ :
     | unexpected null declaration index found as part of scope *n*\ 
 * - ``3417``\ 
   - | ``cl_unnamed_module_map``\ :
     | a module name must be specified for the module file map referencing
       the file *"xxxx"*\ 
 * - ``3418``\ 
   - | ``ifc_partition_missing_element``\ :
     | a null index value was received where a node in the IFC partition
       *"xxxx"*\  was expected
 * - ``3419``\ 
   - | ``void_template_variable``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) cannot have type
       *"type"*\ 
 * - ``3420``\ 
   - | ``ref_qualifier_nonstandard``\ :
     | a ref-qualifier is nonstandard in this mode
 * - ``3421``\ 
   - | ``range_based_for_nonstandard``\ :
     | a range-based "for" statement is nonstandard in this mode
 * - ``3422``\ 
   - | ``auto_type_nonstandard``\ :
     | "auto" as a type specifier is nonstandard in this mode
 * - ``3423``\ 
   - | ``cannot_import_module_bad_checksum``\ :
     | could not import module file *"xxxx"*\  due to file corruption
 * - ``3425``\ 
   - | ``extraneous_injected_member_tokens``\ :
     | extraneous tokens injected after member declaration
 * - ``3426``\ 
   - | ``bad_injection_scope``\ :
     | bad injection scope (*reflection-description*\ )
 * - ``3427``\ 
   - | ``expected_string_view_value``\ :
     | expected a value of type std::string_view but got *"type"*\ 
 * - ``3428``\ 
   - | ``extraneous_injected_statement_tokens``\ :
     | extraneous tokens injected after statement
 * - ``3429``\ 
   - | ``extraneous_injected_declaration_tokens``\ :
     | extraneous tokens injected after declaration
 * - ``3430``\ 
   - | ``tuple_index_overflow``\ :
     | tuple index value (*n*\ ) overflow
 * - ``3431``\ 
   - | ``constexpr_begin_report_tokens``\ :
     | >> output from std::meta::\__report_tokens
 * - ``3432``\ 
   - | ``constexpr_end_report_tokens``\ :
     | >> end output from std::meta::\__report_tokens
 * - ``3433``\ 
   - | ``constexpr_no_current_parameters``\ :
     | not in a context with parameter variables
 * - ``3434``\ 
   - | ``empty_delimited_escape``\ :
     | a delimited escape sequence must have at least one character
 * - ``3435``\ 
   - | ``unterminated_delimited_escape``\ :
     | unterminated delimited escape sequence
 * - ``3436``\ 
   - | ``constant_addresses_local_variable``\ :
     | constant contains address of a local variable
 * - ``3437``\ 
   - | ``struct_binding_consteval``\ :
     | a structured binding cannot be declared "consteval"
 * - ``3438``\ 
   - | ``module_import_conflict``\ :
     | *"entity"*\  conflicts with the imported declaration *entity-kind
       "entity"*\  (declared at line *xxxx*\ )
 * - ``3439``\ 
   - | ``char_too_wide_for_rep``\ :
     | character cannot be represented in the specified character type
 * - ``3440``\ 
   - | ``annotation_after_using``\ :
     | an annotation cannot appear in the context of a "using" attribute
       prefix
 * - ``3441``\ 
   - | ``annotation_must_have_literal_type``\ :
     | type *"type"*\  of annotation is not a literal type
 * - ``3442``\ 
   - | ``ext_vector_type_requires_bool_integral_floating_type``\ :
     | the "ext_vector_type" attribute applies only to bool, integer, or
       floating-point types
 * - ``3443``\ 
   - | ``multiple_union_designators``\ :
     | multiple designators into the same union are not permitted
 * - ``3444``\ 
   - | ``debug_show_location``\ :
     | test message
 * - ``3445``\ 
   - | ``microsoft_version_doesnt_support_cpp23_mode``\ :
     | the version of Microsoft being emulated must be at least 1943 to use
       "--ms_c++23"
 * - ``3446``\ 
   - | ``cl_invalid_cwd``\ :
     | invalid current working directory: *xxxx*\ 
 * - ``3447``\ 
   - | ``cleanup_attribute_in_constexpr_function``\ :
     | "cleanup" attribute within a constexpr function is not currently
       supported
 * - ``3448``\ 
   - | ``assume_statement_applies_to_null_statements``\ :
     | the "assume" attribute can only apply to a null statement
 * - ``3449``\ 
   - | ``assumption_failed``\ :
     | assumption failed
 * - ``3450``\ 
   - | ``variable_templates_is_cpp14``\ :
     | variable templates are a C++14 feature
 * - ``3451``\ 
   - | ``address_of_function_with_pass_object_size_attr``\ :
     | cannot take the address of a function with a parameter declared with
       the "pass_object_size" attribute
 * - ``3452``\ 
   - | ``all_arguments_must_have_same_type``\ :
     | all arguments must have the same type
 * - ``3453``\ 
   - | ``comparison_details``\ :
     | the final comparison was *xxxx*\  *xxxx*\  *xxxx*\ 
 * - ``3454``\ 
   - | ``too_many_arguments_provided_for_attribute``\ :
     | too many arguments for attribute *"xxxx"*\ 
 * - ``3455``\ 
   - | ``constexpr_bad_mantissa_string``\ :
     | mantissa string does not contain a valid number
 * - ``3456``\ 
   - | ``constexpr_float_error``\ :
     | floating-point error during constant evaluation
 * - ``3457``\ 
   - | ``inheriting_ctor_ignored_for_copy``\ :
     | inheriting constructor *entity-kind "entity"*\  ignored for
       copy/move-like operation
 * - ``3458``\ 
   - | ``cannot_get_file_size``\ :
     | cannot determine size of file *xxxx*\ 
 * - ``3459``\ 
   - | ``cannot_read_file``\ :
     | cannot read *xxxx*\ 
 * - ``3461``\ 
   - | ``unrec_embed_param``\ :
     | unrecognized parameter name
 * - ``3462``\ 
   - | ``dupl_embed_param``\ :
     | parameter specified more than once
 * - ``3463``\ 
   - | ``has_embed_not_in_if``\ :
     | __has_embed cannot appear outside #if
 * - ``3464``\ 
   - | ``setlocale_lc_numeric_failed``\ :
     | could not set LC_NUMERIC locale to C
 * - ``3465``\ 
   - | ``elifdef_not_enabled``\ :
     | elifdef and elifndef are not enabled in this mode and are ignored in
       text being skipped
 * - ``3466``\ 
   - | ``nonstandard_alias_declaration_context``\ :
     | an alias declaration is nonstandard in this context
 * - ``3467``\ 
   - | ``out_of_order_field_allocation_nonstandard``\ :
     | the target ABI may allocate nonstatic members in an order not
       matching their declaration order, which is nonstandard in C++23 and
       later
 * - ``3470``\ 
   - | ``module_unit_export_unexpected``\ :
     | the module declaration cannot be exported from this translation unit
       unless creating a module interface file
 * - ``3471``\ 
   - | ``module_unit_export_expected``\ :
     | the module declaration must be exported from this translation unit to
       create a module interface file
 * - ``3472``\ 
   - | ``cannot_write_module_none_declared``\ :
     | module file generation was requested but no module was declared in
       the translation unit
 * - ``3473``\ 
   - | ``candidate_constraints_failed``\ :
     | substituting *"<templ-args>"*\  for *entity-kind "entity"*\  failed
       constraints
 * - ``3474``\ 
   - | ``concept_not_satisfied``\ :
     | *entity-kind "entity"*\  not satisfied for *"<templ-args>"*\ 
 * - ``3475``\ 
   - | ``bad_embed_initializer``\ :
     | the #embed expansion is too long to initialize an entity of type
       *"type"*\ 
 * - ``3476``\ 
   - | ``bad_defined_pp_op``\ :
     | the "defined" operator is not permitted here
 * - ``3477``\ 
   - | ``ptr_to_mem_to_non_member``\ :
     | *entity-kind "entity"*\  is not a member of *"type"*\ 
 * - ``3478``\ 
   - | ``embed_narrowing``\ :
     | narrowing conversion to signed character in #embed data
 * - ``3479``\ 
   - | ``invalid_vector_of_bool_operator``\ :
     | operator is not permitted for "vector of bool" types
 * - ``3480``\ 
   - | ``constexpr_object_too_large``\ :
     | object too large for constant-evaluation
 * - ``3481``\ 
   - | ``self_referencing_temporary_object``\ :
     | self-referencing temporary object
 * - ``3482``\ 
   - | ``lambda_cannot_refer_to_local_entity_here``\ :
     | a lambda cannot refer to a local variable or init-capture in this
       context
 * - ``3483``\ 
   - | ``parameter_capture_conflict``\ :
     | a lambda parameter cannot hide an explicit capture
 * - ``3484``\ 
   - | ``template_parameter_capture_conflict``\ :
     | a lambda template parameter cannot hide an explicit capture
 * - ``3485``\ 
   - | ``insufficient_address_space``\ :
     | insufficient address space exists to process this translation unit
 * - ``3486``\ 
   - | ``undetermined_type``\ :
     | <undetermined type>
 * - ``3487``\ 
   - | ``undetermined_constant``\ :
     | <undetermined constant>
 * - ``3488``\ 
   - | ``undetermined_template``\ :
     | <undetermined template>
 * - ``3489``\ 
   - | ``bad_flt_config``\ :
     | the configured size of *xxxx*\  is too small for the specified number
       of mantissa + exponent bits
 * - ``3490``\ 
   - | ``expression``\ :
     | expression
 * - ``3491``\ 
   - | ``quoted_expression``\ :
     | <expression>
 * - ``3492``\ 
   - | ``unnamed``\ :
     | unnamed
 * - ``3493``\ 
   - | ``quoted_unnamed``\ :
     | <unnamed>
 * - ``3494``\ 
   - | ``error_type``\ :
     | <error-type>
 * - ``3495``\ 
   - | ``unknown_type``\ :
     | <unknown-type>
 * - ``3496``\ 
   - | ``something``\ :
     | <something>
 * - ``3497``\ 
   - | ``null_type``\ :
     | <null-type>
 * - ``3498``\ 
   - | ``no_init``\ :
     | <no-init>
 * - ``3499``\ 
   - | ``zero_init``\ :
     | <zero-init>
 * - ``3500``\ 
   - | ``bitwise_copy_of``\ :
     | bitwise copy of: 
 * - ``3501``\ 
   - | ``bitwise_copy``\ :
     | <bitwise-copy>
 * - ``3502``\ 
   - | ``class_result_via_ctor``\ :
     | class result via ctor: 
 * - ``3503``\ 
   - | ``constructor_call``\ :
     | <constructor-call>
 * - ``3504``\ 
   - | ``null_expression``\ :
     | <NULL expression>
 * - ``3505``\ 
   - | ``quoted_error``\ :
     | <error>
 * - ``3506``\ 
   - | ``null_routine``\ :
     | <NULL routine>
 * - ``3507``\ 
   - | ``quoted_default``\ :
     | <default>
 * - ``3508``\ 
   - | ``parameter_number``\ :
     | parameter #
 * - ``3509``\ 
   - | ``one_level_up``\ :
     | (one level up)
 * - ``3510``\ 
   - | ``levels_up``\ :
     | levels up
 * - ``3511``\ 
   - | ``dynamic_init``\ :
     | dynamic-init: 
 * - ``3512``\ 
   - | ``error_constant``\ :
     | <error-constant>
 * - ``3513``\ 
   - | ``stack_offset_of``\ :
     | stack-offset-of:
 * - ``3514``\ 
   - | ``implicit_element``\ :
     | <implicit element> 
 * - ``3515``\ 
   - | ``repetitions_of``\ :
     | repetitions of 
 * - ``3516``\ 
   - | ``type_code_integer``\ :
     | integer
 * - ``3517``\ 
   - | ``type_code_enum``\ :
     | enum
 * - ``3518``\ 
   - | ``type_code_scoped_enum``\ :
     | scoped enum
 * - ``3519``\ 
   - | ``type_code_arithmetic``\ :
     | arithmetic
 * - ``3520``\ 
   - | ``type_code_non_bool_arithmetic``\ :
     | non-bool arithmetic
 * - ``3521``\ 
   - | ``type_code_pointer``\ :
     | pointer
 * - ``3522``\ 
   - | ``type_code_nullptr_type``\ :
     | nullptr type
 * - ``3523``\ 
   - | ``type_code_handle``\ :
     | handle
 * - ``3524``\ 
   - | ``type_code_handle_to_CLI_array``\ :
     | handle-to-CLI-array
 * - ``3525``\ 
   - | ``type_code_pointer_to_object``\ :
     | pointer-to-object
 * - ``3526``\ 
   - | ``type_code_pointer_to_function``\ :
     | pointer-to-function
 * - ``3527``\ 
   - | ``type_code_pointer_to_member``\ :
     | pointer-to-member
 * - ``3528``\ 
   - | ``type_code_bool``\ :
     | bool
 * - ``3529``\ 
   - | ``type_code_bool_equivalent``\ :
     | bool-equivalent
 * - ``3530``\ 
   - | ``type_code_class``\ :
     | class
 * - ``3531``\ 
   - | ``volatile_inc_deprecated``\ :
     | a volatile operand to an increment expression is deprecated
 * - ``3532``\ 
   - | ``volatile_dec_deprecated``\ :
     | a volatile operand to a decrement expression is deprecated
 * - ``3533``\ 
   - | ``indeterminate_not_on_first_decl``\ :
     | *entity-kind "entity"*\  previously declared without the
       "indeterminate" attribute
 * - ``3534``\ 
   - | ``default_constructor_is_explicit``\ :
     | the default constructor for *"type"*\  is explicit
 * - ``3535``\ 
   - | ``ifc_definition_load_failure``\ :
     | failed to load the definition of *entity-kind "entity"*\  in *module
       "module name"*\ 
 * - ``3536``\ 
   - | ``ifc_initializer_load_failure``\ :
     | failed to load the initializer for *entity-kind "entity"*\  in
       *module "module name"*\ 
 * - ``3537``\ 
   - | ``base_class_of_class_type_with_typedef_name``\ :
     | a class with a typedef name for linkage purposes cannot have a base
       class
 * - ``3538``\ 
   - | ``member_function_of_class_type_with_typedef_name``\ :
     | a class with a typedef name for linkage purposes cannot have a member
       function
 * - ``3539``\ 
   - | ``member_type_of_class_type_with_typedef_name``\ :
     | a class with a typedef name for linkage purposes cannot have a nested
       type, other than an enumeration type or a non-closure class type
 * - ``3540``\ 
   - | ``lambda_in_class_type_with_typedef_name``\ :
     | a class with a typedef name for linkage purposes cannot contain a
       lambda expression
 * - ``3541``\ 
   - | ``field_init_in_class_type_with_typedef_name``\ :
     | a class with a typedef name for linkage purposes cannot have a
       nonstatic data member with a default member initializer
 * - ``3542``\ 
   - | ``static_data_member_not_allowed_in_unnamed_class``\ :
     | a static data member declaration is not allowed in an unnamed class
 * - ``3543``\ 
   - | ``initializer_addresses_dllimport_variable``\ :
     | initializer result addresses a dllimport variable
 * - ``3544``\ 
   - | ``cannot_be_specialized``\ :
     | template with "no_specializations" attribute cannot be specialized
 * - ``3545``\ 
   - | ``static_dimension_nonstandard``\ :
     | "static" is nonstandard here
 * - ``3546``\ 
   - | ``enum_previously_declared_without_explicit_base``\ :
     | *entity-kind "entity"*\  (declared at line *xxxx*\ ) was previously
       declared without an explicit enum base
 * - ``3547``\ 
   - | ``missing_typename``\ :
     | missing "typename" is nonstandard here
 * - ``3548``\ 
   - | ``abbreviated_deduction_guide``\ :
     | abbreviated function template syntax is nonstandard for deduction
       guides
 * - ``3549``\ 
   - | ``explicit_this_is_cpp23``\ :
     | explicit "this" parameters are a C++23 feature
 * - ``3550``\ 
   - | ``array_designator_for_deduced_context``\ :
     | array designators are not supported in template deduction contexts
 * - ``3551``\ 
   - | ``second_argument_wrong_shape``\ :
     | the second argument must be an integer type with the same shape (a
       scalar or vector type) as the first argument
 * - ``3552``\ 
   - | ``vector_of_boolean_required``\ :
     | argument must be a vector of boolean types
 * - ``3553``\ 
   - | ``pointer_to_scalar_required``\ :
     | argument must be a pointer to a scalar type
 * - ``3554``\ 
   - | ``incorrect_masked_type``\ :
     | incorrect argument type (should be *"type"*\ )
 * - ``3555``\ 
   - | ``vector_type_with_size_is_required``\ :
     | a vector type (with the same number of elements as the first
       argument) is required
 * - ``3556``\ 
   - | ``struct_binding_packs_is_cpp26``\ :
     | structured binding packs are a C++26 feature
 * - ``3557``\ 
   - | ``non_template_structured_binding_pack``\ :
     | a structured binding pack can only be declared inside a template
 * - ``3558``\ 
   - | ``multiple_structured_binding_packs``\ :
     | cannot declare multiple packs in a structured binding declaration
 * - ``3559``\ 
   - | ``cl_cpp26_requires_parse_nonclass_templates``\ :
     | The "--no_parse_templates" option cannot be used in C++26 mode
 * - ``3560``\ 
   - | ``type_pack_element_cannot_match``\ :
     | a type pack element cannot match *entity-kind "entity"*\ 
 * - ``3561``\ 
   - | ``c23_old_style_param_id``\ :
     | identifier-only parameters can only be used in old-style function
       definitions, which are nonstandard since C23
 * - ``3562``\ 
   - | ``cl_require_func_prototypes_option_only_in_C``\ :
     | option "no_require_func_prototypes" can be used only when compiling C
 * - ``3563``\ 
   - | ``address_of_overload_set_implicit_obj_member_selected``\ :
     | overload resolution for an address-of-overload-set call may not
       select implicit-object member *entity-kind "entity"*\  (declared at
       line *xxxx*\ )
 * - ``3564``\ 
   - | ``pack_indexing_is_cpp26``\ :
     | pack indexing is a C++26 feature
 * - ``3565``\ 
   - | ``pack_index_out_of_bounds``\ :
     | pack index exceeds number of pack elements
 * - ``3566``\ 
   - | ``alias_ctad_is_cpp20``\ :
     | class template argument deduction for alias templates is a C++20
       feature
 * - ``3567``\ 
   - | ``bitint_width_too_large``\ :
     | _BitInt(*n*\ ) is invalid; the maximum integer width is *n*\ 
 * - ``3568``\ 
   - | ``signed_bitint_width_too_small``\ :
     | _BitInt(*n*\ ) is invalid; the minimum signed _BitInt width is *n*\ 
 * - ``3569``\ 
   - | ``unsigned_bitint_width_too_small``\ :
     | unsigned _BitInt(*n*\ ) is invalid; the minimum unsigned _BitInt
       width is *n*\ 
 * - ``3570``\ 
   - | ``bitint_enum_base_not_allowed``\ :
     | _BitInt types cannot be the underlying type of an enumeration type
 * - ``3571``\ 
   - | ``deduction_guide_redeclaration``\ :
     | redeclaration of deduction guide *"type"*\  (previous declaration at
       line *xxxx*\ )
 * - ``3572``\ 
   - | ``create_pch_file_not_created``\ :
     | precompiled header file *"xxxx"*\  was not created
 * - ``3576``\ 
   - | ``non_c_like_typedef_for_linkage``\ :
     | anonymous non-C-like type given name for linkage purposes by typedef
       or alias declaration
 * - ``3577``\ 
   - | ``non_c_like_because_of_base_class``\ :
     | type is not C-like because of this base class
 * - ``3578``\ 
   - | ``non_c_like_because_of_default_member_init``\ :
     | type is not C-like because of this default member initializer
 * - ``3579``\ 
   - | ``non_c_like_because_of_friend``\ :
     | type is not C-like because of this friend declaration
 * - ``3580``\ 
   - | ``non_c_like_because_of_member``\ :
     | type is not C-like because of this member declaration
 * - ``3581``\ 
   - | ``non_c_like_because_of_lambda``\ :
     | type is not C-like because of this lambda expression
 * - ``3582``\ 
   - | ``enum_qualifier_is_cpp11``\ :
     | an enum-type name qualifier is a C++11 feature
 * - ``3583``\ 
   - | ``unrestricted_unions_is_cpp11``\ :
     | unrestricted unions are a C++11 feature
 * - ``3584``\ 
   - | ``pack_expansion_for_non_pack_alias_param``\ :
     | a pack expansion cannot be used as an argument for non-pack
       *entity-kind "entity"*\  of *entity-kind "entity"*\ 
 * - ``3585``\ 
   - | ``uniterable_reflection_range``\ :
     | the range passed to the metafunction cannot be iterated
 * - ``3587``\ 
   - | ``interpreter_message_remark``\ :
     | *xxxx*\  *xxxx*\ 
 * - ``3588``\ 
   - | ``interpreter_message_warning``\ :
     | *xxxx*\  *xxxx*\ 
 * - ``3589``\ 
   - | ``interpreter_message_error``\ :
     | *xxxx*\  *xxxx*\ 
 * - ``3590``\ 
   - | ``constexpr_diag_arg_count``\ :
     | a call to "\__builtin_constexpr_diag" requires three arguments
 * - ``3591``\ 
   - | ``constexpr_diag_bad_level``\ :
     | the first argument in a call to "\__builtin_constexpr_diag" must be 0
       (remark), 1 (warning), 2 (error), 16 (remark at caller), 17 (warning
       at caller), or 18 (error at caller)
 * - ``3592``\ 
   - | ``constexpr_diag_bad_string``\ :
     | a string literal or an object with the representation of
       std::string_view is required for the tag and the message in a call to
       "\__builtin_constexpr_diag"
 * - ``3593``\ 
   - | ``constexpr_diag_bad_tag``\ :
     | the tag in a call to "\__builtin_constexpr_diag" may contain only
       letters, digits, and underscores
 * - ``3594``\ 
   - | ``cl_invalid_constexpr_diag_tag``\ :
     | invalid tag in constexpr diagnostic control option: *xxxx*\ 
 * - ``3595``\ 
   - | ``token_sequence_needs_double_caret``\ :
     | a token sequence must be introduced by "^^"
 * - ``3596``\ 
   - | ``exp_interpolator``\ :
     | expected an interpolator ("\[", "\[:", "\{", "\val", or "\str")
 * - ``3597``\ 
   - | ``interpolated_id_is_not_identifier``\ :
     | *"xxxx"*\  does not spell an identifier
 * - ``3598``\ 
   - | ``interpolated_operand_is_not_token_sequence``\ :
     | the operand of a "\{...}" interpolator is not a token sequence
 * - ``3599``\ 
   - | ``tag_redefined_differently``\ :
     | this declaration of *entity-kind "entity"*\  does not declare the
       same type as the declaration at line *xxxx*\ 
 * - ``3600``\ 
   - | ``bad_gcc_unroll_pragma_value``\ :
     | the argument of "#pragma GCC unroll" must be a non-negative integral
       constant less than 65535
