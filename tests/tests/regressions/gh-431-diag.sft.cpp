//type:fn
//options_all:-tused
//options:--c++11:--gnu_version 150200:--clang_version 220100:--ms_c++20 --microsoft_version 1950

namespace direct_call
{
  template<class T> struct C
  {
    void f()
    {
      static_assert(sizeof(T) == 0, "");
    }
  };

  extern template struct C<int>;

  /* An extern template declaration must not suppress inline body checking. */
  void g(C<int>& c)
  {
    c.f();
  }
}

namespace direct_call_inline_out_of_class
{
  template<class T> struct C
  {
    void f();
  };

  template<class T> inline void C<T>::f()
  {
    static_assert(sizeof(T) == 0, "");
  }

  extern template struct C<int>;

  void g(C<int>& c)
  {
    c.f();
  }
}

namespace explicit_instantiation
{
  template<class T> struct C
  {
    virtual void f()
    {
      static_assert(sizeof(T) == 0, "");
    }
  };

  extern template struct C<int>;
  /* A subsequent explicit instantiation must instantiate the virtual member. */
  template struct C<int>;
}

namespace explicit_instantiation_out_of_class
{
  template<class T> struct C
  {
    virtual void f();
  };

  template<class T> void C<T>::f()
  {
    static_assert(sizeof(T) == 0, "");
  }

  extern template struct C<int>;
  template struct C<int>;
}

namespace explicit_instantiation_inline_out_of_class
{
  template<class T> struct C
  {
    virtual void f();
  };

  template<class T> inline void C<T>::f()
  {
    static_assert(sizeof(T) == 0, "");
  }

  extern template struct C<int>;
  template struct C<int>;
}

namespace derived_vtable
{
  template<class T> struct C
  {
    virtual void f()
    {
      static_assert(sizeof(T) == 0, "");
    }
  };

  extern template struct C<int>;
  struct D : C<int>
  {};

  /* The derived vtable still needs the inherited virtual member. */
  D *g()
  {
    return new D();
  }
}

namespace derived_vtable_inline_out_of_class
{
  template<class T> struct C
  {
    virtual void f();
  };

  template<class T> inline void C<T>::f()
  {
    static_assert(sizeof(T) == 0, "");
  }

  extern template struct C<int>;
  struct D : C<int>
  {};

  D *g()
  {
    return new D();
  }
}

namespace internal_linkage
{
  namespace
  {
    struct Local {};
  }

  template<class T> struct C
  {
    virtual void f()
    {
      static_assert(sizeof(T) == 0, "");
    }
  };

  extern template struct C<Local>;
  /* A specialization with internal linkage still gets a local vtable. */
  C<Local> c;
}

namespace internal_linkage_inline_out_of_class
{
  namespace
  {
    struct Local {};
  }

  template<class T> struct C
  {
    virtual void f();
  };

  template<class T> inline void C<T>::f()
  {
    static_assert(sizeof(T) == 0, "");
  }

  extern template struct C<Local>;
  C<Local> c;
}

namespace internal_linkage_template
{
  namespace
  {
    template<class T> struct C
    {
      virtual void f()
      {
        static_assert(sizeof(T) == 0, "");
      }
    };

    extern template struct C<int>;

    /* The template itself has internal linkage, even with an external arg. */
    C<int> c;
  }

  void *g()
  {
    return &c;
  }
}

namespace internal_linkage_nontype_argument
{
  static int value;

  template<int *P> struct C
  {
    virtual void f()
    {
      static_assert(P == nullptr, "");
    }
  };

  extern template struct C<&value>;

  /* A pointer to an internal-linkage object makes the instance local too. */
  C<&value> c;
}

namespace direct_virtual_call
{
  template<class T> struct C
  {
    virtual void f()
    {
      static_assert(sizeof(T) == 0, "");
    }
  };

  extern template struct C<int>;

  /* Suppressing the vtable must not suppress a qualified inline call. */
  void g(C<int>& c)
  {
    c.C<int>::f();
  }
}

namespace direct_virtual_call_inline_out_of_class
{
  template<class T> struct C
  {
    virtual void f();
  };

  template<class T> inline void C<T>::f()
  {
    static_assert(sizeof(T) == 0, "");
  }

  extern template struct C<int>;

  void g(C<int>& c)
  {
    c.C<int>::f();
  }
}

namespace specialized_nested_class
{
  template<class T> struct Base
  {
    virtual void f()
    {
      static_assert(sizeof(T) == 0, "");
    }
  };

  extern template struct Base<int>;

  template<class T> struct C
  {
    struct D;
  };

  template<> struct C<int>::D : Base<int> {};

  /* Propagation of suppression must not suppress a specialized class's
     vtable or its inherited virtual functions. */
  extern template struct C<int>;
  C<int>::D d;
}

namespace nontemplate_nested_class
{
  template<class T> struct Base
  {
    virtual void f()
    {
      static_assert(sizeof(T) == 0, "");
    }
  };

  extern template struct Base<int>;

  template<class T> struct C
  {
    struct D;
  };

  template<> struct C<int>::D
  {
    struct E : Base<int> {};
  };

  /* Suppression also propagates to E, which is not a template instance. */
  extern template struct C<int>;
  C<int>::D::E e;
}

namespace unsuppressed_specialization
{
  template<int N> struct C
  {
    virtual void f()
    {
      static_assert(N != 2, "");
    }
  };

  extern template struct C<1>;

  /* Suppression applies only to the named specialization. */
  C<2> c;
}

#if defined(__GNUC__) && !defined(__clang__)
namespace inline_template
{
  template<class T> struct C
  {
    virtual void f()
    {
      static_assert(sizeof(T) == 0, "");
    }
  };

  extern template struct C<int>;
  /* GNU inline template explicitly requests a local vtable. */
  inline template struct C<int>;
}

namespace inline_template_inline_out_of_class
{
  template<class T> struct C
  {
    virtual void f();
  };

  template<class T> inline void C<T>::f()
  {
    static_assert(sizeof(T) == 0, "");
  }

  extern template struct C<int>;
  inline template struct C<int>;
}
#endif
