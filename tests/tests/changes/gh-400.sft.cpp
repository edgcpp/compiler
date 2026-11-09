//type:fp
//options:--c++20
//remark:[GH #400] Constant evaluation destroys array elements in decreasing
//remark:order of their index

struct S {
  int* log;
  int id;
  constexpr ~S() { *log = *log * 10 + id; }
};

constexpr int local_array() {
  int log = 0;
  {
    S a[3] = {{&log, 1}, {&log, 2}, {&log, 3}};
  }
  return log;
}
static_assert(local_array() == 321);  // Previously failed.

constexpr int multidim_array() {
  int log = 0;
  {
    S a[2][2] = {{{&log, 1}, {&log, 2}}, {{&log, 3}, {&log, 4}}};
  }
  return log;
}
static_assert(multidim_array() == 4321);  // Previously failed.

struct M { S m[3]; };
constexpr int member_array() {
  int log = 0;
  {
    M x = {{{&log, 1}, {&log, 2}, {&log, 3}}};
  }
  return log;
}
static_assert(member_array() == 321);  // Previously failed.

struct N {
  S m[2];
  constexpr N(int* log) : m{{log, 1}, {log, 2}} {}
  constexpr ~N() { *m[0].log = *m[0].log * 10 + 9; }
};
constexpr int member_array_user_dtor() {
  int log = 0;
  {
    N x(&log);
  }
  return log;
}
static_assert(member_array_user_dtor() == 921);  // Previously failed.

constexpr int array_delete() {
  int log = 0;
  S* p = new S[3]{{&log, 1}, {&log, 2}, {&log, 3}};
  delete[] p;
  return log;
}
static_assert(array_delete() == 321);  // Previously failed.

constexpr int single_delete() {
  int log = 0;
  S* p = new S{&log, 5};
  delete p;
  return log;
}
static_assert(single_delete() == 5);
