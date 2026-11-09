//type:fp
//options:--c++20
//options_all:--no_il_lower --il_display
//filter:awk -v RS='' -v ORS='\n\n' '/^file-scope (expr-node@.*\nkind: +enk_constant|name-qualifier@)/' | grep -E -e '^(is_class|is_global_qualified_name|from_prototype_instantiation|used_in_primary_declarator|qualifier|qualifier.class_type|name|kind|constant|previous_qualifier|position[.]seq):' -e '^ +(FALSE|TRUE)$' -e '^file-scope ' -e '^$' | edg-enumerate-il-addrs

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
