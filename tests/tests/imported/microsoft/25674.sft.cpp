//options_all:-W --ms_c++14 -P
#define ARG_5(_0, _1, _2, _3, _4, ...) _4

#define ENUM_TEXT_RAW(EnumName, ...) \
    using EnumName = TEnumText<__VA_ARGS__>
#define ENUM_TEXT_SELECTOR_1(EnumName, EnumBase) \
    using EnumName = TSmartEnumText<EnumBase>
#define ENUM_TEXT_SELECTOR_2(EnumName, EnumBase, MaxValue) \
    using EnumName = TSmartEnumText<EnumBase, EnumBase::MaxValue>
#define ENUM_TEXT_SELECTOR_3(EnumName, EnumBase, MinValue, MaxValue) \
    using EnumName = TSmartEnumText<EnumBase, EnumBase::MinValue, EnumBase::MaxValue>

#define ENUM_TEXT_FUNC_RECOMPOSER(argsWithParentheses) ARG_5 argsWithParentheses
#define ENUM_TEXT_MACRO_CHOOSER(...) \
    ENUM_TEXT_FUNC_RECOMPOSER((__VA_ARGS__, ENUM_TEXT_RAW, ENUM_TEXT_SELECTOR_3, ENUM_TEXT_SELECTOR_2, ENUM_TEXT_SELECTOR_1, ))

#define ENUM_NAMED_DEF(EnumName, ...) ENUM_TEXT_MACRO_CHOOSER(__VA_ARGS__)(EnumName, __VA_ARGS__)

enum ENProjectTypeGroup : unsigned char { E_Mathematic };
ENUM_NAMED_DEF(EProjectTypeGroup, ENProjectTypeGroup, E_Mathematic);
