//type:fp
//options:--c++20
//options_all:-w --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

#if TEST_NUMBER == 1

struct A
{
  struct B
  {
    static void g();
    int j;
  };
};

void f()
{
  struct A
  {
    struct B
    {
      static void f()
      { }

      int i;
    };

    void h()
    {
      B::f();
      auto p = &B::i;
    }
  };

  {
    A::B b;
    A::B::f();
    auto p = &A::B::i;
  }

  {
    ::A::B b;
    ::A::B::g();
    auto p = &::A::B::j;
  }
}

#endif

/* @filter
file-scope name-reference[is_global_qualified_name==TRUE]
  num_template_arguments, qualifier, is_template_id, is_global_qualified_name
file-scope expr-node
  kind, type, qualifier, qualifier.class_type, name, constant, operands,
  routine, operation.kind, position.seq, is_global_qualified_name
func-scope expr-node
  kind, type, qualifier, constant, operands, routine, operation.kind,
  position.seq, is_global_qualified_name
file-scope constant
  source_corresp/name, source_corresp/decl_position.seq, kind, type
func-scope constant
  source_corresp/decl_position.seq, kind, type
file-scope name-qualifier
  qualifier.class_type, name, is_class
func-scope name-qualifier
  qualifier.class_type, name, is_class, is_global_qualified_name
file-scope type[kind==tk_struct]
  source_corresp/name, source_corresp/decl_position.seq, kind, extra_info
file-scope type[kind==name_qualifier]
  kind, typeref_type, qualifier, is_global_qualified_name
func-scope type[kind==name_qualifier]
  source_corresp/name, source_corresp/decl_position.seq, kind, declared_type,
  typeref_type, extra_info, qualifier
func-scope type[kind==tk_struct]
  source_corresp/name, source_corresp/decl_position.seq, kind, declared_type,
  typeref_type, extra_info, qualifier
 */
