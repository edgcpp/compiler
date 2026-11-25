//type:fp
//options:--c++17:--c++17 --gn 130100:--c++17 --clang_version 160000:--ms_c++17 --microsoft_version 1927:--ms_c++17 --microsoft_version 1936
//options_all:-tused --no_il_lower --il_display
//filter:edg-il-filter
//script:edg-first-stage-il

namespace non_tmpl
{
  int i = 1;
  static int si = 2;
  const int ci = 3;
  const volatile int cvi = 4;
  inline int ii = 5;
  inline const int ici = 6;
  extern int ei;
  extern const int eci;

  extern const int reci;
  const int reci = 7;

  namespace
  {
    int ui = 8;
  }

  struct C
  {
    static int msi;
    static inline int msii = 15;
  };

  int C::msi = 12;
}

namespace tmpl
{
  template<typename T>
  int ti = 1;

  template<typename T>
  static int tsi = 2;

  template<typename T>
  const int tci = 3;

  template<typename T>
  const volatile int tcvi = 4;

  template<typename T>
  inline int tii = 5;

  template<typename T>
  inline const int tici = 6;

  template<typename T>
  extern int tei;

  template<typename T>
  extern const int teci;

  namespace
  {
    template<typename T>
    int tui = 8;
  }

  template<typename T>
  int ti<T *> = 1;

  template<typename T>
  static int tsi<T *> = 2;

  template<typename T>
  const int tci<T *> = 3;

  template<typename T>
  const volatile int tcvi<T *> = 4;

  template<typename T>
  inline int tii<T *> = 5;

  template<typename T>
  inline const int tici<T *> = 6;

  template<typename T>
  extern int tei<T *>;

  template<typename T>
  extern const int teci<T *>;


  template<>
  int ti<void *> = 1;

  template<>
  int tsi<void *> = 2;

  template<>
  int tci<void *> = 3;

  template<>
  const volatile int tcvi<void *> = 4;

  template<>
  inline int tii<void *> = 5;

  template<>
  inline const int tici<void *> = 6;

  template<>
  int tei<void *> = 7;

  template<>
  const int teci<void *> = 8;


  int f()
  {
    return ti<int> + tsi<int> + tci<int> + tcvi<int> + tii<int> + tici<int> + tei<int> + teci<int> + tui<int> +
           ti<int *> + tsi<int *> + tci<int *> + tcvi<int *> + tii<int *> + tici<int *> + tei<int *> + teci<int *> +
           ti<void *> + tsi<void *> + tci<void *> + tcvi<void *> + tii<void *> + tici<void *> + tei<void *> + teci<void *>;
  }
}

/* @filter
file-scope variable
  source_corresp/name, source_corresp/decl_position.seq,
  source_corresp/name_linkage, storage_class, declared_storage_class,
  is_template_variable, is_specialized, is_prototype_instantiation,
  is_nonreal
 */
