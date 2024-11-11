#ifndef INCLUDE_CONTEXTUALPLANNER_EXPORTSYMBOLS_MACRO_HPP
#define INCLUDE_CONTEXTUALPLANNER_EXPORTSYMBOLS_MACRO_HPP

#if defined _WIN32 || defined __CYGWIN__
#  define CONTEXTUALPLANNER_LIB_API_EXPORTS(LIBRARY_NAME) __declspec(dllexport)
#  define CONTEXTUALPLANNER_LIB_API(LIBRARY_NAME)         __declspec(dllimport)
#elif __GNUC__ >= 4
#  define CONTEXTUALPLANNER_LIB_API_EXPORTS(LIBRARY_NAME) __attribute__ ((visibility("default")))
#  define CONTEXTUALPLANNER_LIB_API(LIBRARY_NAME)        __attribute__ ((visibility("default")))
#else
#  define CONTEXTUALPLANNER_LIB_API_EXPORTS(LIBRARY_NAME)
#  define CONTEXTUALPLANNER_LIB_API(LIBRARY_NAME)
#endif

#endif // INCLUDE_CONTEXTUALPLANNER_EXPORTSYMBOLS_MACRO_HPP