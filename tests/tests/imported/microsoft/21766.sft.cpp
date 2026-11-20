//options_all:--microsoft_version 1923
#if defined(__cplusplus) && (__cplusplus >= 201402L)
    /* use standard [[attribute]] syntax */
#warning use standard [[attribute]] syntax
#elif __has_extension(attribute_deprecated_with_message)
    /* use __attribute__((__deprecated__(“message”))) syntax */
#error This doesn't work in MSVC
#elif defined(_MSC_VER)
    /* use __declspec(deprecated(“message”)) syntax */
#warning use __declspec syntax
#endif
