//type:fp
//remark:[GH #19] Assertion failure on an empty diag_suppress pragma
// 10/3/26  [GH #19]
//
// An empty #pragma diag_suppress used to read past the directive and assert
// in process_immediate_pragmas.  The functions below must still be compiled.

#pragma diagnostic push
#pragma diag_suppress
int get_free_no_warn()
{
  int a;
  return a;
}
#pragma diagnostic pop

int get_free_warn()
{
  int b;
  return b;
}

#pragma diag_suppress =
int after_bare_equals()
{
  int c;
  return c;
}

#pragma diag_suppress 123,
int after_trailing_comma()
{
  int d;
  return d;
}

#pragma diag_suppress = 123
int after_valid_pragma()
{
  int e;
  return e;
}
#pragma diag_default 123
