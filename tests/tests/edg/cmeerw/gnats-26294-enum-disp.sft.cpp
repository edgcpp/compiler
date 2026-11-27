//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

struct A
{
  enum E
  {
    E0
  };
};

using AA = A;

int i =
  A::E0 +
  ::A::E0 +
  A::E::E0 +
  ::A::E::E0 +
  AA::E0 +
  ::AA::E0 +
  AA::E::E0 +
  ::AA::E::E0;

/* @filter
file-scope name-reference[is_global_qualified_name==TRUE]
  num_template_arguments, qualifier, is_global_qualified_name,
  from_prototype_instantiation, used_in_primary_declarator, is_template_id
file-scope expr-node[kind==enk_constant]
  kind, qualifier, constant, position.seq, is_global_qualified_name,
  from_prototype_instantiation, used_in_primary_declarator
file-scope name-qualifier
  qualifier.class_type, previous_qualifier, name, is_class
 */
