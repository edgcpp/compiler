//options_all:--microsoft --concepts
#define CPP_PP_CAT_(X, ...) X ## __VA_ARGS__
#define CPP_PP_CAT(X, ...) CPP_PP_CAT_(X, __VA_ARGS__)

#define CPP_PP_EVAL_(X, ARGS) X ARGS
#define CPP_PP_EVAL(X, ...) CPP_PP_EVAL_(X, (__VA_ARGS__))

#define CPP_PP_EAT(...)

#define CPP_PP_FIRST(LIST) CPP_PP_FIRST_ LIST
#define CPP_PP_FIRST_(...) __VA_ARGS__ CPP_PP_EAT

#define CPP_TEMPLATE_AUX_(...) CPP_PP_CAT(CPP_TEMPLATE_AUX_, 1)(__VA_ARGS__)
#define CPP_TEMPLATE_AUX_1(DECL, ...) concept CPP_CONCEPT_NAME_(DECL) = __VA_ARGS__

#define CPP_CONCEPT_NAME_(DECL) CPP_PP_EVAL(CPP_PP_FIRST, DECL)

template <class T>
CPP_TEMPLATE_AUX_((range)(), true);
