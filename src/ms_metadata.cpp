/*
Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
Exceptions.
See https://edgcpp.org/LICENSE.txt for license information.
SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

/*

ms_metadata.cpp -- reading of C++/CLI metadata from assemblies.

This is a C++ file that relies on Microsoft Windows APIs.
As a result, it can only be compiled on a Windows platform.
In addition, the C++ code makes use of C++0x features, so it must be
compiled by at least the Microsoft VC10 compiler or version 4.2 of the
EDG front end.

If you have CPPCLI_ENABLING_POSSIBLE set to FALSE (the default), you
don't need this file.  You don't need to compile it, and you don't
need to link it in.  You can stick with the traditional C-only build
process.

Note that this file uses the alink.h include file.  It is included with
installations of Visual Studio (beginning with Visual Studio 2012) as
part of the Windows 8 (and newer) SDKs.  The file can be found in:

  \Program Files(x86)\Windows Kits\8.x\Include\um\alink.h

where 8.x is currently either 8.0 or 8.1.
*/

#include "basic_hdrs.h"

#if CPPCLI_ENABLING_POSSIBLE && !defined(_lint)

#include "fe_common.h"

#ifdef _MSC_VER
#include <windows.h>
#else /* !defined(_MSC_VER) */
#if !defined(_AMD64_) && (defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__))
#define _AMD64_
#elif !defined(_ARM64_) && (defined(_M_ARM64) || defined(__aarch64__))
#define _ARM64_
#elif !defined(_X86_) && (defined(_M_IX86) || defined(__i386__))
#define _X86_
#elif !defined(_ARM_) && (defined(_M_ARM) || defined(__arm__))
#define _ARM_
#endif
#include <windef.h>
#include <winbase.h>
#include <winnls.h>
#include <unknwn.h>
#include <objbase.h>
#include <oleauto.h>
#endif /* ifdef _MSC_VER */
#include <metahost.h>
#include <cor.h>
#include <fusion.h>
#include "alink.h"

#include <string>
#include <map>
#include <set>
#include <vector>
#include <stack>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <fstream>
#include <memory>
#include <locale>
#include <type_traits>
#include <atlcomcli.h>

using namespace std;

/* Conditionally open the "edg" namespace. */
BEGIN_EDG_NAMESPACE


/*
Some versions of MSVC have std::make_unique, but others don't.  To enable
compilation with slightly older versions, we use our own "make_unique_ptr"
instead.  This version does not require variadic template support.
*/
template<class T> inline
typename enable_if<!is_array<T>::value, unique_ptr<T> >::type
make_unique_ptr() {
  return unique_ptr<T>(new T());
}  /* make_unique_ptr() */

template<class T, class P1> inline
typename enable_if<!is_array<T>::value, unique_ptr<T> >::type
make_unique_ptr(P1&& p1) {
  return unique_ptr<T>(new T(forward<P1>(p1)));
}  /* make_unique_ptr(P1&&) */

template<class T, class P1, class P2> inline
typename enable_if<!is_array<T>::value, unique_ptr<T> >::type
make_unique_ptr(P1&& p1, P2&& p2) {
  return unique_ptr<T>(new T(forward<P1>(p1), forward<P2>(p2)));
}  /* make_unique_ptr(P1&&, P2&&) */

template<class T, class P1, class P2, class P3> inline
typename enable_if<!is_array<T>::value, unique_ptr<T> >::type
make_unique_ptr(P1&& p1, P2&& p2, P3&& p3) {
  return unique_ptr<T>(new T(forward<P1>(p1), forward<P2>(p2),
                             forward<P3>(p3)));
}  /* make_unique_ptr(P1&&, P2&&, P3&&) */

template<class T, class P1, class P2, class P3, class P4> inline
typename enable_if<!is_array<T>::value, unique_ptr<T> >::type
make_unique_ptr(P1&& p1, P2&& p2, P3&& p3, P4&& p4) {
  return unique_ptr<T>(new T(forward<P1>(p1), forward<P2>(p2), forward<P3>(p3),
                             forward<P4>(p4)));
}  /* make_unique_ptr(P1&&, P2&&, P3&&, P4&&) */

template<class T, class P1, class P2, class P3, class P4, class P5> inline
typename enable_if<!is_array<T>::value, unique_ptr<T> >::type
make_unique_ptr(P1&& p1, P2&& p2, P3&& p3, P4&& p4, P5&& p5) {
  return unique_ptr<T>(new T(forward<P1>(p1), forward<P2>(p2), forward<P3>(p3),
                             forward<P4>(p4), forward<P5>(p5)));
}  /* make_unique_ptr(P1&&, P2&&, P3&&, p4&&, P5&&) */



STATIC_THREAD bool is_cppcx_metadata;

#define WIDEN2(x) L ## x
#define WIDEN(x) WIDEN2(x)
#define MAKE_CLASS_STRING(name) \
               (is_cppcx_metadata ? L"Platform::" WIDEN(#name) \
                                  : L"System::" WIDEN(#name))
#define CLI_NAMESPACE (is_cppcx_metadata ? L"default::" : L"cli::")
#define ATTRIBUTE_ATTRIBUTE (is_cppcx_metadata ? \
                             L"Platform::Metadata::Attribute" : \
                             L"System::Attribute")
#define ATTRIBUTE_INTERFACE (is_cppcx_metadata ? \
                             nullptr : \
                             L"System::Runtime::InteropServices::_Attribute")
#define ATTRIBUTE_USAGE_ATTRIBUTE (is_cppcx_metadata ? \
                 L"Windows::Foundation::Metadata::AttributeUsageAttribute" : \
                 L"System::AttributeUsageAttribute")
#define ATTRIBUTE_TARGETS (is_cppcx_metadata ? \
                        L"Windows::Foundation::Metadata::AttributeTargets" : \
                        L"System::AttributeTargets")
#define OBSOLETE_ATTRIBUTE (is_cppcx_metadata ? \
                            nullptr : \
                            L"System::ObsoleteAttribute")
#define DEFAULT_MEMBER_ATTRIBUTE (is_cppcx_metadata ? \
                             L"Platform::Metadata::DefaultMemberAttribute" : \
                             L"System::Reflection::DefaultMemberAttribute")
#define FLAGS_ATTRIBUTE (is_cppcx_metadata ? \
                         L"Platform::Metadata::FlagsAttribute" : \
                         L"System::FlagsAttribute")
#define BROWSABLE_ATTRIBUTE (is_cppcx_metadata ? \
                             nullptr : \
                             L"System::ComponentModel::BrowsableAttribute")
#define EDITOR_BROWSABLE_ATTRIBUTE (is_cppcx_metadata ? \
                          nullptr : \
                          L"System::ComponentModel::EditorBrowsableAttribute")
#define DESCRIPTION_ATTRIBUTE (is_cppcx_metadata ? \
                              nullptr : \
                              L"System::ComponentModel::DescriptionAttribute")
#define HELP_KEYWORD_ATTRIBUTE (is_cppcx_metadata ? \
                              nullptr : \
                              L"System::ComponentModel::HelpKeywordAttribute")
#define DISPLAY_NAME_ATTRIBUTE (is_cppcx_metadata ? \
                              nullptr : \
                              L"System::ComponentModel::DisplayNameAttribute")
#define ALLOW_MULTIPLE_ATTRIBUTE (is_cppcx_metadata ? \
                  L"Windows::Foundation::Metadata::AllowMultipleAttribute" : \
                  nullptr)
#define DEPRECATED_ATTRIBUTE (is_cppcx_metadata ? \
                     L"Windows::Foundation::Metadata::DeprecatedAttribute" : \
                     nullptr)

/* The carriage return character is used because the new line character,
   ATTENTION_MARKER, has special meaning. */
#define END_OF_LINE '\r'
#define L_END_OF_LINE L'\r'

/*
If hr indicates an error occurred, issue an internal error indicating
that the routine specified by name failed.
*/
#define CHECK_API_RESULT(hr, name)                               \
  if (FAILED(hr)) {                                              \
    unexpected_condition_str("call to API routine " #name " failed"); \
  }


static wstring char_string_to_wstring(a_const_char* str)
/*
Convert a char* to a std::wstring.
*/
{
  return wstring(conv_utf8_to_wchar(str));
}  /* char_string_to_wstring */


ostream& operator<<(ostream& buffer, const wstring& text)
/*
Operator to convert text from wide characters to UTF-8 and output the result.
*/
{
  return buffer << conv_wide_to_utf8(const_cast<wchar_t*>(text.c_str()));
}  /* operator<< */


/*
A helper for the an_import_interface class that implements the wrapping of
calls to functions that require a string buffer of unknown length.  An
attempt is first made to call the wrapped function with a stack-based
buffer of reasonable length.  If that buffer was not large enough, a
heap-based buffer of the required size is allocated and the wrapped
function is called a second time.
*/
#define GET_NAME_WRAPPER(WRAPPED_FUNCTION) \
{ \
  HRESULT             hr; \
  WCHAR               name_buffer_stack[128]; \
  unique_ptr<WCHAR[]> name_buffer_heap; \
  WCHAR               *name_buffer = name_buffer_stack; \
  ULONG               characters_in_name = _countof(name_buffer_stack); \
  ULONG               characters_required = 0; \
  hr = WRAPPED_FUNCTION; \
  if (hr == CLDB_S_TRUNCATION || characters_required > characters_in_name) { \
    check_assertion(characters_required > characters_in_name); \
    name_buffer_heap.reset(new WCHAR[characters_required]); \
    name_buffer = name_buffer_heap.get(); \
    characters_in_name = characters_required; \
    hr = WRAPPED_FUNCTION; \
    check_assertion(hr != CLDB_S_TRUNCATION); \
  }  /* if */ \
  if (FAILED(hr) || characters_required == 0) { \
    name.clear(); \
  } else { \
    name.assign(name_buffer, characters_required - 1); \
  }  /* if */ \
  return hr; \
}


/*
A wrapper for the IMetaDataImport2 interface that facilitates invoking the
functions that require a string buffer of unknown length.
*/
class an_import_interface : public IMetaDataImport2
{
private:
  an_import_interface();
public:
  using IMetaDataImport2::GetScopeProps;
  HRESULT GetScopeProps(wstring &name,
                        GUID    *pmvid) {
    GET_NAME_WRAPPER(GetScopeProps(
      name_buffer, characters_in_name, &characters_required, pmvid));
  }  /* GetScopeProps */

  using IMetaDataImport2::GetTypeDefProps;
  HRESULT GetTypeDefProps(mdTypeDef td,
                          DWORD     *pdwTypeDefFlags,
                          mdToken   *ptkExtends = nullptr) {
    return GetTypeDefProps(td, /*szTypeDef=*/nullptr, /*cchTypeDef=*/0,
                           /*pchTypeDef=*/0, pdwTypeDefFlags,
                           ptkExtends);
  }  /* GetTypeDefProps */
  HRESULT GetTypeDefProps(mdTypeDef td,
                          wstring   &name,
                          DWORD     *pdwTypeDefFlags,
                          mdToken   *ptkExtends) {
    GET_NAME_WRAPPER(GetTypeDefProps(
      td, name_buffer, characters_in_name, &characters_required,
      pdwTypeDefFlags, ptkExtends));
  }  /* GetTypeDefProps */

  using IMetaDataImport2::GetTypeRefProps;
  HRESULT GetTypeRefProps(mdTypeRef tr,
                          mdToken   *ptkResolutionScope,
                          wstring   &name) {
    GET_NAME_WRAPPER(GetTypeRefProps(
      tr, ptkResolutionScope, name_buffer, characters_in_name,
      &characters_required));
  }  /* GetTypeRefProps */

  HRESULT GetTypeRefProps(mdTypeRef tr,
                          mdToken   *ptkResolutionScope) {
    return GetTypeRefProps(tr, ptkResolutionScope, /*szName=*/nullptr,
                           /*cchName=*/0, /*pchName=*/0);
  }  /* GetTypeRefProps */

  using IMetaDataImport2::GetMethodProps;
  HRESULT GetMethodProps(mdMethodDef     mb,
                         mdTypeDef       *pClass,
                         DWORD           *pdwAttr,
                         PCCOR_SIGNATURE *ppvSigBlob,
                         ULONG           *pcbSigBlob,
                         ULONG           *pulCodeRVA,
                         DWORD           *pdwImplFlags) {
    return GetMethodProps(mb, pClass, /*szMethod=*/nullptr, /*cchMethod=*/0,
                          /*pchMethod=*/nullptr, pdwAttr, ppvSigBlob,
                          pcbSigBlob, pulCodeRVA, pdwImplFlags);
  }  /* GetMethodProps */
  HRESULT GetMethodProps(mdMethodDef     mb,
                         mdTypeDef       *pClass,
                         wstring         &name,
                         DWORD           *pdwAttr,
                         PCCOR_SIGNATURE *ppvSigBlob,
                         ULONG           *pcbSigBlob,
                         ULONG           *pulCodeRVA,
                         DWORD           *pdwImplFlags) {
    GET_NAME_WRAPPER(GetMethodProps(
      mb, pClass, name_buffer, characters_in_name, &characters_required,
      pdwAttr, ppvSigBlob, pcbSigBlob, pulCodeRVA, pdwImplFlags));
  }  /* GetMethodProps */

  using IMetaDataImport2::GetMemberRefProps;
  HRESULT GetMemberRefProps(mdMemberRef     mr,
                            mdToken         *ptk,
                            PCCOR_SIGNATURE *ppvSigBlob,
                            ULONG           *pbSig) {
    return GetMemberRefProps(mr, ptk, /*szMember=*/nullptr, /*cchMember=*/0,
                             /*pchMember=*/nullptr, ppvSigBlob, pbSig);
  }  /* GetMemberRefProps */
  HRESULT GetMemberRefProps(mdMemberRef     mr,
                            mdToken         *ptk,
                            wstring         &name,
                            PCCOR_SIGNATURE *ppvSigBlob,
                            ULONG           *pbSig) {
    GET_NAME_WRAPPER(GetMemberRefProps(
      mr, ptk, name_buffer, characters_in_name, &characters_required,
      ppvSigBlob, pbSig));
  }  /* GetMemberRefProps */

  using IMetaDataImport2::GetEventProps;
  HRESULT GetEventProps(mdEvent     ev,
                        mdTypeDef   *pClass,
                        wstring     &name,
                        DWORD       *pdwEventFlags,
                        mdToken     *ptkEventType,
                        mdMethodDef *pmdAddOn,
                        mdMethodDef *pmdRemoveOn,
                        mdMethodDef *pmdFire,
                        mdMethodDef rmdOtherMethod[],
                        ULONG       cMax,
                        ULONG       *pcOtherMethod) {
    GET_NAME_WRAPPER(GetEventProps(
      ev, pClass, name_buffer, characters_in_name, &characters_required,
      pdwEventFlags, ptkEventType, pmdAddOn, pmdRemoveOn, pmdFire,
      rmdOtherMethod, cMax, pcOtherMethod));
  }  /* GetEventProps */

  using IMetaDataImport2::GetModuleRefProps;
  HRESULT GetModuleRefProps(mdModuleRef mur,
                            wstring     &name) {
    GET_NAME_WRAPPER(GetModuleRefProps(
      mur, name_buffer, characters_in_name, &characters_required));
  }  /* GetModuleRefProps */

  using IMetaDataImport2::GetUserString;
  HRESULT GetUserString(mdString stk,
                        wstring  &name) {
    GET_NAME_WRAPPER(GetUserString(
      stk, name_buffer, characters_in_name, &characters_required));
  }  /* GetUserString */

  using IMetaDataImport2::GetPinvokeMap;
  HRESULT GetPinvokeMap(mdToken     tk,
                        DWORD       *pdwMappingFlags,
                        wstring     &name,
                        mdModuleRef *pmrImportDLL) {
    GET_NAME_WRAPPER(GetPinvokeMap(
      tk, pdwMappingFlags, name_buffer, characters_in_name,
      &characters_required, pmrImportDLL));
  }  /* GetPinvokeMap */

  using IMetaDataImport2::GetMemberProps;
  HRESULT GetMemberProps(mdToken         mb,
                         mdTypeDef       *pClass,
                         wstring         &name,
                         DWORD           *pdwAttr,
                         PCCOR_SIGNATURE *ppvSigBlob,
                         ULONG           *pcbSigBlob,
                         ULONG           *pulCodeRVA,
                         DWORD           *pdwImplFlags,
                         DWORD           *pdwCPlusTypeFlag,
                         UVCP_CONSTANT   *ppValue,
                         ULONG           *pcchValue) {
    GET_NAME_WRAPPER(GetMemberProps(
      mb, pClass, name_buffer, characters_in_name, &characters_required,
      pdwAttr, ppvSigBlob, pcbSigBlob, pulCodeRVA, pdwImplFlags,
      pdwCPlusTypeFlag, ppValue, pcchValue));
  }  /* GetMemberProps */

  using IMetaDataImport2::GetFieldProps;
  HRESULT GetFieldProps(mdFieldDef      mb,
                        mdTypeDef       *pClass,
                        wstring         &name,
                        DWORD           *pdwAttr,
                        PCCOR_SIGNATURE *ppvSigBlob,
                        ULONG           *pcbSigBlob,
                        DWORD           *pdwCPlusTypeFlag,
                        UVCP_CONSTANT   *ppValue,
                        ULONG           *pcchValue) {
    GET_NAME_WRAPPER(GetFieldProps(
      mb, pClass, name_buffer, characters_in_name, &characters_required,
      pdwAttr, ppvSigBlob, pcbSigBlob, pdwCPlusTypeFlag, ppValue,
      pcchValue));
  }  /* GetFieldProps */

  using IMetaDataImport2::GetPropertyProps;
  HRESULT GetPropertyProps(mdProperty      prop,
                           mdTypeDef       *pClass,
                           wstring         &name,
                           DWORD           *pdwPropFlags,
                           PCCOR_SIGNATURE *ppvSig,
                           ULONG           *pbSig,
                           DWORD           *pdwCPlusTypeFlag,
                           UVCP_CONSTANT   *ppDefaultValue,
                           ULONG           *pcchDefaultValue,
                           mdMethodDef     *pmdSetter,
                           mdMethodDef     *pmdGetter,
                           mdMethodDef     rmdOtherMethod[],
                           ULONG           cMax,
                           ULONG           *pcOtherMethod) {
    GET_NAME_WRAPPER(GetPropertyProps(
      prop, pClass, name_buffer, characters_in_name, &characters_required,
      pdwPropFlags, ppvSig, pbSig, pdwCPlusTypeFlag, ppDefaultValue,
      pcchDefaultValue, pmdSetter, pmdGetter, rmdOtherMethod, cMax,
      pcOtherMethod));
  }  /* GetPropertyProps */

  using IMetaDataImport2::GetParamProps;
  HRESULT GetParamProps(mdParamDef    tk,
                        mdMethodDef   *pmd,
                        ULONG         *pulSequence,
                        wstring       &name,
                        DWORD         *pdwAttr,
                        DWORD         *pdwCPlusTypeFlag,
                        UVCP_CONSTANT *ppValue,
                        ULONG         *pcchValue) {
    GET_NAME_WRAPPER(GetParamProps(
      tk, pmd, pulSequence, name_buffer, characters_in_name,
      &characters_required, pdwAttr, pdwCPlusTypeFlag, ppValue, pcchValue));
  }  /* GetParamProps */

  using IMetaDataImport2::GetGenericParamProps;
  HRESULT GetGenericParamProps(mdGenericParam gp,
                               ULONG          *pulParamSeq,
                               DWORD          *pdwParamFlags,
                               mdToken        *ptOwner,
                               DWORD          *reserved,
                               wstring        &name) {
    GET_NAME_WRAPPER(GetGenericParamProps(
      gp, pulParamSeq, pdwParamFlags, ptOwner, reserved, name_buffer,
      characters_in_name, &characters_required));
  }  /* GetGenericParamProps */
};  /* an_import_interface */
static_assert(sizeof(an_import_interface) == sizeof(IMetaDataImport2),
             "an_import_interface must be the same size as IMetaDataImport2");
typedef CComPtr<an_import_interface> an_import_interface_ptr;

/*
A wrapper for the IAssemblyName interface.
*/
class an_assembly_name
{
public:
  an_assembly_name()
  {
  }  /* Default constructor. */

  an_assembly_name(wstring reference_name);

  an_assembly_name(wstring                name,
                   const ASSEMBLYMETADATA &data,
                   const void             *public_key_or_token,
                   ULONG                  bytes_in_public_key_or_token,
                   DWORD                  flags);

  ~an_assembly_name()
  {
    if (name_interface_ != nullptr) name_interface_->Finalize();
  }  /* Destructor. */

  const wstring &display_name() const { return display_name_; }

  bool reference_matches_definition(
                               const an_assembly_name &definition_name) const;

private:
  void init_display_name() {
    if (name_interface_ != nullptr) {
      HRESULT hr;
      WCHAR   *name_buffer;
      DWORD   characters_in_name = 0;
      hr = name_interface_->GetDisplayName(nullptr, &characters_in_name,
                                           /*dwDisplayFlags*/0);
      if (hr != HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)) {
        CHECK_API_RESULT(hr, GetDisplayName);
      }  /* if */
      name_buffer = reinterpret_cast<WCHAR*>(
                                 _alloca(characters_in_name * sizeof(WCHAR)));
      hr = name_interface_->GetDisplayName(name_buffer, &characters_in_name,
                                           /*dwDisplayFlags*/0);
      CHECK_API_RESULT(hr, GetDisplayName);
      display_name_.assign(name_buffer, characters_in_name - 1);
    }  /* if */
  }  /* init_display_name */

  CComPtr<IAssemblyName>
                 name_interface_;
                        /* The IAssemblyName interface. */
  wstring        display_name_;
                        /* The display name of the assembly. */
};  /* an_assembly_name */


template<typename wstring_iterator, typename condition_pred>
wstring_iterator find_unescaped_character_if(
                                           condition_pred   condition,
                                           wstring_iterator first,
                                           wstring_iterator last,
                                           wchar_t          escape_ch = L'\\')
/*
Return the iterator to the first character in [first, last) that is not
escaped by the 'escape_ch' character and where the predicate 'condition', of
type 'bool ()(const wchar_t &)', returns true.  Returns 'last' if the search
was unsuccessful.
*/
{
  int escape_ch_count = 0;
  auto pred = [&](wchar_t current_ch) {
    bool result = (escape_ch_count % 2) == 0 && condition(current_ch);
    if (!result) {
      if (current_ch == escape_ch) {
        ++escape_ch_count;
      } else {
        escape_ch_count = 0;
      }  /* if */
    }  /* if */
    return result;
  };
  return find_if(first, last, pred);
}  /* find_unescaped_character_if */


template<typename wstring_iterator>
wstring_iterator find_unescaped_character(wchar_t          ch,
                                          wstring_iterator first,
                                          wstring_iterator last,
                                          wchar_t          escape_ch = L'\\')
/*
Return the iterator to the first occurrence of 'ch' in [first, last) that is
not escaped by the 'escape_ch' character, or 'last' if the search was
unsuccessful.
*/
{
  auto is_equal_char = [ch](wchar_t current_ch) { return ch == current_ch; };
  return find_unescaped_character_if(is_equal_char, first, last, escape_ch);
}  /* find_unescaped_character */


template<typename wstring_iterator, typename condition_pred>
wstring_iterator find_escaped_character_if(condition_pred   condition,
                                           wstring_iterator first,
                                           wstring_iterator last,
                                           wchar_t          escape_ch = L'\\')
/*
Return the iterator to the first character in [first, last) that is
escaped by the 'escape_ch' character and where the predicate 'condition', of
type 'bool ()(const wchar_t &)', returns true.  Returns 'last' if the search
was unsuccessful.
*/
{
  int escape_ch_count = 0;
  auto pred = [&](wchar_t current_ch) {
    bool result = (escape_ch_count % 2) == 1 && condition(current_ch);
    if (!result) {
      if (current_ch == escape_ch) {
        ++escape_ch_count;
      } else {
        escape_ch_count = 0;
      }  /* if */
    }  /* if */
    return result;
  };
  return find_if(first, last, pred);
}  /* find_escaped_character_if */


template<typename wstring_iterator>
wstring_iterator find_escaped_character(wchar_t          ch,
                                        wstring_iterator first,
                                        wstring_iterator last,
                                        wchar_t          escape_ch = L'\\')
/*
Return the iterator to the first occurrence of 'ch' in [first, last) that is
escaped by the 'escape_ch' character, or 'last' if the search was unsuccessful.
*/
{
  auto is_equal_char = [ch](wchar_t current_ch) { return ch == current_ch; };
  return find_escaped_character_if(is_equal_char, first, last, escape_ch);
}  /* find_escaped_character */


template<typename condition_pred>
void unescape_escaped_character_if(wstring           &str,
                                   condition_pred    condition,
                                   wstring::iterator first,
                                   wstring::iterator last,
                                   wchar_t           escape_ch = L'\\')
/*
Removes all unescaped 'escape_ch' characters in [first, last) that precede a
character where the predicate 'condition', of type 'bool ()(const wchar_t &)',
returns true.
*/
{
  auto iter1 = find_escaped_character_if(condition, first, last, escape_ch);
  auto end = iter1 - 1;

  while (iter1 != last) {
    auto iter2 = find_escaped_character_if(condition, iter1 + 1, last,
                                           escape_ch);
    if (iter2 == last) {
      end = move(iter1, iter2, end);
      str.erase(end, last);
      break;
    } else {
      end = move(iter1, iter2 - 1, end);
      iter1 = iter2;
    }  /* if */
  }  /* while */
}  /* unescape_escaped_character_if */


void unmangle_cli_identifier(wstring &identifier)
/*
Unmangle a CLI identifier by unescaping any escaped special characters. See the
"Specifying Fully Qualified Type Names" section in the .NET System.Reflection
API reference: http://msdn.microsoft.com/en-us/library/yfsftwz6.aspx.
*/
{
  auto is_escaped_cli_identifier_char = 
                         [](wchar_t current_ch) { 
                           return wcschr(L",+&*[].\\", current_ch) != nullptr;
                         };
  unescape_escaped_character_if(identifier,
                                is_escaped_cli_identifier_char,
                                identifier.begin(), identifier.end());
}  /* unmangle_cli_identifier */


static bool is_cli_identifier_char(wchar_t ch,
                                   bool    is_identifier_start)
/*
Return true if ch is valid as a character in an identifier (or as the first
character of an identifier if is_identifier_start is true).
*/
{
  bool is_id = false;

  /* See whether the Unicode code point ch is a valid identifier character. */
#if MICROSOFT_UNICODE_COMPATIBILITY
  if (ch > SCHAR_MAX) {
    /* If the high bit is set, MSVC allows any character other than "iswspace"
       in an identifier. */
    is_id = !iswspace((wint_t)ch);
  } else
#endif /* MICROSOFT_UNICODE_COMPATIBILITY */
  /* Do not insert code here */
  if (ch <= UCHAR_MAX) {
    /* Small value -- use the lookup table. */
    is_id = is_id_char_no_mbc[ch] &&
            (!is_identifier_start || !isdigit((unsigned char)ch));
  } else if (ch >= 0xd800 && ch <= 0xdfff) {
    /* Surrogate code points are not allowed. */
    is_id = FALSE;
  } else {
    /* Do the full lookup for larger values. */
    is_id = (is_valid_UCN_identifier_char(ch, is_identifier_start) ==
             ec_no_error);
  }  /* if */
  return is_id;
}  /* is_cli_identifier_char */

bool escaped_char_for_string_literal_special_char(wchar_t &ch)
/*
Returns TRUE if 'ch' must be escaped within a C++ string literal and sets 'ch'
to the corresponding escaped character.
*/
{
  switch (ch) {
    case L'\\': ch = '\\'; return true;
    case L'"':  ch = '"';  return true;
    case L'\n': ch = 'n';  return true;
    case L'\t': ch = 't';  return true;
    case L'\v': ch = 'v';  return true;
    case L'\b': ch = 'b';  return true;
    case L'\r': ch = 'r';  return true;
    case L'\f': ch = 'f';  return true;
    case L'\a': ch = 'a';  return true;
  }  /* switch */
  return false;
}  /* escaped_char_for_string_literal_special_char */

void append_to_string_literal(wstring &string_literal, wchar_t ch)
/*
Append 'ch' to 'string_literal', escaping characters that have special
meaning within a C++ string literal.
*/
{
  if (escaped_char_for_string_literal_special_char(ch)) {
    string_literal += L'\\';
  }  /* if */
  string_literal += ch;
}  /* append_to_string_literal */

void append_to_string_literal(wostringstream &string_literal, wchar_t ch)
/*
Append 'ch' to 'string_literal', escaping characters that have special
meaning within a C++ string literal.
*/
{
  if (escaped_char_for_string_literal_special_char(ch)) {
    string_literal.put(L'\\');
  }  /* if */
  string_literal.put(ch);
}  /* append_to_string_literal */

void escape_invalid_identifier(wstring            &identifier,
                               wstring::size_type chars_to_skip = 0)
/*
If 'identifier' is not a valid C++ identifier, wrap it with
"__identifier("...")".  If chars_to_skip is nonzero, only wrap the portion
of the given string that starts at the position indicated by chars_to_skip.
*/
{
  if (identifier.length() > 0) {
    check_assertion(identifier.length() > chars_to_skip);
    auto skipped_chars_end = identifier.begin() + chars_to_skip;
    auto iter = skipped_chars_end;
    auto ch = *iter;
    if (is_cli_identifier_char(ch, /*is_identifier_start=*/true)) {
      for (++iter; iter != identifier.end(); ++iter) {
        ch = *iter;
        if (!is_cli_identifier_char(ch, /*is_identifier_start=*/false)) {
          break;
        }  /* if */
      }  /* for */
    }  /* if */
    if (iter != identifier.end()) {
      /* At this point 'iter' addresses the first invalid character in
         'identifier'. */
      wstring escaped_identifier;
      escaped_identifier.reserve(sizeof("__identifier(\"")-1 +
                                       identifier.length() + sizeof("\")")-1);
      /* Copy the skipped characters. */
      escaped_identifier.append(identifier.begin(), skipped_chars_end);
      escaped_identifier += L"__identifier(\"";
      /* Copy the characters that are known to be valid. */
      escaped_identifier.append(skipped_chars_end, iter);
      /* Copy the remaining characters, escaping characters that have special
         meaning within a C++ string literal. */
      for_each(iter, identifier.end(),
               [&](wstring::const_reference ch) {
                 append_to_string_literal(escaped_identifier, ch);
               });
      escaped_identifier += L"\")";
      identifier = move(escaped_identifier);
    } else {
      /* Escape identifiers that are spelled like C++ keywords. */
      a_symbol_header_ptr header;
      a_symbol_locator    locator;
      a_symbol_ptr        sym;
      string              utf8_identifier = conv_wide_to_utf8(
                                    const_cast<wchar_t*>(identifier.c_str()) +
                                                               chars_to_skip);
      clear_locator(&locator, &null_source_position);
      header = find_symbol_header(const_cast<char*>(utf8_identifier.c_str()),
                                  utf8_identifier.length(),
                                  &locator);
      sym = symbol_list_for_file_scope_symbols(header);
      /* Macros appear before keywords on the symbol list, so skip the former
         before looking for the latter. */
      while (sym != NULL && symbol_is(sym, sk_macro)) sym = sym->next;
      if (sym != NULL && symbol_is(sym, sk_keyword)) {
        wstring escaped_identifier;
        escaped_identifier.reserve(sizeof("__identifier(\"")-1 +
                                       identifier.length() + sizeof("\")")-1);
        /* Copy the skipped characters. */
        escaped_identifier.append(identifier.begin(), skipped_chars_end);
        escaped_identifier += L"__identifier(\"";
        /* Copy the rest of the identifier. */
        escaped_identifier.append(skipped_chars_end, identifier.end());
        escaped_identifier += L"\")";
        identifier = move(escaped_identifier);
      }  /* if */
    }  /* if */
  }  /* if */
}  /* escape_invalid_identifier */


BYTE strip_generic_arity(wstring &type_name)
/*
Strip the generic arity encoded after the last backtick in the type name.
*/
{
  BYTE generic_arity = 0;
  wstring::size_type back_tick_index = type_name.rfind(L'`');
  check_assertion(back_tick_index != 0 &&
                  back_tick_index != type_name.size());
  if (back_tick_index != wstring::npos) {
    ULONG val = _wtoi(type_name.c_str() + back_tick_index + 1);
    check_assertion(val > 0 && val <= (numeric_limits<BYTE>::max)());
    generic_arity = static_cast<BYTE>(val);
    type_name.resize(back_tick_index);
  }  /* if */
  return generic_arity;
}  /* strip_generic_arity */


class an_import_scope;
typedef const an_import_scope a_const_import_scope;
typedef an_import_scope* an_import_scope_ptr;
typedef a_const_import_scope* a_const_import_scope_ptr;
class a_type_wrapper;
typedef std::shared_ptr<a_type_wrapper> a_type_wrapper_ptr;
typedef std::shared_ptr<const a_type_wrapper> a_const_type_wrapper_ptr;
class a_class_type_wrapper;
typedef std::shared_ptr<a_class_type_wrapper> a_class_type_wrapper_ptr;
typedef std::shared_ptr<const a_class_type_wrapper>
                                               a_const_class_type_wrapper_ptr;
class an_array_type_wrapper;
typedef std::shared_ptr<an_array_type_wrapper> an_array_type_wrapper_ptr;
typedef std::shared_ptr<const an_array_type_wrapper>
                                               a_const_array_type_wrapper_ptr;
class a_type_indirection;
typedef std::shared_ptr<a_type_indirection> a_type_indirection_ptr;
typedef std::shared_ptr<const a_type_indirection>
                                                 a_const_type_indirection_ptr;
class a_function_type_wrapper;
typedef std::shared_ptr<a_function_type_wrapper> a_function_type_wrapper_ptr;
typedef std::shared_ptr<const a_function_type_wrapper>
                                            a_const_function_type_wrapper_ptr;
typedef std::vector<a_type_wrapper_ptr> a_type_wrapper_list;
typedef std::vector<a_const_type_wrapper_ptr> a_const_type_wrapper_list;
class a_signature_decoder_scope;
class a_generic_instance_scope;
typedef shared_ptr<const a_generic_instance_scope>
                                                 a_generic_instance_scope_ptr;
typedef a_type_wrapper_list a_generic_argument_list;
const a_generic_argument_list no_generic_arguments;
typedef a_generic_argument_list::const_iterator a_generic_argument_iterator;
class a_generic_parameter;
typedef std::vector<a_generic_parameter> a_generic_parameter_list;
typedef a_generic_parameter_list::const_iterator a_generic_parameter_iterator;
typedef a_const_type_wrapper_list a_generic_constraint_list;
/* A list of generic types that have been declared with a pending constraint
   clause. */
typedef vector<mdTypeDef> a_pending_constraint_type_list;
class a_type_definition;
class a_method_definition;
typedef unique_ptr<a_method_definition> a_method_definition_ptr;
typedef unique_ptr<const a_method_definition> a_const_method_definition_ptr;

/*
A qualified name.
*/
class a_qualified_name {
public:
  a_qualified_name()
    : generic_arguments_offset_(wstring::npos)
  {
  }  /* Default constructor */

  static a_qualified_name from_dotted_name(const wstring &dotted_name)
  /*
  Construct a qualified name from a dotted name, ensuring that every
  identifier is a valid C++ identifier.
  */
  {
    a_qualified_name qualified_name;
    auto             begin = dotted_name.begin();
    auto             end = dotted_name.end();

    for (;;) {
      auto dot_iter = find_unescaped_character(L'.', begin, end);
      wstring identifier(begin, dot_iter);
      unmangle_cli_identifier(identifier);
      qualified_name.append_identifier(move(identifier));
      if (dot_iter == end) break;
      begin = dot_iter + 1;
    }  /* for */
    return qualified_name;
  }  /* from_dotted_name */

  const wstring &as_string() const {
    return name_;
  }  /* as_string */

  bool operator==(const wchar_t *other) const {
    return name_ == other;
  }  /* operator== */

  bool operator!=(const wchar_t *other) const {
    return name_ != other;
  }  /* operator!= */

  wstring unqualified_name() const
  /*
  Return the last identifier in the qualified name.
  */
  {
    wstring name;
    if (separator_offsets_.empty()) {
      name = name_;
    } else {
      name = name_.substr(separator_offsets_.back() + separator_length);
    }  /* if */
    return name;
  }  /* unqualified_name */

  a_qualified_name namespace_name() const
  /*
  Return the enclosing namespace name.
  */
  {
    a_qualified_name name;
    if (!separator_offsets_.empty()) {
      name.name_ = name_.substr(0, separator_offsets_.back());
      if (separator_offsets_.size() > 1) {
        name.separator_offsets_ = vector<wstring::size_type>(
                                                separator_offsets_.begin(),
                                                separator_offsets_.end() - 1);
      }  /* if */
    }  /* if */
    return name;
  }  /* namespace_name */

  bool empty() const {
    return name_.empty();
  }  /* empty */

  bool is_unqualified() const {
    return separator_offsets_.empty();
  }  /* is_unqualified */

  const vector<wstring::size_type> &separator_offsets() const {
    return separator_offsets_;
  }  /* separator_offsets */

  a_qualified_name &append_identifier(wstring identifier)
  /*
  Append an identifier to the qualified name, wrapping it with
  "__identifier("...")" if it is not a valid C++ identifier.
  */
  {
    check_assertion(!identifier.empty());
    escape_invalid_identifier(identifier);
    if (empty()) {
      name_ = identifier;
    } else {
      separator_offsets_.push_back(name_.length());
      name_ += separator;
      name_ += identifier;
    }  /* if */
    generic_arguments_offset_ = wstring::npos;
    return *this;
  }  /* append_identifier */

  a_qualified_name &operator+=(const wstring &identifier) {
    return append_identifier(identifier);
  }  /* operator+= */

  a_qualified_name &append_qualified_name(const a_qualified_name &other)
  /*
  Append a qualified name to this one.
  */
  {
    if (empty() && !other.empty()) {
      *this = other;
    } else if (!other.empty()) {
      separator_offsets_.reserve(separator_offsets_.size() + 1 +
                                             other.separator_offsets_.size());
      /* Append the separator. */
      separator_offsets_.push_back(name_.length());
      name_ += separator;
      /* Append the other name, adjusting its separator offsets
         accordingly. */
      auto offset_adjustment = name_.length();
      for (auto &offset : other.separator_offsets_) {
        separator_offsets_.push_back(offset + offset_adjustment);
      }  /* for */
      if (other.generic_arguments_offset_ != wstring::npos) {
        generic_arguments_offset_ = other.generic_arguments_offset_ +
                                                            offset_adjustment;
      } else {
        generic_arguments_offset_ = wstring::npos;
      }  /* if */
      name_ += other.name_;
    }  /* if */
    return *this;
  }  /* append_qualified_name */

  a_qualified_name &operator+=(const a_qualified_name &other) {
    return append_qualified_name(other);
  }  /* operator+= */

  void append_generic_arguments(
                          a_generic_argument_iterator generic_arguments_begin,
                          a_generic_argument_iterator generic_arguments_end);

  void strip_generic_arguments()
  /*
  Shorten the embedded string to exclude the generic arguments.
  */
  {
    if (generic_arguments_offset_ != wstring::npos) {
      name_.resize(generic_arguments_offset_);
      generic_arguments_offset_ = wstring::npos;
    }  /* if */
  }  /* strip_generic_arguments */

  static const wchar_t *separator;
  static const auto separator_length = sizeof("::")-1;
private:
  wstring name_;        /* The fully-qualified name in C++ syntax. */
  vector<wstring::size_type> separator_offsets_;
                        /* The indices of the namespace separators. */
  wstring::size_type generic_arguments_offset_;
                        /* The index of the generic arguments. */
}; /* a_qualified_name */

const wchar_t *a_qualified_name::separator = L"::";

/*
A class that represents types imported from metadata.  Instances of the
a_type_wrapper class represent fundamental types.  Instances of derived
classes are used to represent other kinds of types.
*/
class a_type_wrapper
{
public:
  enum a_kind
  {
    twk_invalid,
    twk_void,
    twk_cxx_udt_return,
    twk_copy_ctor,
    twk_bool,
    twk_char,
    twk_signed_char,
    twk_unsigned_char,
    twk_short,
    twk_unsigned_short,
    twk_wchar_t,
    twk_int,
    twk_unsigned_int,
    twk_long,
    twk_unsigned_long,
    twk_long_long,
    twk_unsigned_long_long,
    twk_float,
    twk_double,
    twk_long_double,
    twk_class,
    twk_array,
    twk_indirection,
    twk_function
  };

  typedef unsigned char a_qualifier_flag_set;
  enum a_qualifier_flag : a_qualifier_flag_set
  {
    qf_none     = 0x0,
    qf_const    = 0x1,
    qf_volatile = 0x2
  };

  a_type_wrapper(a_kind kind)
    : kind_(kind)
    , qualifier_flags_(qf_none)
  {
  }  /* Constructor. */

  static a_type_wrapper_ptr create(a_kind kind)
  {
    return make_shared<a_type_wrapper>(kind);
  }  /* create */

  static a_type_wrapper_ptr create(CorElementType element_type,
                                   bool           builtin_wchar_t = true);

  static a_type_wrapper_ptr create(
                               CorSerializationType serialization_type,
                               bool                 builtin_wchar_t = true)
  {
    a_type_wrapper_ptr type;
    switch (serialization_type) {
      case SERIALIZATION_TYPE_BOOLEAN:
      case SERIALIZATION_TYPE_CHAR:
      case SERIALIZATION_TYPE_I1:
      case SERIALIZATION_TYPE_U1:
      case SERIALIZATION_TYPE_I2:
      case SERIALIZATION_TYPE_U2:
      case SERIALIZATION_TYPE_I4:
      case SERIALIZATION_TYPE_U4:
      case SERIALIZATION_TYPE_I8:
      case SERIALIZATION_TYPE_U8:
      case SERIALIZATION_TYPE_R4:
      case SERIALIZATION_TYPE_R8:
      case SERIALIZATION_TYPE_STRING:
        type = create(static_cast<CorElementType>(serialization_type),
                      builtin_wchar_t);
        break;
      case SERIALIZATION_TYPE_TAGGED_OBJECT:
        type = create(ELEMENT_TYPE_OBJECT);
        break;
      default:
        unexpected_condition();
        break;
    }  /* switch */
    return type;
  }  /* create */

  virtual ~a_type_wrapper()
  {
  }  /* Destructor. */

  virtual a_type_wrapper_ptr copy() const {
    return make_shared<a_type_wrapper>(*this);
  }  /* copy */

  a_kind kind() const { return kind_; }
  void set_kind(a_kind kind) { kind_ = kind; }
  bool is_of_kind(a_kind kind) const { return kind_ == kind; }
  bool is_invalid() const { return kind_ == twk_invalid; }

  a_qualifier_flag_set qualifier_flags() const { return qualifier_flags_; }
  void set_qualifier_flags(a_qualifier_flag_set qualifier_flags) {
    qualifier_flags_ = qualifier_flags;
  }  /* set_qualifier_flags */
  void add_qualifier_flags(a_qualifier_flag_set qualifier_flags) {
    qualifier_flags_ |= qualifier_flags;
  }  /* set_qualifier_flags */

  virtual bool uses_unresolved_type(
                        a_const_class_type_wrapper_ptr &unresolved_type) const
  {
    return false;
  }  /* uses_unresolved_type */

  virtual a_class_type_wrapper_ptr as_class() {
    check_assertion(!is_of_kind(twk_class));
    return nullptr;
  }  /* as_class */

  virtual a_const_class_type_wrapper_ptr as_class() const {
    check_assertion(!is_of_kind(twk_class));
    return nullptr;
  }  /* as_class */

  virtual an_array_type_wrapper_ptr as_array() {
    check_assertion(!is_of_kind(twk_array));
    return nullptr;
  }  /* as_array */

  virtual a_const_array_type_wrapper_ptr as_array() const {
    check_assertion(!is_of_kind(twk_array));
    return nullptr;
  }  /* as_array */

  virtual an_array_type_wrapper_ptr as_handle_to_array() {
    check_assertion(!is_of_kind(twk_indirection));
    return nullptr;
  }  /* as_handle_to_array */

  virtual a_const_array_type_wrapper_ptr as_handle_to_array() const {
    check_assertion(!is_of_kind(twk_indirection));
    return nullptr;
  }  /* as_handle_to_array */

  virtual a_type_indirection_ptr as_indirection() {
    check_assertion(!is_of_kind(twk_indirection));
    return nullptr;
  }  /* as_pointer */

  virtual a_const_type_indirection_ptr as_indirection() const {
    check_assertion(!is_of_kind(twk_indirection));
    return nullptr;
  }  /* as_pointer */

  virtual a_function_type_wrapper_ptr as_function() {
    check_assertion(!is_of_kind(twk_function));
    return nullptr;
  }  /* as_function */

  virtual a_const_function_type_wrapper_ptr as_function() const {
    check_assertion(!is_of_kind(twk_function));
    return nullptr;
  }  /* as_function */

  wstring get_string(bool expand_unresolved_types = true) const
  {
    return get_string(wstring(), expand_unresolved_types);
  }

  wstring get_string(const wstring &declarator,
                     bool          expand_unresolved_types = true) const
  {
    wostringstream buffer;
    write_first_part(buffer, expand_unresolved_types);
    if (!declarator.empty()) {
      buffer << L' ' << declarator;
    }  /* if */
    write_second_part(buffer, expand_unresolved_types);
    return buffer.str();
  }  /* get_string */

  void write_qualifiers(wostringstream &buffer) const {
    if ((qualifier_flags_ & qf_const) != 0) {
      buffer << L"const";
    }  /* if */
    if ((qualifier_flags_ & qf_volatile) != 0) {
      buffer << L"volatile";
    }  /* if */
  }  /* write_qualifiers */

  virtual void write_first_part(
                               wostringstream &buffer,
                               bool           expand_unresolved_types) const {
    if (qualifier_flags() != qf_none) {
      write_qualifiers(buffer);
      buffer << L' ';
    }  /* if */
    switch (kind_) {
      case twk_void:               buffer << L"void"; break;
      case twk_bool:               buffer << L"bool"; break;
      case twk_char:               buffer << L"char"; break;
      case twk_signed_char:        buffer << L"signed char"; break;
      case twk_unsigned_char:      buffer << L"unsigned char"; break;
      case twk_short:              buffer << L"short"; break;
      case twk_unsigned_short:     buffer << L"unsigned short"; break;
      case twk_wchar_t:            buffer << L"__wchar_t"; break;
      case twk_int:                buffer << L"int"; break;
      case twk_unsigned_int:       buffer << L"unsigned int"; break;
      case twk_long:               buffer << L"long"; break;
      case twk_unsigned_long:      buffer << L"unsigned long"; break;
      case twk_long_long:          buffer << L"long long"; break;
      case twk_unsigned_long_long: buffer << L"unsigned long long"; break;
      case twk_float:              buffer << L"float"; break;
      case twk_double:             buffer << L"double"; break;
      case twk_long_double:        buffer << L"long double"; break;
      default:
        unexpected_condition();
        buffer << L"__error_type";
        break;
    }  /* switch */
  }  /* write_first_part */

  virtual void write_second_part(wostringstream &buffer,
                                 bool           expand_unresolved_types) const
  {
  }  /* write_second_part */

private:
  a_kind kind_;
  a_qualifier_flag_set qualifier_flags_;
};  /* a_type_wrapper */


void a_qualified_name::append_generic_arguments(
                          a_generic_argument_iterator generic_arguments_begin,
                          a_generic_argument_iterator generic_arguments_end)
/*
Append the generic arguments to the qualified name.
*/
{
  check_assertion(!empty());
  check_assertion(std::distance(generic_arguments_begin,
                                generic_arguments_end) > 0);
  generic_arguments_offset_ = name_.length();
  name_ += L'<';
  for (auto iter = generic_arguments_begin;
       iter != generic_arguments_end;
       ++iter) {
    auto &generic_argument = *iter;
    name_ += generic_argument->get_string(/*expand_unresolved_types=*/false);
    if (iter + 1 != generic_arguments_end) {
      name_ += L", ";
    }  /* if */
  }  /* for */
  name_ += L'>';
}  /* a_qualified_name::append_generic_arguments */

/*
A class that represents types imported from metadata that aren't fundamental,
array, function, pointer, or reference types.
*/
class a_class_type_wrapper
  : public a_type_wrapper
  , public enable_shared_from_this<a_class_type_wrapper>
{
public:
  enum a_class_kind
  {
    ck_invalid,
    ck_unresolved,
    ck_class,
    ck_generic_parameter,
  };

  static a_class_type_wrapper_ptr create_system_class(
                                                   const wstring &dotted_name)
  {
    a_qualified_name qualified_name;
    qualified_name.append_identifier(is_cppcx_metadata ? L"Platform"
                                                       : L"System");
    qualified_name.append_qualified_name(
                             a_qualified_name::from_dotted_name(dotted_name));
    return make_shared<a_class_type_wrapper>(ck_class, move(qualified_name));
  }  /* create_system_class*/

  a_class_type_wrapper()
    : a_type_wrapper(twk_invalid)
    , class_kind_(ck_invalid)
    , token_(mdTokenNil)
  {
  }  /* Constructor. */

  a_class_type_wrapper(a_class_kind             kind,
                       a_qualified_name         name,
                       a_const_import_scope_ptr import_scope = nullptr,
                       mdToken                  token = mdTokenNil)
    : a_type_wrapper(twk_class)
    , class_kind_(kind)
    , name_(move(name))
    , import_scope_(import_scope)
    , token_(token)
  {
  }  /* Constructor. */

  virtual a_type_wrapper_ptr copy() const {
    return make_shared<a_class_type_wrapper>(*this);
  }  /* copy */

  a_class_kind class_kind() const { return class_kind_; }
  void set_class_kind(a_class_kind kind) { class_kind_ = kind; }
  bool is_of_class_kind(a_class_kind kind) const
  { return class_kind_ == kind; }

  const a_qualified_name &name() const { return name_; }
  void set_name(a_qualified_name name) { name_ = move(name); }

  a_const_import_scope_ptr import_scope() const
  {
    return import_scope_;
  }  /* import_scope */

  an_import_interface_ptr import_interface() const;

  mdToken token() const
  {
    return token_;
  }  /* token */

  const a_type_definition *type_definition() const;

  a_generic_instance_scope_ptr generic_instance_scope() const
  {
    return generic_instance_scope_;
  }  /* generic_instance_scope */

  void set_generic_instance_scope(
                          a_generic_instance_scope_ptr generic_instance_scope)
  {
    generic_instance_scope_ = generic_instance_scope;
  }  /* set_generic_instance_scope */

  bool is_unresolved_type() const
  {
    return is_of_class_kind(ck_unresolved);
  }  /* is_unresolved_type */

  virtual bool uses_unresolved_type(
                        a_const_class_type_wrapper_ptr &unresolved_type) const
  {
    bool uses_unresolved_type = is_unresolved_type();
    if (uses_unresolved_type) {
      unresolved_type = shared_from_this();
    }  /* if */
    return uses_unresolved_type;
  }  /* uses_unresolved_type */

  virtual a_class_type_wrapper_ptr as_class() {
    check_assertion(is_of_kind(twk_class) || is_invalid());
    return shared_from_this();
  }  /* as_class */

  virtual a_const_class_type_wrapper_ptr as_class() const
  {
    check_assertion(is_of_kind(twk_class) || is_invalid());
    return shared_from_this();
  }  /* as_class */

  virtual void write_first_part(wostringstream &buffer,
                                bool           expand_unresolved_types) const;

private:
  a_class_kind                 class_kind_;
  a_qualified_name             name_;
  a_const_import_scope_ptr     import_scope_;
  mdToken                      token_;
  a_generic_instance_scope_ptr generic_instance_scope_;
};  /* a_class_type_wrapper */


/*
A class that represents pointer and reference types imported from metadata.
*/
class a_type_indirection
  : public a_type_wrapper
  , public enable_shared_from_this<a_type_indirection>
{
public:
  enum an_indirection_kind
  {
    tik_invalid,
    tik_pointer,
    tik_interior_pointer,
    tik_handle,
    tik_reference,
    tik_rvalue_reference,
    tik_tracking_reference,
    tik_tentative_byref
  };

  a_type_indirection(an_indirection_kind indirection_kind,
                     a_type_wrapper_ptr  underlying_type)
    : a_type_wrapper(twk_indirection)
    , indirection_kind_(indirection_kind)
    , underlying_type_(move(underlying_type))
  {
    check_assertion(underlying_type_ != nullptr);
  }  /* Constructor. */

  virtual a_type_wrapper_ptr copy() const {
    return make_shared<a_type_indirection>(*this);
  }  /* copy */

  an_indirection_kind indirection_kind() const { return indirection_kind_; }
  void set_indirection_kind(an_indirection_kind indirection_kind)
  { indirection_kind_ = indirection_kind; }
  bool is_of_indirection_kind(an_indirection_kind indirection_kind) const
  { return indirection_kind_ == indirection_kind; }

  a_type_wrapper_ptr underlying_type() {
    return underlying_type_;
  }  /* underlying_type */

  a_const_type_wrapper_ptr underlying_type() const {
    return underlying_type_;
  }  /* underlying_type */

  virtual bool uses_unresolved_type(
                        a_const_class_type_wrapper_ptr &unresolved_type) const
  {
    return underlying_type_ != nullptr &&
           underlying_type_->uses_unresolved_type(unresolved_type);
  }  /* uses_unresolved_type */

  a_type_indirection_ptr as_indirection() {
    check_assertion(is_of_kind(twk_indirection));
    return shared_from_this();
  }  /* as_indirection */

  a_const_type_indirection_ptr as_indirection() const {
    check_assertion(is_of_kind(twk_indirection));
    return shared_from_this();
  }  /* as_indirection */

  virtual an_array_type_wrapper_ptr as_handle_to_array() {
    check_assertion(is_of_kind(twk_indirection));
    return is_of_indirection_kind(a_type_indirection::tik_handle) ?
                                      underlying_type()->as_array() : nullptr;
  }  /* as_handle_to_array */

  virtual a_const_array_type_wrapper_ptr as_handle_to_array() const {
    check_assertion(is_of_kind(twk_indirection));
    return is_of_indirection_kind(a_type_indirection::tik_handle) ?
                                      underlying_type()->as_array() : nullptr;
  }  /* as_handle_to_array */

  virtual void write_first_part(wostringstream &buffer,
                                bool           expand_unresolved_types) const
  {
    if (underlying_type_) {
      wstring kind_string;
      switch (indirection_kind_) {
        case tik_interior_pointer:
          if (!underlying_type_->is_of_kind(a_type_wrapper::twk_function) &&
              qualifier_flags() == qf_none) {
            buffer << L"cli::interior_ptr<"
                   << underlying_type_->get_string(expand_unresolved_types)
                   << L'>';
          } else {
            unexpected_condition();
            buffer << L"__error_type";
          }  /* if */
          break;
        case tik_pointer:            kind_string = L'*';  goto have_string;
        case tik_handle:             kind_string = L'^';  goto have_string;
        case tik_reference:          kind_string = L'&';  goto have_string;
        case tik_rvalue_reference:   kind_string = L"&&"; goto have_string;
        case tik_tracking_reference: kind_string = L'%';
have_string:
          underlying_type_->write_first_part(buffer, expand_unresolved_types);
          if (underlying_type_->is_of_kind(a_type_wrapper::twk_function)) {
            buffer << L" (";
          }  /* if */
          buffer << kind_string;
          if (qualifier_flags() != qf_none) {
            buffer << L' ';
            write_qualifiers(buffer);
          }  /* if */
          break;
        default:
          unexpected_condition();
          buffer << L"__error_type";
          break;
      }  /* switch */
    } else {
      unexpected_condition();
      buffer << L"__error_type";
    }  /* if */
  }  /* write_first_part */

  virtual void write_second_part(wostringstream &buffer,
                                 bool           expand_unresolved_types) const
  {
    if (underlying_type_) {
      switch (indirection_kind_) {
        case tik_interior_pointer:
          break;
        case tik_pointer:
        case tik_handle:
        case tik_reference:
        case tik_rvalue_reference:
        case tik_tracking_reference:
          if (underlying_type_->is_of_kind(a_type_wrapper::twk_function)) {
            buffer << L")";
          }  /* if */
          underlying_type_->write_second_part(buffer, expand_unresolved_types);
          break;
        default:
          unexpected_condition();
          break;
      }  /* switch */
    } else {
      unexpected_condition();
    }  /* if */
  }  /* write_second_part */

private:
  an_indirection_kind indirection_kind_;
  a_type_wrapper_ptr  underlying_type_;
};  /* a_type_indirection */


/*
A class that represents arrays imported from metadata.
*/
class an_array_type_wrapper
  : public a_type_wrapper
  , public enable_shared_from_this<an_array_type_wrapper>
{
public:
  enum an_array_kind
  {
    ak_invalid,
    ak_array,
    ak_param_array,
    ak_write_only_array,
  };

  static a_type_indirection_ptr create_handle_to_array(
                                    const a_type_wrapper_ptr &underlying_type,
                                    ULONG                    rank = 1,
                                    an_array_kind            kind = ak_array)
  {
    auto array_type = make_shared<an_array_type_wrapper>(underlying_type,
                                                         rank, kind);
    return make_shared<a_type_indirection>(a_type_indirection::tik_handle,
                                           move(array_type));
  }  /* create_handle_to_array */

  an_array_type_wrapper(a_type_wrapper_ptr underlying_type,
                        ULONG              rank = 1,
                        an_array_kind      kind = ak_array)
    : a_type_wrapper(twk_array)
    , array_kind_(kind)
    , underlying_type_(move(underlying_type))
    , rank_(rank)
  {
  }  /* Constructor. */

  virtual a_type_wrapper_ptr copy() const {
    return make_shared<an_array_type_wrapper>(*this);
  }  /* copy */

  an_array_kind array_kind() const { return array_kind_; }
  void set_array_kind(an_array_kind kind) { array_kind_ = kind; }
  bool is_of_array_kind(an_array_kind kind) const
  { return array_kind_ == kind; }

  a_type_wrapper_ptr &underlying_type() {
    return underlying_type_;
  }  /* underlying_type */

  a_const_type_wrapper_ptr underlying_type() const {
    return underlying_type_;
  }  /* underlying_type */

  UINT rank() const {
    return rank_;
  }  /* rank */

  virtual bool uses_unresolved_type(
                        a_const_class_type_wrapper_ptr &unresolved_type) const
  {
    return underlying_type_ != nullptr &&
           underlying_type_->uses_unresolved_type(unresolved_type);
  }  /* uses_unresolved_type */

  virtual an_array_type_wrapper_ptr as_array() {
    check_assertion(is_of_kind(twk_array));
    return shared_from_this();
  }  /* as_array */

  virtual a_const_array_type_wrapper_ptr as_array() const {
    check_assertion(is_of_kind(twk_array));
    return shared_from_this();
  }  /* as_array */

  virtual void write_first_part(wostringstream &buffer,
                                bool           expand_unresolved_types) const
  {
    if (!is_of_array_kind(ak_invalid) && underlying_type_ != nullptr) {
      if (array_kind_ == ak_param_array) {
        buffer << L"... ";
      }  /* if */
      if (qualifier_flags() != qf_none) {
        write_qualifiers(buffer);
        buffer << L' ';
      }  /* if */
      switch (array_kind_) {
        case ak_array:
        case ak_param_array:
          if (is_cppcx_metadata) {
            buffer << L"Platform::Array<";
          } else {
            buffer << L"cli::array<";
          }  /* if */
          break;
        case ak_write_only_array:
          check_assertion(is_cppcx_metadata);
          buffer << L"Platform::WriteOnlyArray<";
          break;
        default:
          unexpected_condition();
          buffer << L"__error_type<";
          break;
      }  /* switch */
      buffer << underlying_type_->get_string(expand_unresolved_types);
      if (rank_ > 1) {
        buffer << L", " << to_wstring((unsigned long long)rank_);
      }  /* if */
      buffer << L'>';
    } else {
      unexpected_condition();
      buffer << L"__error_type";
    }  /* if */
  }  /* write_first_part */

private:
  an_array_kind      array_kind_;
  a_type_wrapper_ptr underlying_type_;
  ULONG              rank_;
};  /* an_array_type_wrapper */


a_type_wrapper_ptr a_type_wrapper::create(CorElementType element_type,
                                          bool           builtin_wchar_t)
{
  a_type_wrapper_ptr type;
  switch (element_type) {
    case ELEMENT_TYPE_VOID:
      type = create(twk_void);
      break;
    case ELEMENT_TYPE_BOOLEAN:
      type = create(twk_bool);
      break;
    case ELEMENT_TYPE_CHAR:
      if (builtin_wchar_t || is_cppcx_metadata) {
        type = create(twk_wchar_t);
      } else {
        type = create(twk_unsigned_short);
      }  /* if */
      break;
    case ELEMENT_TYPE_I1:
      type = create(twk_signed_char);
      break;
    case ELEMENT_TYPE_U1:
      type = create(twk_unsigned_char);
      break;
    case ELEMENT_TYPE_I2:
      type = create(twk_short);
      break;
    case ELEMENT_TYPE_U2:
      type = create(twk_unsigned_short);
      break;
    case ELEMENT_TYPE_I4:
      type = create(twk_int);
      break;
    case ELEMENT_TYPE_U4:
      type = create(twk_unsigned_int);
      break;
    case ELEMENT_TYPE_I8:
      type = create(twk_long_long);
      break;
    case ELEMENT_TYPE_U8:
      type = create(twk_unsigned_long_long);
      break;
    case ELEMENT_TYPE_R4:
      type = create(twk_float);
      break;
    case ELEMENT_TYPE_R8:
      type = create(twk_double);
      break;
    case ELEMENT_TYPE_STRING:
      { auto string_type = a_class_type_wrapper::create_system_class(
                                                                   L"String");
        type = make_shared<a_type_indirection>(a_type_indirection::tik_handle,
                                               move(string_type));
        break;
      }
    case ELEMENT_TYPE_TYPEDBYREF:
      type = a_class_type_wrapper::create_system_class(L"TypedReference");
      break;
    case ELEMENT_TYPE_I:
      type = a_class_type_wrapper::create_system_class(L"IntPtr");
      break;
    case ELEMENT_TYPE_U:
      type = a_class_type_wrapper::create_system_class( L"UIntPtr");
      break;
    case ELEMENT_TYPE_OBJECT:
      { auto object_type = a_class_type_wrapper::create_system_class(
                                                                   L"Object");
        type = make_shared<a_type_indirection>(a_type_indirection::tik_handle,
                                               move(object_type));
        break;
      }
    default:
      unexpected_condition();
      break;
  }  /* switch */
  return type;
}  /* a_type_wrapper::create */


/*
The representation of a single method parameter.
*/
class a_method_parameter {
public:
  a_method_parameter()
    : type_(a_type_wrapper::create(a_type_wrapper::twk_invalid))
    , token_(mdParamDefNil)
    , attributes_(0)
  {
  }  /* Default constructor. */

  a_method_parameter(const an_import_scope &import_scope,
                     a_type_wrapper_ptr    type,
                     mdParamDef            token);

  wstring get_string(bool expand_unresolved_types) const {
    return type_->get_string(name_, expand_unresolved_types);
  }  /* get_string */

  void set_type(a_type_wrapper_ptr type) { type_ = move(type); }
  a_type_wrapper_ptr       type() { return type_; }
  a_const_type_wrapper_ptr type() const { return type_; }
  mdParamDef token() const { return token_; }
  wstring    name() const { return name_; }
  DWORD      attributes() const { return attributes_; }
  a_boolean  is_parameter_array(const an_import_scope &import_scope) const;
private:
  a_type_wrapper_ptr type_;       /* The type of this parameter. */
  mdParamDef         token_;      /* The token for this parameter. */
  wstring            name_;       /* The name of this parameter. */
  DWORD              attributes_; /* Attributes associated with this
                                     parameter. */
}; /* a_method_parameter */


/* A list of method parameters. */
typedef vector<a_method_parameter> a_method_parameter_list;


/*
A class that represents function types imported from metadata.
*/
class a_function_type_wrapper
  : public a_type_wrapper
  , public enable_shared_from_this<a_function_type_wrapper>
{
public:
  a_function_type_wrapper()
    : a_type_wrapper(twk_invalid)
    , generic_arity_(0)
    , calling_convention_(0)
  {
  }  /* Constructor. */

  a_function_type_wrapper(BYTE                    generic_arity,
                          BYTE                    calling_convention,
                          a_method_parameter      return_value,
                          a_method_parameter_list parameter_list)
    : a_type_wrapper(twk_function)
    , generic_arity_(generic_arity)
    , calling_convention_(calling_convention)
    , return_value_(move(return_value))
    , parameter_list_(move(parameter_list))
  {
  }  /* Constructor. */

  virtual a_type_wrapper_ptr copy() const {
    return make_shared<a_function_type_wrapper>(*this);
  }  /* copy */

  BYTE generic_arity() const {
    return generic_arity_;
  }  /* generic_arity */

  BYTE calling_convention() const {
    return calling_convention_;
  }  /* calling_convention */

  const a_method_parameter &return_value() const {
    return return_value_;
  }  /* return_type */

  a_const_type_wrapper_ptr return_type() const {
    return return_value_.type();
  }  /* return_type */

  void omit_return_type() {
    return_value_.set_type(nullptr);
  }  /* omit_return_type */

  const a_method_parameter_list &parameter_list() const {
    return parameter_list_;
  }  /* parameter_list */

  virtual a_function_type_wrapper_ptr as_function() {
    check_assertion(is_of_kind(twk_function) || is_invalid());
    return shared_from_this();
  }  /* as_function */

  virtual a_const_function_type_wrapper_ptr as_function() const {
    check_assertion(is_of_kind(twk_function) || is_invalid());
    return shared_from_this();
  }  /* as_function */

protected:
  virtual void write_first_part(wostringstream &buffer,
                                bool           expand_unresolved_types) const
  {
    if (return_type() != nullptr) {
      buffer << return_type()->get_string(expand_unresolved_types);
    }  /* if */
#if 0
    /* Calling conventions are not currently emitted. */
    switch (calling_convention_) {
      case IMAGE_CEE_CS_CALLCONV_C:
        buffer << L" __cdecl ";
        break;
      case IMAGE_CEE_CS_CALLCONV_STDCALL:
        buffer << L" __stdcall ";
        break;
      case IMAGE_CEE_CS_CALLCONV_THISCALL:
        buffer << L" __thiscall ";
        break;
      case IMAGE_CEE_CS_CALLCONV_FASTCALL:
        buffer << L" __fastcall ";
        break;
      default:
        break;
    }  /* switch */
#endif /* 0 */
  }  /* write_first_part */

  virtual void write_second_part(wostringstream &buffer,
                                 bool           expand_unresolved_types) const
  {
    /* Open the parameter list. */
    if (calling_convention_ == IMAGE_CEE_CS_CALLCONV_PROPERTY) {
      check_assertion(!parameter_list_.empty());
      buffer << L'[';
    } else {
      buffer << L'(';
    }  /* if */
    /* Write the parameters. */
    auto iter = parameter_list_.begin();
    for (; iter != parameter_list_.end(); ++iter) {
      auto &parameter = *iter;
      if (iter != parameter_list_.begin()) {
        buffer << L", ";
      }  /* if */
      buffer << parameter.get_string(expand_unresolved_types);
    }  /* for */
    /* Write the vararg parameter. */
    if (calling_convention_ == IMAGE_CEE_CS_CALLCONV_VARARG) {
      if (iter != parameter_list_.begin()) {
        buffer << L", ";
      }  /* if */
      buffer << L"...";
    }  /* if */
    /* Close the parameter list. */
    if (calling_convention_ == IMAGE_CEE_CS_CALLCONV_PROPERTY) {
      buffer << L']';
    } else {
      buffer << L')';
    }  /* if */
    /* Close the parameter list. */
    if (qualifier_flags() != qf_none) {
      buffer << L' ';
      write_qualifiers(buffer);
    }  /* if */
  }  /* write_second_part */

private:
  BYTE                    generic_arity_;
  BYTE                    calling_convention_;
  a_method_parameter      return_value_;
  a_method_parameter_list parameter_list_;
};  /* a_function_type_wrapper */


class an_element_value
{
public:
  an_element_value()
    : element_type_(ELEMENT_TYPE_VOID)
  {
  }  /* Default constructor */

  an_element_value(nullptr_t)
    : element_type_(ELEMENT_TYPE_OBJECT)
  {
    value_.unsigned_int_value = 0;
  }  /* Constructor */

  an_element_value(const wchar_t *string_value,
                   size_t        string_length = 0)
  {
    set_string_value(string_value, string_length);
  }  /* Constructor */

  an_element_value(const wstring &string_value)
  {
    set_string_value(string_value.data(), string_value.length());
  }  /* Constructor */

  an_element_value(const unique_ptr<wstring> &string_value)
  {
    if (string_value == nullptr) {
      set_string_value(nullptr);
    } else {
      set_string_value(string_value->data(), string_value->length());
    }  /* if */
  }  /* Constructor */

  an_element_value(const an_element_value &other)
    : element_type_(other.element_type_)
    , value_(other.value_)
  {
    if (element_type_ == ELEMENT_TYPE_STRING) {
      set_string_value(other.value_.string.value, other.value_.string.length);
    }  /* if */
  }  /* Copy constructor */

  an_element_value(an_element_value&& other)
  {
    other.swap(*this);
    /* The destructor for the moved-from object will inspect element_type_
       and potentially take some action based on its value, which at this
       point is the uninitialized value previously in this->element_type.
       Ensure that it has an innocuous value. */
    other.element_type_ = ELEMENT_TYPE_VOID;
  }  /* Move constructor. */

  an_element_value& operator=(an_element_value other)
  {
    other.swap(*this);
    return *this;
  }  /* Assignment operator. */

  void swap(an_element_value& other)
  {
    std::swap(element_type_, other.element_type_);
    std::swap(value_, other.value_);
  }  /* swap */

  ~an_element_value()
  {
    if (element_type_ == ELEMENT_TYPE_STRING &&
        value_.string.value != nullptr) {
      delete [] value_.string.value;
    }  /* if */
  }  /* Destructor */

  CorElementType element_type() const { return element_type_; }

  bool string_value(const wchar_t *&string_value,
                    size_t        &string_length) const
  {
    bool result = element_type_ == ELEMENT_TYPE_STRING;
    if (result) {
      string_value = value_.string.value;
      string_length = value_.string.length;
    }  /* if */
    return result;
  }  /* string_value */

  bool string_value(unique_ptr<wstring> &string_value) const
  {
    bool result = element_type_ == ELEMENT_TYPE_STRING;
    if (result) {
      if (value_.string.value != nullptr) {
        string_value = make_unique_ptr<wstring>(value_.string.value,
                                                value_.string.length);
      } else {
        string_value.reset();
      }  /* if */
    }  /* if */
    return result;
  }  /* string_value */

  void set_string_value(const wchar_t *string_value,
                        size_t        string_length = 0)
  {
    element_type_ = ELEMENT_TYPE_STRING;
    if (string_value == nullptr) {
      check_assertion(string_length == 0);
      value_.string.value = nullptr;
      value_.string.length = 0;
    } else {
      if (string_length == 0) {
        /* string_value is a NULL-terminated string. */
        string_length = wcslen(string_value);
      }  /* if */
      value_.string.value = new wchar_t[string_length];
      memcpy(value_.string.value, string_value,
             string_length * sizeof(wchar_t));
      value_.string.length = string_length;
    }  /* if */
  }  /* set_string_value */

  wstring get_source_code(a_const_type_wrapper_ptr cast_type = nullptr) const;

protected:
  CorElementType element_type_;
  union a_value {
    bool               bool_value;
    signed char        signed_char_value;
    unsigned char      unsigned_char_value;
    short              short_value;
    unsigned short     unsigned_short_value;
    int                int_value;
    unsigned int       unsigned_int_value;
    long long          long_long_value;
    unsigned long long unsigned_long_long_value;
    float              float_value;
    double             double_value;
    struct {
      wchar_t          *value;
      size_t           length;
    } string;
  } value_;

#define DEFINE_VALUE_MEMBERS(TYPE, NAME, ELEMENT_TYPE)\
protected:                                            \
  typedef TYPE a_##NAME;                              \
public:                                               \
  an_element_value(a_##NAME value)                    \
    : element_type_(ELEMENT_TYPE)                     \
  {                                                   \
    value_.NAME = value;                              \
  }                                                   \
  bool NAME(a_##NAME &value) const {                  \
    bool result = element_type_ == ELEMENT_TYPE;      \
    if (result) {                                     \
      value = value_.NAME;                            \
    }  /* if */                                       \
    return result;                                    \
  }                                                   \
  a_##NAME &NAME() {                                  \
    if (element_type_ == ELEMENT_TYPE_VOID) {         \
      element_type_ = ELEMENT_TYPE;                   \
    } else {                                          \
      check_assertion(element_type_ == ELEMENT_TYPE); \
    }  /* if */                                       \
    return value_.NAME;                               \
  }                                                   \
  a_##NAME NAME() const {                             \
    check_assertion(element_type_ == ELEMENT_TYPE);   \
    return value_.NAME;                               \
  }

  DEFINE_VALUE_MEMBERS(bool, bool_value, ELEMENT_TYPE_BOOLEAN);
  DEFINE_VALUE_MEMBERS(signed char, signed_char_value, ELEMENT_TYPE_I1);
  DEFINE_VALUE_MEMBERS(unsigned char, unsigned_char_value, ELEMENT_TYPE_U1);
  DEFINE_VALUE_MEMBERS(short, short_value, ELEMENT_TYPE_I2);
  DEFINE_VALUE_MEMBERS(unsigned short, unsigned_short_value, ELEMENT_TYPE_U2);
  DEFINE_VALUE_MEMBERS(int, int_value, ELEMENT_TYPE_I4);
  DEFINE_VALUE_MEMBERS(unsigned int, unsigned_int_value, ELEMENT_TYPE_U4);
  DEFINE_VALUE_MEMBERS(long long, long_long_value, ELEMENT_TYPE_I8);
  DEFINE_VALUE_MEMBERS(unsigned long long, unsigned_long_long_value,
                       ELEMENT_TYPE_U8);
  DEFINE_VALUE_MEMBERS(float, float_value, ELEMENT_TYPE_R4);
  DEFINE_VALUE_MEMBERS(double, double_value, ELEMENT_TYPE_R8);

#undef DEFINE_VALUE_MEMBERS
};  /* an_element_value */

/*
A class used to decode data associated with a custom attribute.
*/
class a_custom_attribute_data
{
public:
  a_custom_attribute_data()
    : import_scope_(nullptr)
    , begin_(nullptr)
    , end_(nullptr)
  {
  }  /* Default constructor. */

  a_custom_attribute_data(a_const_import_scope_ptr import_scope,
                          const BYTE               *data,
                          ULONG                    bytes_in_data)
    : import_scope_(import_scope)
    , begin_(data)
    , end_(data + bytes_in_data)
  {
  }  /* Constructor. */

  template<typename T>
  ULONG read(T &value) const
  {
    ULONG size = sizeof(T);
    if (begin_ + size <= end_) {
      const T *value_ptr = reinterpret_cast<const T*>(begin_);
      value = *value_ptr;
    } else {
      unexpected_condition();
      size = 0;
    }  /* if */
    return size;
  }  /* read */

  ULONG read(unique_ptr<wstring> &value)
  {
    ULONG size = 0;
    BYTE next_byte;
    read(next_byte);
    if (next_byte == 0xFF) {
      /* A NULL string is encoded as a single byte. */
      size = 1;
    } else {
      PCCOR_SIGNATURE signature = begin_;
      ULONG string_length = CorSigUncompressData(signature);
      size = (ULONG)(signature - begin_ + string_length);
      /* The utf8 string is not NULL terminated, so copy it to a string
         object. */
      auto utf8_string = string(reinterpret_cast<const char*>(signature),
                                string_length);
      auto wchar_string = conv_utf8_to_wchar(
                                      const_cast<char*>(utf8_string.c_str()));
      value.reset(new wstring(wchar_string));
    }  /* if */
    return size;
  }  /* read */

  ULONG read(CorSerializationType &type)
  {
    BYTE next_byte;
    read(next_byte);
    type = static_cast<CorSerializationType>(next_byte);
    return 1;
  }  /* read */

  void advance(ULONG byte_count)
  {
    check_assertion(begin_ + byte_count <= end_);
    begin_ += byte_count;
  }  /* advance */

  template<typename T>
  void read_and_advance(T &value)
  {
    advance(read(value));
  }  /* read_and_advance */

  bool empty() { return begin_ == end_; }

  a_type_wrapper_ptr read_serialized_type_and_advance();
  a_const_class_type_wrapper_ptr read_named_type_and_advance();

private:
  a_const_import_scope_ptr
                import_scope_;
                        /* The import scope from where this custom attribute
                           data originated. */
  const BYTE    *begin_;
                        /* A pointer to the current location in the data that
                           encodes the arguments to the custom attribute. */
  const BYTE    *end_;  /* A pointer to the end of the data. */
};  /* a_custom_attribute_data */


class an_attribute_argument;
typedef const an_attribute_argument a_const_attribute_argument;
typedef vector<an_attribute_argument> an_attribute_argument_list;
typedef const an_attribute_argument_list a_const_attribute_argument_list;
typedef std::shared_ptr<an_attribute_argument_list>
                                               an_attribute_argument_list_ptr;
typedef std::shared_ptr<a_const_attribute_argument_list>
                                          a_const_attribute_argument_list_ptr;

/*
A single argument to a custom attribute.
*/
class an_attribute_argument
{
public:
  an_attribute_argument()
    : serialization_type_(SERIALIZATION_TYPE_UNDEFINED)
  {
  }  /* Default constructor. */

  an_attribute_argument(a_custom_attribute_data  &data,
                        a_const_type_wrapper_ptr type,
                        wstring                  name = wstring());

  an_attribute_argument(a_const_type_wrapper_ptr type,
                        wstring                  name = wstring());

  an_attribute_argument(an_element_value value,
                        wstring          name = wstring());

  an_attribute_argument(a_const_type_wrapper_ptr type,
                        an_element_value         value,
                        wstring                  name = wstring());

  const a_const_type_wrapper_ptr &type() const { return type_; }
  const an_element_value &value() const { return value_; }
  const wstring &name() const { return name_; }

  CorSerializationType serialization_type() const
  {
    return serialization_type_;
  }  /* serialization_type */

  wstring get_source_code() const
  /*
  Returns the source code for the attribute argument.
  */
  {
    wstring result;
    if (!name_.empty()) {
      result = name_ + L" = ";
    }  /* if */
    switch (serialization_type_) {
      case SERIALIZATION_TYPE_BOOLEAN:
      case SERIALIZATION_TYPE_CHAR:
      case SERIALIZATION_TYPE_I1:
      case SERIALIZATION_TYPE_U1:
      case SERIALIZATION_TYPE_I2:
      case SERIALIZATION_TYPE_U2:
      case SERIALIZATION_TYPE_I4:
      case SERIALIZATION_TYPE_U4:
      case SERIALIZATION_TYPE_I8:
      case SERIALIZATION_TYPE_U8:
      case SERIALIZATION_TYPE_R4:
      case SERIALIZATION_TYPE_R8:
      case SERIALIZATION_TYPE_STRING:
        result += value_.get_source_code();
        break;
      case SERIALIZATION_TYPE_TAGGED_OBJECT:
      case SERIALIZATION_TYPE_ENUM:
        result += value_.get_source_code(type_);
        break;
      case SERIALIZATION_TYPE_SZARRAY:
        if (init_list_ == nullptr) {
          result += an_element_value(nullptr).get_source_code(type_);
        } else {
          result += L"gcnew ";
          result += type_->get_string() + L" { ";
          for (auto iter = init_list_->cbegin();
               iter != init_list_->cend();
               ++iter) {
            if (iter != init_list_->cbegin()) {
              result += L", ";
            }  /* if */
            result += iter->get_source_code();
          }  /* for */
          result += L" }";
        }  /* if */
        break;
      case SERIALIZATION_TYPE_TYPE:
        result += type_->get_string() + L"::typeid";
        break;
      default:
        result.clear();
        unexpected_condition();
        break;
    }  /* switch */
    return result;
  }  /* get_source_code */

private:
  a_const_attribute_argument_list_ptr array_init_list() const {
    return serialization_type_ == SERIALIZATION_TYPE_SZARRAY ? init_list_
                                                             : nullptr;
  }  /* array_init_list */
  void value_constructor_helper();

  a_const_type_wrapper_ptr
                type_;
                        /* If serialization_type_ == SERIALIZATION_TYPE_TYPE,
                           the type to be used in the typeid expression;
                           otherwise, the type of the argument. */
  CorSerializationType
                serialization_type_;
                        /* The serialization type of the argument. */
  an_element_value
                value_; /* If the argument is a fundamental, enum, or string
                           type, this contains the argument's value. */
  an_attribute_argument_list_ptr
                init_list_;
                        /* If the argument is an array type, i.e.
                           serialization_type_ == SERIALIZATION_TYPE_SZARRAY,
                           the list of array initializers, or nullptr for a
                           NULL array as opposed to an empty one. */
  wstring       name_;
                        /* The argument's name if it is a named argument, or
                           the empty string if it is a fixed argument. */
};  /* an_attribute_argument */


/*
Flags that represent standard custom attributes.
*/
typedef unsigned int a_standard_attribute_flag_set;
enum a_standard_attribute_flag : a_standard_attribute_flag_set
{
  saf_none                       = 0x00000000,
  saf_ide_custom_attribute       = 0x10000000,
  saf_cli_custom_attribute       = 0x20000000,
                                   /* The attribute is specific to C++/CLI. */
  saf_cppcx_custom_attribute     = 0x40000000,
                                   /* The attribute is specific to C++/CX. */
  saf_attribute_usage_attribute  = 0x00000001,
  saf_default_member_attribute   = 0x00000002,
  saf_flags_attribute            = 0x00000004,
  saf_obsolete_attribute         = 0x00000008 | saf_cli_custom_attribute,
  saf_browsable_attribute        = 0x00000010 | saf_ide_custom_attribute
                                              | saf_cli_custom_attribute,
  saf_editor_browsable_attribute = 0x00000020 | saf_ide_custom_attribute
                                              | saf_cli_custom_attribute,
  saf_description_attribute      = 0x00000040 | saf_ide_custom_attribute
                                              | saf_cli_custom_attribute,
  saf_help_keyword_attribute     = 0x00000080 | saf_ide_custom_attribute
                                              | saf_cli_custom_attribute,
  saf_display_name_attribute     = 0x00000100 | saf_ide_custom_attribute
                                              | saf_cli_custom_attribute,
  saf_allow_multiple_attribute   = 0x00010000 | saf_cppcx_custom_attribute,
  saf_deprecated_attribute       = 0x00020000 | saf_cppcx_custom_attribute,
};  /* a_standard_attribute_flag */


/*
The representation of a single custom attribute.
*/
class a_custom_attribute
{
public:
  a_custom_attribute()
    : import_scope_(nullptr)
    , token_(mdCustomAttributeNil)
    , ctor_token_(mdTokenNil)
    , signature_(nullptr)
    , bytes_in_signature_(0)
  {
  }  /* Default constructor. */

  a_custom_attribute(a_const_import_scope_ptr import_scope,
                     mdCustomAttribute        token)
    : import_scope_(import_scope)
    , token_(token)
    , ctor_token_(mdTokenNil)
    , signature_(nullptr)
    , bytes_in_signature_(0)
  {
  }  /* Constructor. */

  a_const_attribute_argument_list &fixed_args() const
  {
    decode();
    return fixed_args_;
  }  /* fixed_args */

  a_const_attribute_argument_list &named_args() const
  {
    decode();
    return named_args_;
  }  /* named_args */

  mdCustomAttribute token() const { return token_; }
  mdToken ctor_token() const { decode_type(); return ctor_token_; }

  a_const_class_type_wrapper_ptr class_type() const
  {
    decode_type();
    return class_type_;
  }  /* class_type */

  a_qualified_name type_name() const { return class_type()->name(); }

  bool is_standard_attribute(a_standard_attribute_flag saf) const;

  bool requires_type_definition(
                              const a_type_definition &type_definition) const;
  
  wstring get_source_code(const wstring &attribute_target = wstring()) const
  /*
  Returns the source code for the custom attribute.
  */
  {
    wstring result;
    result += L'[';
    result += attribute_target;
    result += type_name().as_string();
    result += L"(";
    for (auto &fixed_arg : fixed_args()) {
      result += fixed_arg.get_source_code();
      result += L", ";
    }  /* for */
    for (auto &named_arg : named_args()) {
      result += named_arg.get_source_code();
      result += L", ";
    }  /* for */
    if (!fixed_args().empty() || !named_args().empty()) {
      result.resize(result.size() - (_countof(L", ")-1));
    }  /* if */
    result += L")]";
    result += L_END_OF_LINE;
    return result;
  }  /* get_source_code */

private:  
  an_import_interface_ptr import_interface() const;
  void decode_type() const;
  void decode_fixed_args() const;
  void decode_named_args() const;

  void decode() const
  {
    /* Decode the entire attribute to detect any uses of unresolved types.
       If that is the case, class_type_ will be set to the first encountered
       unresolved type and fixed_args_ and named_args_ will be empty. */
    decode_named_args();
  }  /* decode */

  a_const_import_scope_ptr
                import_scope_;
                        /* The import scope associated with this attribute. */
  mdCustomAttribute
                token_;
                        /* The token of the custom attribute. */
  mutable mdToken
                ctor_token_;
                        /* The mdMethodDef or mdMemberRef token of the custom
                           attribute's constructor. */
  mutable PCCOR_SIGNATURE
                signature_;
                        /* The signature of the custom attribute
                           constructor. */
  mutable ULONG bytes_in_signature_;
                        /* The number of bytes in the signature of the custom
                           attribute constructor. */
  mutable a_custom_attribute_data
                data_;
                        /* The data that encodes custom attribute's
                           arguments. */
  mutable a_const_class_type_wrapper_ptr
                class_type_;
                        /* The custom attribute's class type. */
  mutable an_attribute_argument_list
                fixed_args_;
                        /* The fixed arguments to the custom attribute's
                           constructor. */
  mutable an_attribute_argument_list
                named_args_;
                        /* The custom attribute's named arguments. */
};  /* a_custom_attribute */


/*
A wrapper for the System.Reflection.DefaultMemberAttribute custom attribute.
*/
class a_default_member_attribute
{
public:
  a_default_member_attribute()
  {
  }  /* Default constructor. */

  a_default_member_attribute(const wstring &member_name)
    : member_name_(member_name)
  {
  }  /* Constructor. */

  a_default_member_attribute(wstring &&member_name)
    : member_name_(move(member_name))
  {
  }  /* Constructor. */

  static bool create(const a_custom_attribute   &attribute,
                     a_default_member_attribute &default_member_attribute)
  /* Process the DefaultMemberAttribute and initialize the specified
     default_member_attribute.  The constructor is expected to be of the form:
        DefaultMemberAttribute(String^ member_name);
     Returns true if the attribute was successfully processed; otherwise
     return false.
  */
  {
    bool result = false;
    if (attribute.is_standard_attribute(saf_default_member_attribute)) {
      auto &fixed_args = attribute.fixed_args();
      if (fixed_args.size() == 1) {
        auto                &first_arg = fixed_args.front();
        unique_ptr<wstring> member_name;
        if (first_arg.value().string_value(member_name) &&
            member_name != nullptr && !member_name->empty()) {
          default_member_attribute = a_default_member_attribute(
                                                          move(*member_name));
          result = true;
        } else {
          unexpected_condition();
        }  /* if */
      } else {
        unexpected_condition();
      }  /* if */
    }  /* if */
    return result;
  }  /* create */

  const wstring &member_name() const
  {
    return member_name_;
  }  /* member_name */

private:
  wstring       member_name_;
                        /* The string passed to the DefaultMemberAttribute
                           constructor. */
};  /* a_default_member_attribute */


/*
A class that handles obtaining the list of custom attributes for a given
token and extracting information from them as required.
*/
class a_custom_attribute_list {
public:
  a_custom_attribute_list(a_const_import_scope_ptr import_scope,
                          mdToken                  token);

  mdToken token() const { return token_; }

  const a_default_member_attribute *default_member_attribute() const
  {
    return has_attribute(saf_default_member_attribute)
                                       ? &default_member_attribute_ : nullptr;
  }  /* default_member_attribute */

  wstring get_source_code(
                 const a_type_definition *type_definition_context,
                 const wstring           &attribute_target = wstring()) const;

private:
  bool has_attribute(a_standard_attribute_flag saf) const
  {
    return (standard_attribute_flags_ & saf) != 0;
  }  /* has_attribute */

  bool process_standard_attribute(a_standard_attribute_flag saf,
                                  const a_custom_attribute &custom_attribute);
  bool process_attribute(const a_custom_attribute &custom_attribute);

  an_import_interface_ptr import_interface() const;

  a_const_import_scope_ptr
                import_scope_;
                        /* The import scope associated with the custom
                           attributes. */
  mdToken       token_;
                        /* The token associated with the custom attributes. */
  vector<a_custom_attribute>
                custom_attributes_;
                        /* The list of custom attributes. */
  a_standard_attribute_flag_set
                standard_attribute_flags_;
                        /* A set of a_standard_attribute_flag values that
                           indicates the standard attribute members that are
                           valid. */
  a_default_member_attribute
                default_member_attribute_;
                        /* The DefaultMemberAttribute instance.  Only valid if
                           standard_attribute_flags_ contains
                           saf_default_member_attribute. */
};  /* a_custom_attribute_list */


/*
A class to decode a CLR constant.
*/
class a_constant_value : public an_element_value {
public:
  a_constant_value(DWORD         constant_type,
                   UVCP_CONSTANT constant_value,
                   ULONG         characters_in_constant)
  {
    element_type_ = static_cast<CorElementType>(constant_type);
    switch (element_type_) {
      case ELEMENT_TYPE_BOOLEAN:
        convert_value(constant_value, value_.bool_value);
        break;
      case ELEMENT_TYPE_I1:
        convert_value(constant_value, value_.signed_char_value);
        break;
      case ELEMENT_TYPE_U1:
        convert_value(constant_value, value_.unsigned_char_value);
        break;
      case ELEMENT_TYPE_I2:
        convert_value(constant_value, value_.short_value);
        break;
      case ELEMENT_TYPE_CHAR:
      case ELEMENT_TYPE_U2:
        convert_value(constant_value, value_.unsigned_short_value);
        break;
      case ELEMENT_TYPE_I4:
        convert_value(constant_value, value_.int_value);
        break;
      case ELEMENT_TYPE_U4:
        convert_value(constant_value, value_.unsigned_int_value);
        break;
      case ELEMENT_TYPE_I8:
        convert_value(constant_value, value_.long_long_value);
        break;
      case ELEMENT_TYPE_U8:
        convert_value(constant_value, value_.unsigned_long_long_value);
        break;
      case ELEMENT_TYPE_R4:
        convert_value(constant_value, value_.float_value);
        break;
      case ELEMENT_TYPE_R8:
        convert_value(constant_value, value_.double_value);
        break;
      case ELEMENT_TYPE_STRING:
        set_string_value(static_cast<const wchar_t*>(constant_value),
                         characters_in_constant);
        break;
      case ELEMENT_TYPE_OBJECT:
        /* The nullptr constant. */
        convert_value(constant_value, value_.unsigned_int_value);
        check_assertion(value_.unsigned_int_value == 0);
        break;
      default:
        unexpected_condition();
        break;
    }  /* switch */
  }  /* constructor */

private:

  template<typename T>
  void convert_value(UVCP_CONSTANT constant_value, T &value)
  /*
  Cast the constant blob to the correct type.
  */
  {
    value = *reinterpret_cast<UNALIGNED const T*>(constant_value);
  }  /* convert */

}; /* a_constant_value. */


/*
Classes for importing an assembly and a scope with an assembly.
*/

/*
The representation of a single assembly.  Note: an assembly may contain
multiple import scopes, but only one import scope contains metadata.
*/
class an_assembly {
public:
  an_assembly(const wstring             &assembly_path,
              an_assembly_index         assembly_index,
              IALink                    *alink_interface,
              mdFile                    file_token,
              a_cpp_cli_import_flag_set import_flags)
    : assembly_path_(assembly_path),
      assembly_index_(assembly_index),
      alink_interface_(alink_interface),
      alink_token_(mdTokenNil),
      import_flags_(import_flags),
      resolution_scope_(mdTokenNil),
      file_token_(file_token),
      is_cppcx_metadata_(false),
      count_of_scopes_(0)
  {
  }  /* constructor */


  an_assembly(an_assembly&& other)
    : assembly_path_(move(other.assembly_path_)),
      assembly_name_(move(other.assembly_name_)),
      assembly_index_(move(other.assembly_index_)),
      alink_interface_(move(other.alink_interface_)),
      alink_token_(move(other.alink_token_)),
      import_flags_(move(other.import_flags_)),
      md_assembly_import_interface_(
                                   move(other.md_assembly_import_interface_)),
      resolution_scope_(move(other.resolution_scope_)),
      file_token_(move(other.file_token_)),
      is_cppcx_metadata_(other.is_cppcx_metadata_),
      count_of_scopes_(move(other.count_of_scopes_)),
      imported_scopes_(move(other.imported_scopes_))
  {
  }  /* constructor */


  an_assembly& operator=(an_assembly&& other)
  {
    assembly_path_ = move(other.assembly_path_);
    assembly_name_ = move(other.assembly_name_);
    assembly_index_ = move(other.assembly_index_);
    alink_interface_ = move(other.alink_interface_);
    alink_token_ = move(other.alink_token_);
    import_flags_ = move(other.import_flags_);
    md_assembly_import_interface_ = move(other.md_assembly_import_interface_);
    resolution_scope_ = move(other.resolution_scope_);
    file_token_ = move(other.file_token_);
    is_cppcx_metadata_ = other.is_cppcx_metadata_;
    count_of_scopes_ = move(other.count_of_scopes_);
    imported_scopes_ = move(other.imported_scopes_);
    return *this;
  }  /* operator= */

  bool process();

  a_cpp_cli_import_flag_set import_flags() const
  {
    return import_flags_;
  }  /* import_flags */

  an_assembly_index assembly_index() const
  {
    return assembly_index_;
  }  /* assembly_index */

  bool is_cppcx_metadata() const
  {
    return is_cppcx_metadata_;
  }  /* is_cppcx_metadata */

  bool is_platform_winmd() const
  {
    bool is_platform_winmd = false;
    if (cppcx_enabled && assembly_index() == 1) {
      auto last_backslash_index = assembly_path_.rfind(L'\\');
      auto compare_index = (last_backslash_index == wstring::npos) ?
                                                 0 : last_backslash_index + 1;
      is_platform_winmd = _wcsicmp(assembly_path_.c_str() + compare_index,
                                   L"platform.winmd") == 0;
    }  /* if */
    return is_platform_winmd;
  }

  void import_all_types(ostringstream &buffer);
  void import_all_types(char   *buffer,
                        size_t *buffer_size);

#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES
  void create_portable_assembly(a_const_char *assembly_name);
#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES */

  const wstring &assembly_path() const
  {
    return assembly_path_;
  }  /* assembly_name */

  const an_assembly_name &assembly_name() const
  {
    return assembly_name_;
  }  /* assembly_name */

  void init_assembly_name()
  {
    HRESULT          hr;
    mdAssembly       token;
    ASSEMBLYMETADATA data = {0};
    const void       *public_key;
    ULONG            bytes_in_public_key = 0;
    WCHAR            *name_buffer;
    ULONG            characters_in_name;
    DWORD            flags;
    wstring          name;

    token = mdAssemblyNil;
    hr = md_assembly_import_interface_->GetAssemblyFromScope(&token);
    CHECK_API_RESULT(hr, GetAssemblyFromScope);
    hr = md_assembly_import_interface_->GetAssemblyProps(
                                            token,
                                            /*ppbPublicKeyOrToken=*/nullptr,
                                            &bytes_in_public_key,
                                            /*pulHashAlgId=*/nullptr,
                                            /*szName=*/nullptr, /*cchName=*/0,
                                            &characters_in_name, &data,
                                            &flags);
    CHECK_API_RESULT(hr, GetAssemblyProps);
    name_buffer = reinterpret_cast<WCHAR*>(_alloca(characters_in_name *
                                                              sizeof(WCHAR)));
    if (data.cbLocale > 0) {
      data.szLocale = reinterpret_cast<WCHAR*>(_alloca(data.cbLocale *
                                                              sizeof(WCHAR)));
    }  /* if */
    hr = md_assembly_import_interface_->GetAssemblyProps(
                                              token,
                                              &public_key,
                                              &bytes_in_public_key,
                                              /*pulHashAlgId=*/nullptr,
                                              name_buffer, characters_in_name,
                                              &characters_in_name, &data,
                                              &flags);
    CHECK_API_RESULT(hr, GetAssemblyRefProps);
    name.assign(name_buffer, characters_in_name - 1);
    assembly_name_ = an_assembly_name(name, data,
                                      public_key, bytes_in_public_key,
                                      flags);
  }  /* init_assembly_name */

  an_assembly_name get_assembly_ref_name(mdAssemblyRef token) const
  {
    HRESULT          hr;
    ASSEMBLYMETADATA data = {0};
    const void       *public_key_or_token;
    ULONG            bytes_in_public_key_or_token = 0;
    WCHAR            *name_buffer;
    ULONG            characters_in_name;
    DWORD            flags;
    wstring          name;

    check_assertion(TypeFromToken(token) == mdtAssemblyRef);
    hr = md_assembly_import_interface_->GetAssemblyRefProps(
                                            token,
                                            /*ppbPublicKeyOrToken=*/nullptr,
                                            &bytes_in_public_key_or_token,
                                            /*szName=*/nullptr, /*cchName=*/0,
                                            &characters_in_name, &data,
                                            /*ppbHashValue=*/NULL,
                                            /*pcbHashValue*/NULL,
                                            &flags);
    CHECK_API_RESULT(hr, GetAssemblyRefProps);
    name_buffer = reinterpret_cast<WCHAR*>(_alloca(characters_in_name *
                                                              sizeof(WCHAR)));
    if (data.cbLocale > 0) {
      data.szLocale = reinterpret_cast<WCHAR*>(_alloca(data.cbLocale *
                                                              sizeof(WCHAR)));
    }  /* if */
    hr = md_assembly_import_interface_->GetAssemblyRefProps(
                                              token,
                                              &public_key_or_token,
                                              &bytes_in_public_key_or_token,
                                              name_buffer, characters_in_name,
                                              &characters_in_name, &data,
                                              /*ppbHashValue=*/NULL,
                                              /*pcbHashValue*/NULL,
                                              &flags);
    CHECK_API_RESULT(hr, GetAssemblyRefProps);
    name.assign(name_buffer, characters_in_name - 1);
    return an_assembly_name(name, data, public_key_or_token,
                            bytes_in_public_key_or_token, flags);
  }  /* get_assembly_ref_name */

  an_import_scope& import_scope_from_index(int scope_index);
  a_const_import_scope_ptr find_import_scope_by_name(
                                             const wstring &scope_name) const;
  bool find_nested_type_by_name(
                              a_const_import_scope_ptr import_scope,
                              mdTypeDef                parent_token,
                              const wstring            &type_name,
                              mdTypeDef                &resolved_token) const;
  bool find_type_by_name(const wstring            &type_name,
                         a_const_import_scope_ptr &resolved_import_scope,
                         mdTypeDef                &resolved_token) const;

private:
  bool get_assembly_info();
  bool init_assembly_import_interface();
  bool import_all_scopes();

private:
  wstring       assembly_path_;
                        /* The full path of the assembly. */
  an_assembly_name
                assembly_name_;
                        /* The name of the assembly. */
  an_assembly_index
                assembly_index_;
                        /* The index of this assembly. */
  IALink        *alink_interface_;
                        /* The interface to the functionality provided by
                           alink.dll. */
  mdToken       alink_token_;
                        /* The token to be used when calling any IALink
                           API. */
  a_cpp_cli_import_flag_set
                import_flags_;
                        /* Flags which control the import behavior. */
  CComPtr<IMetaDataAssemblyImport>
                md_assembly_import_interface_;
                        /* The IMetaDataAssemblyImport interface. */
  mdToken       resolution_scope_;
                        /* The resolution scope token for the assembly. */
  mdFile        file_token_;
                        /* The token for the current translation unit. */
  DWORD         count_of_scopes_;
                        /* The number of scopes in this assembly. */
  vector<unique_ptr<an_import_scope>>
                imported_scopes_;
                        /* The scopes that we imported from this assembly. */
  bool          is_cppcx_metadata_;
};  /* an_assembly */

typedef const an_assembly a_const_assembly;
typedef an_assembly* an_assembly_ptr;
typedef a_const_assembly* a_const_assembly_ptr;

/*
The representation of a single import scope.  Each assembly can contain one
or more import scope - though only one import scope has the metadata for
types.
*/
class an_import_scope {
public:
  an_import_scope(a_scope_index           scope_index,
                  an_import_interface_ptr import_interface,
                  a_const_assembly_ptr    containing_assembly);

  an_import_scope(an_import_scope&& other);

  an_import_scope &operator=(an_import_scope&& other);

  const a_type_definition &get_type_definition(mdTypeDef token) const;

  a_const_method_definition_ptr get_method_definition(
                                                     mdMethodDef token) const;

  an_import_interface_ptr import_interface() const
  {
    return import_interface_;
  }  /* import_interface */

  a_const_assembly &containing_assembly() const
  {
    return *containing_assembly_;
  }  /* containing_assembly */

  a_scope_index scope_index() const
  {
    return scope_index_;
  }  /* scope_index */

  an_assembly_scope_index assembly_scope_index() const
  {
    return make_assembly_scope_index(containing_assembly_->assembly_index(),
                                     scope_index_);
  }  /* assembly_scope_index */

  const wstring &scope_name() const { return scope_name_; }

  void import_all_types(ostringstream &buffer);

  void import_one_type(
              ostringstream                  &buffer,
              mdTypeDef                      typedef_token,
              bool                           at_top_level,
              bool                           want_definition,
              bool                           class_body_only,
              a_pending_constraint_type_list *pending_constraint_types,
              a_boolean                      *is_delegate) const;

  a_qualified_name name_from_typedef(
          mdTypeDef                       token,
          const a_signature_decoder_scope &scope,
          BYTE                            &generic_parameter_count,
          a_const_class_type_wrapper_ptr  &unresolved_generic_argument) const;

  mdToken get_associated_event_or_property(mdToken method_token) const;

  a_cli_operator_kind rename_cli_operator(wstring &method_name,
                                          DWORD   method_attributes) const;

  wstring get_overridden_name(
        const a_signature_decoder_scope &scope,
        mdToken                         method_token,
        a_const_class_type_wrapper_ptr  enclosing_class_type = nullptr) const;

private:
  a_const_class_type_wrapper_ptr type_from_typedef(
                                 const a_signature_decoder_scope &scope,
                                 mdTypeDef                       token) const;

  a_const_class_type_wrapper_ptr type_from_typeref(
                                 const a_signature_decoder_scope &scope,
                                 mdTypeRef                       token) const;
public:
  a_const_class_type_wrapper_ptr type_from_token(
                                 const a_signature_decoder_scope &scope,
                                 mdToken                         token) const;

  BYTE get_generic_parameter_count(mdTypeDef token) const;

  const a_custom_attribute_list &get_custom_attributes(mdToken token) const
  {
    check_assertion(!IsNilToken(token));
    auto iter = map_token_to_custom_attributes_.lower_bound(token);
    if (iter == map_token_to_custom_attributes_.end() ||
        map_token_to_custom_attributes_.key_comp()(token, iter->first)) {
      /* This is the first request for a custom attribute list for this
         token. */
      a_custom_attribute_list attributes(this, token);
      iter = map_token_to_custom_attributes_.emplace_hint(iter, token,
                                                          move(attributes));
    }  /* if */
    return iter->second;
  }  /* get_custom_attributes */

private:
  void set_namespace_scope(ostringstream    &buffer,
                           a_qualified_name namespace_name) const;
  void open_namespace(ostringstream           &buffer,
                      wstring::const_iterator namespace_begin,
                      wstring::const_iterator namespace_end) const;
  void close_namespace(ostringstream           &buffer,
                       wstring::const_iterator namespace_begin,
                       wstring::const_iterator namespace_end) const;
  void close_all_namespace_scopes(ostringstream &buffer) const {
    set_namespace_scope(buffer, a_qualified_name());
  }  /* close_all_namespace_scopes */

private:
  a_scope_index scope_index_;
                        /* The index of this scope. */
  wstring       scope_name_;
                        /* The name of this scope. */
  a_const_assembly_ptr
                containing_assembly_;
                        /* The assembly that contains this scope. */
  an_import_interface_ptr
                import_interface_;
                        /* The IMetaDataImport2 interface. */
  mutable a_qualified_name
                active_namespace_;
                        /* The stack of active namespaces. */
  mutable map<mdTypeDef, const a_type_definition>
                map_typedef_to_definition_;
                        /* A mapping from a mdTypeDef to its type
                           definition. */
  mutable map<mdTypeRef, a_const_class_type_wrapper_ptr>
                map_typeref_to_class_type_;
                        /* A mapping from a mdTypeRef to the class type. */
  mutable map<mdToken, const a_custom_attribute_list>
                map_token_to_custom_attributes_;
                        /* A mapping from an mdToken to its corresponding
                           custom attribute list. */
}; /* an_import_scope */


an_import_interface_ptr a_class_type_wrapper::import_interface() const
{
  return import_scope_->import_interface();
}  /* a_class_type_wrapper::import_interface */


const a_type_definition *a_class_type_wrapper::type_definition() const
{
  const a_type_definition *type_definition = nullptr;
  if (is_of_class_kind(ck_class) && import_scope_ != nullptr) {
    check_assertion(!IsNilToken(token_) &&
                    TypeFromToken(token_) == mdtTypeDef);
    type_definition = &import_scope_->get_type_definition(token_);
  }  /* if */
  return type_definition;
}  /* a_class_type_wrapper::type_definition */


void a_class_type_wrapper::write_first_part(
                               wostringstream &buffer,
                               bool           expand_unresolved_types) const
{
  if (!is_of_class_kind(ck_invalid)) {
    if (qualifier_flags() != qf_none) {
      write_qualifiers(buffer);
      buffer << L' ';
    }  /* if */
#if 0
    /* Emit the "__unresolved_type" construct if requested. */
    if (is_of_class_kind(ck_unresolved) && expand_unresolved_types &&
        (import_scope_->containing_assembly().import_flags()
                                           & cpp_cli_define_all_types) == 0) {
      /* Form the __unresolved_type(<assembly_index>, <type_token>, "name")
         construct. */
      an_element_value name_string(name_.as_string());
      buffer << L"__unresolved_type(0x" << setw(8) << setfill(L'0') << hex
             << import_scope_->assembly_scope_index()
             << L", 0x" << token_ << L", " << name_string.get_source_code()
             << L')';
    } else {
      buffer << name_.as_string();
    }  /* if */
#else /* 0 */
    buffer << name_.as_string();
#endif /* 0 */
  } else {
    unexpected_condition();
    buffer << L"__error_type";
  }  /* if */
}  /* a_class_type_wrapper::write_first_part */


class a_signature_decoder_scope abstract
{
public:
  a_signature_decoder_scope(a_const_import_scope &import_scope)
    : import_scope_(import_scope)
  {
  }  /* Constructor. */

  a_const_import_scope &import_scope() const
  {
    return import_scope_;
  }  /* import_scope */

  an_import_interface_ptr import_interface() const
  {
    return import_scope_.import_interface();
  }  /* import_interface */

  a_const_class_type_wrapper_ptr type_from_token(mdToken token) const
  {
    return import_scope_.type_from_token(*this, token);
  }  /* type from token */

  virtual bool is_system_string_scope() const = 0;
  virtual bool is_generic_instance_scope() const = 0;
  virtual const a_generic_argument_list &generic_type_arguments() const = 0;
  virtual const a_generic_argument_list &generic_method_arguments() const = 0;

protected:
  a_const_import_scope &import_scope_;
};  /* a_signature_decoder_scope */


class a_generic_instance_scope
  : public a_signature_decoder_scope
{
public:
  a_generic_instance_scope(a_const_import_scope    &import_scope,
                           a_generic_argument_list generic_type_arguments)
    : a_signature_decoder_scope(import_scope)
    , generic_type_arguments_(move(generic_type_arguments))
  {
  }  /* Constructor. */

  a_generic_instance_scope(a_const_import_scope    &import_scope,
                           a_generic_argument_list generic_type_arguments,
                           a_generic_argument_list generic_method_arguments)
    : a_signature_decoder_scope(import_scope)
    , generic_type_arguments_(move(generic_type_arguments))
    , generic_method_arguments_(move(generic_method_arguments))
  {
  }  /* Constructor. */

  virtual bool is_system_string_scope() const
  {
    return false;
  }  /* is_system_string_scope */

  virtual bool is_generic_instance_scope() const
  {
    return true;
  }  /* is_generic_instance_scope */

  virtual const a_generic_argument_list &generic_type_arguments() const
  {
    return generic_type_arguments_;
  }  /* generic_type_arguments */

  virtual const a_generic_argument_list &generic_method_arguments() const
  {
    return generic_method_arguments_;
  }  /* generic_method_arguments */

private:
  a_generic_argument_list generic_type_arguments_;
  a_generic_argument_list generic_method_arguments_;
};  /* a_generic_instance_scope */


/*
A class to decode a CLR signature.
*/
class a_signature_decoder {
public:
  static a_type_wrapper_ptr decode_type(
                           const a_signature_decoder_scope &scope,
                           PCCOR_SIGNATURE                 signature,
                           ULONG                           bytes_in_signature)
  {
    a_signature_decoder decoder(scope, signature, bytes_in_signature);
    return decoder.decode_type();
  }  /* decode_type */

  static a_type_wrapper_ptr decode_field(
                           const a_signature_decoder_scope &scope,
                           PCCOR_SIGNATURE                 signature,
                           ULONG                           bytes_in_signature)
  {
    a_signature_decoder decoder(scope, signature, bytes_in_signature);
    decoder.skip_calling_convention(IMAGE_CEE_CS_CALLCONV_FIELD);
    return decoder.decode_type();
  }  /* decode_field */

  static a_function_type_wrapper_ptr decode_method(
                           const a_signature_decoder_scope &scope,
                           mdToken                         token,
                           PCCOR_SIGNATURE                 signature,
                           ULONG                           bytes_in_signature)
  {
    a_signature_decoder decoder(scope, signature, bytes_in_signature);
    return decode_method(decoder, token);
  }  /* decode_method */

  static a_function_type_wrapper_ptr decode_method(
                                                 a_signature_decoder &decoder,
                                                 mdToken             token);

private:
  a_signature_decoder(const a_signature_decoder_scope &scope,
                      PCCOR_SIGNATURE                 signature,
                      ULONG                           bytes_in_signature)
    : scope_(scope),
      signature_(signature),
      bytes_in_signature_(bytes_in_signature),
      index_(0),
      contains_unknown_optional_type_modifiers_(false)
  {
  }  /* constructor */

  a_type_wrapper_ptr decode_modified_type(CorElementType element_type);

  a_type_wrapper_ptr decode_raw_type();

  a_type_wrapper_ptr decode_type();

  a_boolean contains_unknown_optional_type_modifiers()
  {
    return contains_unknown_optional_type_modifiers_;
  }  /* contains_unknown_optional_type_modifiers */

private:
  BYTE read_one_byte()
  {
    check_assertion(index_ < bytes_in_signature_);
    return signature_[index_++];
  }  /* read_one_byte */


  ULONG read_four_bytes()
  {
    ULONG value;

    check_assertion(index_ < bytes_in_signature_);
    index_ += CorSigUncompressData(&signature_[index_], &value);
    return value;
  }  /* read_four_bytes */


  mdToken read_token()
  {
    mdToken value;

    check_assertion(index_ < bytes_in_signature_);
    index_ += CorSigUncompressToken(&signature_[index_], &value);
    return value;
  }  /* read_token */


  CorElementType peek_element_type()
  {
    check_assertion(index_ < bytes_in_signature_);
    return static_cast<CorElementType>(signature_[index_]);
  }  /* peek_element_type */


  CorElementType get_element_type()
  {
    check_assertion(index_ < bytes_in_signature_);
    return static_cast<CorElementType>(signature_[index_++]);
  }  /* get_element_type */


  void skip_calling_convention(CorCallingConvention expected_value)
  {
    check_assertion(index_ < bytes_in_signature_);
    check_assertion((signature_[index_] & IMAGE_CEE_CS_CALLCONV_MASK) ==
                                                              expected_value);
    ++index_;
  }  /* skip_calling_convention */

  a_generic_instance_scope_ptr decode_generic_arguments();

private:
  const a_signature_decoder_scope
                &scope_;
                        /* The scope in which this signature is being
                           decoded. */
  const PCCOR_SIGNATURE
                signature_;
                        /* The signature we want to decode. */
  const ULONG   bytes_in_signature_;
                        /* The number of bytes in the signature. */
  ULONG         index_;
                        /* The current index into the signature. */
  a_boolean     contains_unknown_optional_type_modifiers_;
                        /* TRUE if this signature contains any unknown
                           optional type modifiers (modopts). */
}; /* a_signature_decoder */


class a_global_scope
  : public a_signature_decoder_scope
{
public:
  a_global_scope(a_const_import_scope &import_scope)
    : a_signature_decoder_scope(import_scope)
  {
  }  /* Constructor. */

  virtual bool is_system_string_scope() const
  {
    return false;
  }  /* is_system_string_scope */

  virtual bool is_generic_instance_scope() const
  {
    return false;
  }  /* is_generic_instance_scope */

  virtual const a_generic_argument_list &generic_type_arguments() const
  {
    unexpected_condition();
    return no_generic_arguments;
  }  /* generic_type_arguments */

  virtual const a_generic_argument_list &generic_method_arguments() const
  {
    unexpected_condition();
    return no_generic_arguments;
  }  /* generic_method_arguments */
};  /* a_global_scope */


/*
A class that represents generic parameters from metadata.
*/
class a_generic_parameter
{
public:
  a_generic_parameter(wstring            name,
                      mdGenericParam     token,
                      DWORD              flags)
    : name_(move(name))
    , token_(token)
    , flags_(flags)
  {
  }  /* Constructor. */

  const wstring &name() const { return name_; }
  void set_name(wstring name) { name_ = move(name); }
  mdGenericParam token() const { return token_; }
  DWORD flags() const { return flags_; }

  a_boolean is_constrained() const
  {
    return (flags_ & gpSpecialConstraintMask) != gpNoSpecialConstraint ||
           !constraints_.empty();
  }  /* is_constrained */

  const a_generic_constraint_list &get_constraints() const
  {
    return constraints_;
  }  /* get_constraints */

  void set_constraints(a_generic_constraint_list constraints)
  {
    constraints_ = move(constraints);
  }  /* set_constraints */

  wstring constraint_clause() const
  {
    wstring constraints;
    if (is_constrained()) {
      constraints = L"where " + name_ + L" : ";
      if (flags_ & gpDefaultConstructorConstraint) {
          constraints += L"gcnew(), ";
      }  /* if */
      if (flags_ & gpReferenceTypeConstraint) {
        check_assertion((flags_ & gpNotNullableValueTypeConstraint) == 0);
        constraints += L"ref class, ";
      }  /* if */
      if (flags_ & gpNotNullableValueTypeConstraint) {
        check_assertion((flags_ & gpReferenceTypeConstraint) == 0);
        constraints += L"value class, ";
      }  /* if */
      for (auto &constraint : constraints_) {
        constraints += constraint->get_string();
        constraints += L", ";
      }  /* for */
      constraints.resize(constraints.size() - (_countof(L", ")-1));
      constraints += L_END_OF_LINE;
    }  /* if */
    return constraints;
  }  /* constraint_clause */

private:
  mdGenericParam            token_;
  wstring                   name_;
  DWORD                     flags_;
  a_generic_constraint_list constraints_;
};  /* a_generic_parameter */


class a_generic_definition abstract
  : public a_signature_decoder_scope
{
protected:
  a_generic_definition(a_const_import_scope &import_scope,
                       mdToken              token)
    : a_signature_decoder_scope(import_scope)
    , token_(token)
    , parameters_initialized_(false)
    , constraints_initialized_(false)
    , has_type_constraints_(false)
  {
  }  /* Constructor. */

public:
  mdTypeDef token() const
  {
    return token_;
  }  /* token */

  virtual bool is_system_string_scope() const
  {
    return false;
  }  /* is_system_string_scope */

  virtual bool is_generic_instance_scope() const
  {
    return false;
  }  /* is_generic_instance_scope */

  const a_generic_parameter_list &generic_parameters() const
  {
    init_generic_parameters();
    return generic_parameters_;
  }  /* generic_parameters */

  const a_generic_argument_list &generic_arguments() const
  {
    init_generic_parameters();
    return generic_arguments_;
  }  /* generic_arguments */

  BYTE generic_parameter_count() const
  {
    return static_cast<BYTE>(generic_parameters().size());
  }  /* generic_parameter_count */

  a_boolean has_type_constraints() const
  {
    init_parameter_constraints();
    return has_type_constraints_;
  }  /* has_type_constraints */

  virtual wstring generic_header(
                           a_boolean out_of_class_definition,
                           a_boolean omit_constraints,
                           a_boolean use_pending_constraint_clause) const = 0;

protected:
  virtual BYTE generic_arity() const = 0;
  virtual void fixup_parameter_names() const = 0;

  static wstring form_generic_parameter_list(
                        a_generic_parameter_iterator generic_parameters_begin,
                        a_generic_parameter_iterator generic_parameters_end)
  {
    wstring parameter_list;

    parameter_list = L"generic<";
    for (auto iter = generic_parameters_begin;
         iter != generic_parameters_end;
         ++iter) {
      auto &parameter = *iter;
      parameter_list += L"typename " + parameter.name();
      if (iter + 1 != generic_parameters_end) {
        parameter_list += L", ";
      }  /* if */
    }  /* for */
    parameter_list += L'>';
    parameter_list += L_END_OF_LINE;
    return parameter_list;
  }  /* form_generic_parameter_list */

  wstring generic_constraint_clause_list(
                                a_boolean use_pending_constraint_clause) const
  /*
  Return the constraint clauses for a generic type or method.
  */
  {
    wstring constraint_clause_list;

    init_parameter_constraints();
    if (use_pending_constraint_clause && has_type_constraints_) {
      constraint_clause_list += L"...";
      constraint_clause_list += L_END_OF_LINE;
    } else {
      for (auto iter = generic_parameters_.end() - generic_arity();
           iter != generic_parameters_.end();
           ++iter) {
        auto &parameter = *iter;
        constraint_clause_list += parameter.constraint_clause();
      }  /* for */
    }  /* if */
    return constraint_clause_list;
  }  /* generic_constraint_clause_list */

  void init_parameter_constraints() const
  {
    if (!constraints_initialized_) {
      an_import_interface_ptr import_interface;
      import_interface = import_scope_.import_interface();
      for (auto param_iter = generic_parameters_.end() - generic_arity();
           param_iter != generic_parameters_.end();
           ++param_iter) {
        auto                                   &parameter = *param_iter;
        HRESULT                                hr;
        HCORENUM                               enum_constraints = nullptr;
        ULONG                                  count_of_constraints;
        unique_ptr<mdGenericParamConstraint[]> constraint_tokens;
        a_generic_constraint_list              constraints;
        hr = import_interface->EnumGenericParamConstraints(
                                         &enum_constraints,
                                         parameter.token(),
                                         /*rGenericParamConstraints=*/nullptr,
                                         /*cMax*/0,
                                         &count_of_constraints);
        CHECK_API_RESULT(hr, EnumGenericParamConstraints);
        hr = import_interface->CountEnum(enum_constraints,
                                         &count_of_constraints);
        CHECK_API_RESULT(hr, CountEnum);
        constraint_tokens.reset(new mdGenericParamConstraint[
                                                       count_of_constraints]);
        hr = import_interface->EnumGenericParamConstraints(
                                                      &enum_constraints,
                                                      parameter.token(),
                                                      constraint_tokens.get(),
                                                      count_of_constraints,
                                                      &count_of_constraints);
        CHECK_API_RESULT(hr, EnumGenericParamConstraints);
        import_interface->CloseEnum(enum_constraints);
        constraints.reserve(count_of_constraints);
        for (ULONG constraint_index = 0;
             constraint_index < count_of_constraints;
             ++constraint_index) {
          mdGenericParamConstraint constraint_token;
          mdToken                  constraint_param;
          mdToken                  constraint_item;
          constraint_token = constraint_tokens[constraint_index];
          hr = import_interface->GetGenericParamConstraintProps(
                                                             constraint_token,
                                                             &constraint_param,
                                                             &constraint_item);
          CHECK_API_RESULT(hr, GetGenericParamConstraintProps);
          constraints.emplace_back(type_from_token(constraint_item));
          has_type_constraints_ = true;
        }  /* for */
        parameter.set_constraints(constraints);
      }  /* for */
      constraints_initialized_ = true;
    }  /* if */
  }  /* init_parameter_constraints */

private:
  void init_generic_parameters() const
  {
    if (!parameters_initialized_) {
      HRESULT                      hr;
      HCORENUM                     enum_parameters = nullptr;
      ULONG                        count_of_parameters;
      unique_ptr<mdGenericParam[]> parameter_tokens;
      an_import_interface_ptr      import_interface;

      import_interface = import_scope_.import_interface();
      hr = import_interface->EnumGenericParams(&enum_parameters, token_,
                                               /*rGenericParams=*/nullptr,
                                               /*cMax*/0,
                                               &count_of_parameters);
      CHECK_API_RESULT(hr, EnumGenericParams);
      hr = import_interface->CountEnum(enum_parameters, &count_of_parameters);
      CHECK_API_RESULT(hr, CountEnum);
      parameter_tokens.reset(new mdGenericParam[count_of_parameters]);
      hr = import_interface->EnumGenericParams(&enum_parameters, token_,
                                               parameter_tokens.get(),
                                               count_of_parameters,
                                               &count_of_parameters);
      CHECK_API_RESULT(hr, EnumGenericParams);
      import_interface->CloseEnum(enum_parameters);
      generic_parameters_.reserve(count_of_parameters);
      for (ULONG parameter_index = 0;
           parameter_index < count_of_parameters;
           ++parameter_index) {
        mdGenericParam param_token = parameter_tokens[parameter_index];
        ULONG          param_index;
        DWORD          param_flags;
        wstring        param_name;
        hr = import_interface->GetGenericParamProps(param_token,
                                                    &param_index,
                                                    &param_flags,
                                                    /*param_owner=*/nullptr,
                                                    /*reserved=*/nullptr,
                                                    param_name);
        CHECK_API_RESULT(hr, GetGenericParamProps);
        check_assertion(generic_parameters_.size() == param_index);
        generic_parameters_.emplace_back(param_name, param_token,
                                         param_flags);
      }  /* for */
      parameters_initialized_ = true;
      fixup_parameter_names();
      init_generic_arguments();
    }  /* if */
  }  /* init_parameters */

  void init_generic_arguments() const
  {
    generic_arguments_.reserve(generic_parameters_.size());
    for (auto &parameter : generic_parameters_) {
      a_qualified_name  parameter_name;
      parameter_name.append_identifier(parameter.name());
      generic_arguments_.emplace_back(make_shared<a_class_type_wrapper>(
                                   a_class_type_wrapper::ck_generic_parameter,
                                   parameter_name));
      /* Also update the parameter name to account for invalid parameter
         names escaped by escape_invalid_identifier. */
      parameter.set_name(parameter_name.as_string());
    }  /* for */
  }  /* init_generic_arguments */

protected:
  mdToken                          token_;
  mutable a_generic_parameter_list generic_parameters_;
  mutable a_generic_argument_list  generic_arguments_;

private:
  mutable bool                     parameters_initialized_;
  mutable bool                     constraints_initialized_;
  mutable bool                     has_type_constraints_;
};  /* a_generic_definition */


class an_accessibility
{
public:
  an_accessibility()
    : access_(access_unknown)
    , enclosing_type_(nullptr)
  {
  }  /* Constructor. */

  an_accessibility(a_const_import_scope    &import_scope,
                   mdToken                 token,
                   DWORD                   attributes,
                   const a_type_definition *enclosing_type);

  wstring get_string() const
  {
    wstring result;
    switch (access_) {
      case access_unknown:
      case access_none:
        /* We should not be emitting this accessibility. */
        unexpected_condition();
        break;
      case access_private:
      case access_imported_private:
        /* Accessible only by the parent type */
        result = L"private";
        break;
      case access_private_as_friend:
        result = L"public";
        break;
      case access_family_and_assembly:
      case access_imported_family_and_assembly:
        /* Accessible by subtypes only in the assembly. */
        result = L"private protected";
        break;
      case access_family_and_assembly_as_friend:
        result = L"protected";
        break;
      case access_assembly:
      case access_imported_assembly:
        /* Accessibly by anyone in the assembly. */
        result = L"internal";
        break;
      case access_assembly_as_friend:
        result = L"public";
        break;
      case access_family:
        /* Accessible only by type and subtypes. */
        result = L"protected";
        break;
      case access_family_or_assembly:
        /* Accessible by derived classes and by other types in the
            assembly. */
        result = L"protected public";
        break;
      case access_family_or_assembly_as_friend:
        result = L"public";
        break;
      case access_public:
        /* Accessible by all types with access to the scope. */
        result = L"public";
        break;
      default:
        unexpected_condition();
        break;
    }  /* switch */
    return result;
  }  /* get_string */

  bool is_unknown() const
  {
    return access_ == access_unknown;
  }  /* is_unknown */

  bool is_accessible() const;
  bool is_publically_accessible() const;

  bool operator==(const an_accessibility &access) const
  {
    return access_ == access.access_;
  }  /* operator== */

  static an_accessibility wider_accessibility(
                                            const an_accessibility &access1,
                                            const an_accessibility &access2)
  /*
    "access1" has wider access than "access2" if "access1" permits more
    access than "access2" both within the assembly and outside the assembly.
  */
  {
    return access1.access_ > access2.access_ ? access1 : access2;
  }

  static an_accessibility narrower_accessibility(
                                            const an_accessibility &access1,
                                            const an_accessibility &access2)
  /*
    "access1" has narrower access than "access2" if "access1" permits less
    access than "access2" both within the assembly and outside the assembly.
  */
  {
    return access1.access_ <= access2.access_ ? access1 : access2;
  }

private:
  const a_type_definition *enclosing_type_;

  enum access_kind {
    access_unknown,                    /* A special value that represents an
                                          uninitialized access_kind. */
                                       /* within assembly  outside assembly */
    access_none,                       /* none             none             */
    access_private,                    /* private          private          */
    access_imported_private,
    /* A special case of access_private used to indicate that the type or
       member will be imported even though it is inaccessible. */
    access_private_as_friend,
    access_family_and_assembly,        /* protected        private          */
    access_imported_family_and_assembly,
    /* A special case of access_family_and_assembly used to indicate that the
       type or member will be imported even though it is inaccessible. */
    access_family_and_assembly_as_friend,
    access_assembly,                   /* public           private          */
    access_imported_assembly,
    /* A special case of access_assembly used to indicate that the type or
       member will be imported even though it is inaccessible. */
    access_assembly_as_friend,
    access_family,                     /* protected        protected        */
    access_family_or_assembly,         /* public           protected        */
    access_family_or_assembly_as_friend,
    access_public                      /* public           public           */
  } access_;

  an_accessibility(access_kind             access,
                   const a_type_definition *enclosing_type_ = nullptr)
    : access_(access)
    , enclosing_type_(nullptr)
  {
  }  /* Constructor. */
};  /* an_accessibility */


class a_type_definition
  : public a_generic_definition
{
public:
  enum a_type_definition_kind
  {
    tdk_unknown,
    tdk_ref_class,
    tdk_interface_class,
    tdk_value_class,
    tdk_enum_class,
    tdk_delegate,
    tdk_native_class,
    tdk_native_enum
  };

  static wstring string_from_kind(a_type_definition_kind kind)
  {
    wstring result;

    switch (kind) {
      case tdk_ref_class:
        result = L"ref class";
        break;
      case tdk_interface_class:
        result = L"interface class";
        break;
      case tdk_value_class:
        result = L"value class";
        break;
      case tdk_enum_class:
        result = L"enum class";
        break;
      case tdk_delegate:
        result = L"delegate";
        break;
      case tdk_native_class:
        result = L"class";
        break;
      case tdk_native_enum:
        result = L"enum";
        break;
      case tdk_unknown:
      default:
        unexpected_condition();
        break;
    }  /* switch */
    return result;
  }  /* string_from_kind */

  a_type_definition(a_const_import_scope &import_scope)
    : a_generic_definition(import_scope, mdTypeDefNil)
    , attributes_(0)
    , base_class_token_(mdTokenNil)
    , kind_(tdk_unknown)
    , is_system_string_scope_(false)
    , enclosing_type_(nullptr)
    , named_overrides_initialized_(false)
    , enumerators_initialized_(false)
  {
  }  /* Constructor. */

  a_type_definition(a_const_import_scope &import_scope,
                    mdTypeDef            token)
    : a_generic_definition(import_scope, token)
    , kind_(tdk_unknown)
    , is_system_string_scope_(false)
    , enclosing_type_(nullptr)
    , named_overrides_initialized_(false)
    , enumerators_initialized_(false)
  {
    HRESULT hr = import_interface()->GetTypeDefProps(token,
                                                     &attributes_,
                                                     &base_class_token_);
    CHECK_API_RESULT(hr, GetTypeDefProps);
    if (IsTdNested(attributes_)) {
      mdTypeDef enclosing_type_token;
      hr = import_interface()->GetNestedClassProps(token,
                                                   &enclosing_type_token);
      CHECK_API_RESULT(hr, GetNestedClassProps);
      enclosing_type_ = &import_scope.get_type_definition(
                                                        enclosing_type_token);
    }  /* if */
    accessibility_ = an_accessibility(import_scope, token, attributes_,
                                      enclosing_type_);
    BYTE                           generic_parameter_count;
    a_const_class_type_wrapper_ptr unresolved_generic_argument;
    a_qualified_name full_type_name = import_scope.name_from_typedef(
                                                 token,
                                                 *this,
                                                 generic_parameter_count,
                                                 unresolved_generic_argument);
    class_type_ = make_shared<a_class_type_wrapper>(
                                               a_class_type_wrapper::ck_class,
                                               full_type_name,
                                               &import_scope_,
                                               token);
    full_type_name.strip_generic_arguments();
    type_name_ = full_type_name.unqualified_name();
    if (!is_cppcx_metadata) {
      is_system_string_scope_ = class_type_->name() ==
                                                    MAKE_CLASS_STRING(String);
    }  /* if */
  }  /* Constructor. */

  bool is_nested() const
  {
    return enclosing_type_ != nullptr;
  }  /* base_class_token */

  DWORD attributes() const
  {
    return attributes_;
  }  /* attributes */

  mdToken base_class_token() const
  {
    return base_class_token_;
  }  /* base_class_token */

  const an_accessibility &accessibility() const
  {
    return accessibility_;
  }  /* accessibility */

  a_const_class_type_wrapper_ptr class_type() const
  {
    return class_type_;
  }  /* class_type */

  a_type_definition_kind kind() const
  {
    if (kind_ == tdk_unknown) {
      if (IsTdInterface(attributes_)) {
        kind_ = tdk_interface_class;
      } else if (class_type_->name() == MAKE_CLASS_STRING(Enum) ||
                 class_type_->name() == MAKE_CLASS_STRING(MulticastDelegate)) {
        kind_ = tdk_ref_class;
      } else {
        bool    is_native = false;
        HRESULT hr = S_FALSE;
        if (!is_cppcx_metadata) {
          hr = import_interface()->GetCustomAttributeByName(
                   token_,
                   L"System.Runtime.CompilerServices.NativeCppClassAttribute",
                   /*ppData=*/nullptr,
                   /*pcbData=*/nullptr);
          CHECK_API_RESULT(hr, GetCustomAttributeByName);
          is_native = (hr == S_OK);
        }  /* if */
        auto base_class = base_class_type();
        if (base_class != nullptr) {
          if (base_class->name() == MAKE_CLASS_STRING(ValueType)) {
            kind_ = tdk_value_class;
          } else if (base_class->name() == MAKE_CLASS_STRING(Enum)) {
            HCORENUM    enum_fields = NULL;
            mdFieldDef  fields[2];
            ULONG       count_of_fields;
            /* Mark the type definition as an enum class. */
            if (is_native) {
              kind_ = tdk_native_enum;
            } else {
              kind_ = tdk_enum_class;
            }  /* if */
            /* Change the base class type to the underlying type of the
               enum. */
            hr = import_interface()->EnumFieldsWithName(
                                                     &enum_fields, token_,
                                                     COR_ENUM_FIELD_NAME_W,
                                                     fields, _countof(fields),
                                                     &count_of_fields);
            CHECK_API_RESULT(hr, EnumFieldsWithName);
            import_interface()->CloseEnum(enum_fields);
            check_assertion(count_of_fields == 1);
            PCCOR_SIGNATURE  signature;
            ULONG            bytes_in_signature;
            DWORD            field_attributes;
            hr = import_interface()->GetFieldProps(
                                          fields[0], /*pClass=*/nullptr,
                                          /*szField=*/nullptr, /*cchField=*/0,
                                          /*pchField=*/nullptr,
                                          &field_attributes,
                                          &signature, &bytes_in_signature,
                                          /*constant_type=*/nullptr,
                                          /*constant_value=*/nullptr,
                                          /*characters_in_constant=*/nullptr);
            CHECK_API_RESULT(hr, GetFieldProps);
            check_assertion(!IsFdStatic(field_attributes) &&
                            IsFdSpecialName(field_attributes) &&
                            IsFdRTSpecialName(field_attributes));
            base_class_type_ = a_signature_decoder::decode_field(
                                                          *this,
                                                          signature,
                                                          bytes_in_signature);
            check_assertion(base_class_type_ != nullptr);
            switch (base_class_type_->kind()) {
              case a_type_wrapper::twk_bool:
              case a_type_wrapper::twk_wchar_t:
              case a_type_wrapper::twk_char:
              case a_type_wrapper::twk_signed_char:
              case a_type_wrapper::twk_unsigned_char:
              case a_type_wrapper::twk_short:
              case a_type_wrapper::twk_unsigned_short:
              case a_type_wrapper::twk_int:
              case a_type_wrapper::twk_unsigned_int:
              case a_type_wrapper::twk_long:
              case a_type_wrapper::twk_unsigned_long:
              case a_type_wrapper::twk_long_long:
              case a_type_wrapper::twk_unsigned_long_long:
                break;
              default:
                unexpected_condition();
                break;
            }  /* switch */
          } else if (base_class->name() == MAKE_CLASS_STRING(Delegate) ||
                     base_class->name() ==
                                       MAKE_CLASS_STRING(MulticastDelegate)) {
            kind_ = tdk_delegate;
          } else {
            /* Not one of the above so this must be a ref-class. */
            kind_ = tdk_ref_class;
          }  /* if */
        } else {
          /* Not an interface and it doesn't extend anything so this must be a
             native class or System.Object, which is a ref-class. */
          kind_ = is_native ? tdk_native_class : tdk_ref_class;
        }  /* if */
      }  /* if */
    }  /* if */
    return kind_;
  }  /* kind */

  a_const_type_wrapper_ptr underlying_type() const
  {
    a_const_type_wrapper_ptr underlying_type;
    a_type_definition_kind   type_kind = kind();

    if (type_kind == tdk_native_enum || type_kind == tdk_enum_class) {
      underlying_type = base_class_type_;
    } else {
      unexpected_condition();
    }  /* if */
    return underlying_type;
  }  /* underlying_type */

  a_const_class_type_wrapper_ptr base_class_type() const
  {
    if (!IsNilToken(base_class_token_) && base_class_type_ == nullptr) {
      base_class_type_ = type_from_token(base_class_token_);
    }  /* if */
    return (base_class_type_ != nullptr) ? base_class_type_->as_class()
                                         : nullptr;
  }  /* base_class_type */

  const wstring &type_name() const
  {
    return type_name_;
  }  /* type_name */

  const a_qualified_name &qualified_name() const
  {
    return class_type_->name();
  }  /* qualified_name */

  void import_definition(ostringstream &buffer) const;
  void import_enum_definition(ostringstream &buffer) const;
  void import_delegate_definition(ostringstream &buffer) const;

  virtual wstring generic_header(
                                a_boolean out_of_class_definition,
                                a_boolean omit_constraints,
                                a_boolean use_pending_constraint_clause) const
  {
    wstring header;

    header += generic_parameter_list(out_of_class_definition);
    if (!omit_constraints) {
      header += generic_constraint_clause_list(use_pending_constraint_clause);
    }  /* if */
    return header;
  }  /* generic_header */

  virtual bool is_system_string_scope() const
  {
    return is_system_string_scope_;
  }  /* is_system_string_scope */

  virtual BYTE generic_arity() const
  {
    BYTE arity = (BYTE)generic_parameters().size();
    if (enclosing_type_ != nullptr) {
      arity -= enclosing_type_->generic_parameter_count();
    }  /* if */
    return arity;
  }  /* arity */

  virtual const a_generic_argument_list &generic_type_arguments() const
  {
    return generic_arguments();
  }  /* generic_type_arguments */

  virtual const a_generic_argument_list &generic_method_arguments() const
  {
    unexpected_condition();
    return no_generic_arguments;
  }  /* generic_method_arguments */

  bool is_named_override(mdToken method_token) const
  /*
  Returns TRUE if the method is a named override for an accessible method from
  a base class or interface.
  */
  {
    const auto &named_overrides = get_named_overrides();
    auto named_override_iter = named_overrides.find(method_token);
    return named_override_iter != named_overrides.end();
  }  /* is_named_override */

  bool is_custom_attribute() const
  /*
  Returns TRUE if the class is derived from System::Attribute.
  */
  {
    bool result = false;
    auto base_class = base_class_type();
    if (base_class != nullptr) {
      result = base_class->name() == MAKE_CLASS_STRING(Attribute);
      if (!result) {
         auto base_class_definition = base_class->type_definition();
         result = base_class_definition != nullptr &&
                  base_class_definition->is_custom_attribute();
      }  /* if */
    }  /* if */
    return result;
  }  /* is_custom_attribute */

  void write_custom_attributes(
                           ostringstream &buffer,
                           mdToken       token,
                           const wstring &attribute_target = wstring()) const;
private:
  string process_base_class_list(ostringstream& buffer) const;
  bool process_base_class(ostringstream& buffer) const;
  void process_interfaces(ostringstream& buffer) const;
  void import_nested_classes(ostringstream& buffer) const;
  void import_all_methods(ostringstream &buffer) const;
  void write_method_decl_specifiers(ostringstream &buffer,
                                    mdToken       token,
                                    DWORD         method_attributes) const;
  void import_one_method(ostringstream             &buffer,
                         const a_method_definition &method_definition) const;
  void import_all_fields(ostringstream &buffer) const;
  void import_one_field(ostringstream &buffer,
                        mdFieldDef    field_token) const;
  void import_properties(ostringstream& buffer) const;
  void import_one_property(ostringstream& buffer,
                           mdProperty property_token) const;
  void import_events(ostringstream& buffer) const;
  void import_one_event(ostringstream& buffer, mdEvent event_token) const;

  static wstring form_full_generic_parameter_list(
                        const a_type_definition      &type_definition,
                        a_generic_parameter_iterator generic_parameters_begin,
                        a_generic_parameter_iterator generic_parameters_end)
  {
    wstring                 parameter_list;
    BYTE                    generic_arity = type_definition.generic_arity();
    const a_type_definition *enclosing_type_definition = type_definition.
                                                              enclosing_type_;

    if (enclosing_type_definition != nullptr) {
      parameter_list += form_full_generic_parameter_list(
                                      *enclosing_type_definition,
                                      generic_parameters_begin,
                                      generic_parameters_end - generic_arity);
    }  /* if */
    if (generic_arity > 0) {
      parameter_list += form_generic_parameter_list(
                                       generic_parameters_end - generic_arity,
                                       generic_parameters_end);
    }  /* if */
    return parameter_list;
  }  /* form_generic_parameter_list */

  wstring generic_parameter_list(a_boolean out_of_class_definition) const
  {
    wstring parameter_list;
    if (out_of_class_definition) {
      parameter_list = form_full_generic_parameter_list(
                                                  *this,
                                                  generic_parameters_.begin(),
                                                  generic_parameters_.end());
    } else if (generic_arity() > 0) {
      parameter_list = form_generic_parameter_list(
                                  generic_parameters_.end() - generic_arity(),
                                  generic_parameters_.end());
    }  /* if */
    return parameter_list;
  }  /* generic_parameter_list */

  virtual void fixup_parameter_names() const
  {
    /* When a generic class is nested within a generic class, parameter names
       associated with the enclosing generic class can be the same as
       parameter names associated with the nested generic class. For example:
         generic <typename T>
         public ref struct GenericClass {
           typedef T OUTER_T;
           generic <typename T>
           ref struct NestedGenericClass {
             T t;
             OUTER_T ot;
           };
         };
       However, the generated definition would not be valid if duplicate names
       exist.  For example, the following would be generated as the definition
       of NestedGenericClass:
         generic <typename T>
         generic <typename T>
         ref struct GenericClass<T>::NestedGenericClass {
           T t;
           T ot;
         };
       To handle this case, rename any duplicate generic parameter names
       associated with any enclosing generic classes. */
    for (auto param_iter = generic_parameters_.begin();
         param_iter != generic_parameters_.end() - generic_arity();
         ++param_iter) {
      auto &parameter = *param_iter;
      auto duplicate_count = count_if(
                                  param_iter + 1,
                                  generic_parameters_.end(),
                                  [&](a_generic_parameter &other) {
                                    return parameter.name() == other.name();
                                  });
      if (duplicate_count > 0) {
        wstring param_name = parameter.name();
        param_name += L"__hidden";
        param_name += to_wstring((LONGLONG)duplicate_count);
        parameter.set_name(move(param_name));
      }  /* if */
    }  /* for */
  }  /* fixup_parameter_names */

private:
  DWORD         attributes_;
                        /* The attributes for this type. */
  mdToken       base_class_token_;
                        /* The token for the base class of this type. */
  const a_type_definition
                *enclosing_type_;
                        /* The enclosing type if this is a nested type;
                           nullptr otherwise. */
  an_accessibility
                accessibility_;
                        /* The accessibility of this type. */
  a_class_type_wrapper_ptr
                class_type_;
                        /* The class type for the type. */
  mutable a_const_type_wrapper_ptr
                base_class_type_;
                        /* The type of the base class or underlying type
                           of an enum. */
  wstring       type_name_;
                        /* The name of the type. */
  mutable a_type_definition_kind
                kind_;  /* The type's kind. */
  bool          is_system_string_scope_;
                        /* Indicates that the type is System::String. */

  typedef map<mdToken, wstring> a_named_override_map;
  typedef const a_named_override_map &a_const_named_override_map_ref;
  a_const_named_override_map_ref get_named_overrides() const;
  mutable bool  named_overrides_initialized_;
                        /* Indicates that named_overrides_ has been
                           initialized. */
  mutable a_named_override_map
                named_overrides_;
                        /* If this type definition is a class, this maps a
                           virtual function in this class to the list of
                           accessible base class virtual functions that it
                           explicitly overrides. */

public:
  typedef pair<wstring, a_constant_value> an_enumerator;
  typedef vector<an_enumerator> an_enumerator_list;
  typedef const an_enumerator_list &a_const_enumerator_list_ref;
  a_const_enumerator_list_ref get_enumerators() const;
  const a_constant_value *get_enumerator(const wstring &name) const
  {
    auto &enumerators = get_enumerators();
    auto iter = find_if(enumerators.cbegin(), enumerators.cend(),
                        [&](const an_enumerator &enumerator) {
                          return enumerator.first == name;
                        });
    return (iter != enumerators.cend()) ? &iter->second : nullptr;
  }  /* get_enumerator */
private:
  mutable bool  enumerators_initialized_;
                        /* Indicates that enumerators_ has been
                           initialized. */
  mutable an_enumerator_list
                enumerators_;
                        /* If this type definition is an enum type, this
                           contains the list of enumerators. */
};  /* a_type_definition */


an_import_scope::an_import_scope(an_import_scope&& other)
  : scope_index_(other.scope_index_),
    scope_name_(move(other.scope_name_)),
    containing_assembly_(other.containing_assembly_),
    import_interface_(move(other.import_interface_)),
    active_namespace_(move(other.active_namespace_)),
    map_typedef_to_definition_(move(other.map_typedef_to_definition_)),
    map_typeref_to_class_type_(move(other.map_typeref_to_class_type_)),
    map_token_to_custom_attributes_(
                                  move(other.map_token_to_custom_attributes_))
{
}  /* an_import_scope move constructor */


const a_type_definition &an_import_scope::get_type_definition(
                                                        mdTypeDef token) const
{
  auto iter = map_typedef_to_definition_.lower_bound(token);
  if (iter == map_typedef_to_definition_.end() ||
      map_typedef_to_definition_.key_comp()(token, iter->first)) {
    /* This is the first request for the type definition of this token. */
    a_type_definition type_definition(*this, token);
    iter = map_typedef_to_definition_.emplace_hint(iter, token,
                                                   move(type_definition));
  }  /* if */
  return iter->second;
}  /* an_import_scope::get_type_definition */


an_accessibility::an_accessibility(a_const_import_scope    &import_scope,
                                   mdToken                 token,
                                   DWORD                   attributes,
                                   const a_type_definition *enclosing_type)
  : enclosing_type_(enclosing_type)
{
  auto& containing_assembly = import_scope.containing_assembly();
  auto              import_flags = containing_assembly.import_flags();
  bool              import_inaccessible = false;
  bool  import_as_friend = (import_flags & cpp_cli_as_friend_assembly) != 0;

  if (is_cppcx_metadata) {
    /* Import inaccessible types and members from platform.winmd. */
    import_inaccessible = containing_assembly.is_platform_winmd();
  }  /* if */
  check_assertion(!IsNilToken(token));
  switch (TypeFromToken(token)) {
    case mdtTypeDef:
      switch (attributes & tdVisibilityMask) {
        case tdNotPublic:
          access_ = import_as_friend ? access_private_as_friend
                                      : access_private;
          break;
        case tdPublic:
          access_ = access_public;
          break;
        case tdNestedPublic:
          access_ = access_public;
          break;
        case tdNestedPrivate:
          access_ = access_private;
          break;
        case tdNestedFamily:
          access_ = access_family;
          break;
        case tdNestedAssembly:
          access_ = import_as_friend ? access_assembly_as_friend
                                      : access_assembly;
          break;
        case tdNestedFamANDAssem:
          access_ = import_as_friend ? access_family_and_assembly_as_friend
                                      : access_family_and_assembly;
          break;
        case tdNestedFamORAssem:
          access_ = import_as_friend ? access_family_or_assembly_as_friend
                                      : access_family_or_assembly;
          break;
        default:
          unexpected_condition();
          break;
      }  /* switch */
      break;
    case mdtFieldDef:
      switch (attributes & fdFieldAccessMask) {
        case fdPrivateScope:
          access_ = access_none;
          break;
        case fdPrivate:
          access_ = access_private;
          break;
        case fdFamANDAssem:
          access_ = import_as_friend ? access_family_and_assembly_as_friend
                                      : access_family_and_assembly;
          break;
        case fdAssembly:
          access_ = import_as_friend ? access_assembly_as_friend
                                      : access_assembly;
          break;
        case fdFamily:
          access_ = access_family;
          break;
        case fdFamORAssem:
          access_ = import_as_friend ? access_family_or_assembly_as_friend
                                      : access_family_or_assembly;
          break;
        case fdPublic:
          access_ = access_public;
          break;
        default:
          unexpected_condition();
          break;
      }  /* switch */
      break;
    case mdtMethodDef:
      switch (attributes & mdMemberAccessMask) {
        case mdPrivateScope:
          access_ = access_none;
          break;
        case mdPrivate:
          access_ = access_private;
          break;
        case mdFamANDAssem:
          access_ = import_as_friend ? access_family_and_assembly_as_friend
                                      : access_family_and_assembly;
          break;
        case mdAssem:
          access_ = import_as_friend ? access_assembly_as_friend
                                      : access_assembly;
          break;
        case mdFamily:
          access_ = access_family;
          break;
        case mdFamORAssem:
          access_ = import_as_friend ? access_family_or_assembly_as_friend
                                      : access_family_or_assembly;
          break;
        case mdPublic:
          access_ = access_public;
          break;
        default:
          unexpected_condition();
          break;
      }  /* switch */
      if (!import_inaccessible && enclosing_type != nullptr) {
        switch (access_) {
          case access_private:
          case access_family_and_assembly:
          case access_assembly:
            /* Inaccessible methods are not usually imported; however, a
               method that is a named override of a method from a base
               interface must still be imported to satisfy the interface's
               contract. */
            import_inaccessible = enclosing_type->is_named_override(token);
            break;
          default:
            break;
        }  /* switch */
      }  /* if */
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  if (import_inaccessible) {
    switch (access_) {
      case access_private:
        access_ = access_imported_private;
        break;
      case access_family_and_assembly:
        access_ = access_imported_family_and_assembly;
        break;
      case access_assembly:
        access_ = access_imported_assembly;
        break;
      default:
        break;
    }  /* switch */
  }  /* if */
}  /* an_accessibility constructor. */


bool an_accessibility::is_accessible() const
{
  return access_ != access_none &&
         access_ != access_private &&
         access_ != access_family_and_assembly &&
         access_ != access_assembly &&
         (enclosing_type_ == nullptr ||
          enclosing_type_->accessibility().is_accessible());
}  /* an_accessibility::is_accessible */


bool an_accessibility::is_publically_accessible() const
{
  return access_ == access_public &&
         (enclosing_type_ == nullptr ||
          enclosing_type_->accessibility().is_publically_accessible());
}  /* an_accessibility::is_publically_accessible */


wstring name_from_method_semantics(DWORD method_semantics)
{
  wstring name;
  switch (method_semantics) {
    case msSetter:
      name = L"set";
      break;
    case msGetter:
      name = L"get";
      break;
    case msAddOn:
      name = L"add";
      break;
    case msRemoveOn:
      name = L"remove";
      break;
    case msFire:
      name = L"raise";
      break;
    case msOther:
    default:
      unexpected_condition();
      break;
  }  /* switch */
  return name;
}  /* name_from_method_semantics */


bool fixup_destructor_or_finalizer_name(const wstring &type_name,
                                        wstring       &method_name)
/*
Returns TRUE if 'method_name' is a destructor or finalizer for the type named
type_name and adjusts 'method_name' accordingly if the type name is an invalid
C++ identifier.
*/
{
  bool is_destructor_or_finalizer = false;
  wchar_t first_char = method_name[0];
  if (first_char == L'~' || first_char == L'!') {
    wstring::size_type chars_to_skip = 1;
    if (type_name.compare(0, _countof(L"__identifier(") - 1,
                          L"__identifier(")) {
      wstring temp_method_name = method_name;
      escape_invalid_identifier(temp_method_name, chars_to_skip);
      if (temp_method_name.compare(chars_to_skip, wstring::npos,
                                   type_name) == 0) {
        is_destructor_or_finalizer = true;
        /* Modify the name to be of the form "~__identifier(...)" or
           "!__identifier(...)". */
        method_name = temp_method_name;
      }  /* if */
    } else {
      if (method_name.compare(chars_to_skip, wstring::npos,
                              type_name) == 0) {
        is_destructor_or_finalizer = true;
      }  /* if */
    }  /* if */
  }  /* if */
  return is_destructor_or_finalizer;
}  /* fixup_destructor_or_finalizer_name */


class a_method_definition
  : public a_generic_definition
{
public:
  a_method_definition(a_const_import_scope &import_scope,
                      mdMethodDef          token)
    : a_generic_definition(import_scope, token)
    , enclosing_type_(nullptr)
    , event_or_property_token_(mdTokenNil)
    , cli_operator_kind_(cok_none)
  {
    HRESULT    hr;
    mdTypeDef  parent_token;

    hr = import_interface()->GetMethodProps(token_,
                                            &parent_token,
                                            &attributes_,
                                            /*ppvSigBlob=*/nullptr,
                                            /*pcbSigBlob=*/nullptr,
                                            /*pulCodeRVA=*/nullptr,
                                            /*pdwImplFlags=*/nullptr);
    CHECK_API_RESULT(hr, GetMethodProps);
    if (!IsNilToken(parent_token)) {
      enclosing_type_ = &import_scope_.get_type_definition(parent_token);
    }  /* if */
  }  /* Constructor. */

  DWORD attributes() const
  {
    return attributes_;
  }  /* attributes */

  const wstring &name() const
  {
    /* Ensure the function type has been determined. */
    (void)function_type();
    return name_;
  }  /* attributes */

  const an_accessibility &accessibility() const
  {
    if (accessibility_.is_unknown()) {
      accessibility_ = an_accessibility(import_scope_, token_, attributes_,
                                        enclosing_type_);
      check_assertion(!accessibility_.is_unknown());
    }  /* if */
    return accessibility_;
  }  /* accessibility */

  a_const_function_type_wrapper_ptr function_type() const
  {
    if (function_type_ == nullptr) {
      HRESULT             hr;
      PCCOR_SIGNATURE     signature;
      ULONG               bytes_in_signature;

      hr = import_interface()->GetMethodProps(token_,
                                              /*pClass=*/nullptr,
                                              name_,
                                              /*pdwAttr=*/nullptr,
                                              &signature,
                                              &bytes_in_signature,
                                              /*pulCodeRVA=*/nullptr,
                                              /*pdwImplFlags=*/nullptr);
      CHECK_API_RESULT(hr, GetMethodProps);
      function_type_ = a_signature_decoder::decode_method(*this, token_,
                                                          signature,
                                                          bytes_in_signature);
      if (function_type_->is_invalid()) {
        /* The method was not successfully decoded.  It will not be
           imported. */
      } else if (enclosing_type_ == nullptr) {
        escape_invalid_identifier(name_);
      } else {
        auto &type_name = enclosing_type_->type_name();
        if (IsMdInstanceInitializerW(attributes_, name_.c_str()) ||
            IsMdClassConstructorW(attributes_, name_.c_str())) {
          name_ = type_name;
          /* Constructors don't have a return type. */
          function_type_->omit_return_type();
        } else if (fixup_destructor_or_finalizer_name(type_name, name_)) {
          /* Destructors and finalizers don't have a return type. */
          function_type_->omit_return_type();
        } else {
          /* Determine if this is an event or property accessor method. */
          event_or_property_token_ = import_scope_.
                                      get_associated_event_or_property(token_);
          if (!IsNilToken(event_or_property_token_)) {
            /* This method is associated with an event or property. */
            DWORD method_semantics;
            hr = import_interface()->GetMethodSemantics(
                                                     token_,
                                                     event_or_property_token_,
                                                     &method_semantics);
            CHECK_API_RESULT(hr, GetMethodSemantics);
            if (!IsMsOther(method_semantics)) {
              name_ = name_from_method_semantics(method_semantics);
            }  /* if */
          } else {
            /* Rename any CLI operators to their corresponding C++/CLI operator
               name. */
            cli_operator_kind_ = import_scope_.rename_cli_operator(
                                                                 name_,
                                                                 attributes_);
            if (cli_operator_kind_ == cok_none) {
              escape_invalid_identifier(name_);
            } else if (cli_operator_kind_ == cok_implicit ||
                       cli_operator_kind_ == cok_explicit) {
              /* The method is a user-defined conversion operator. */
              auto return_type = function_type_->return_type();
              check_assertion(return_type != nullptr);
              name_ = L"operator " + return_type->get_string();
              /* Conversion operators don't have a return type. */
              function_type_->omit_return_type();
            }  /* if */
          }  /* if  */
        }  /* if  */
      }  /* if */
    }  /* if */
    return function_type_;
  }  /* function_type */

  a_boolean is_event_or_property_accessor() const
  {
    return !IsNilToken(event_or_property_token());
  }  /* event_or_property_token */

  mdToken event_or_property_token() const
  {
    /* Ensure the function type has been determined. */
    (void)function_type();
    return event_or_property_token_;
  }  /* event_or_property_token */

  a_cli_operator_kind cli_operator_kind() const
  {
    /* Ensure the function type has been determined. */
    (void)function_type();
    return cli_operator_kind_;
  }  /* cli_operator_kind */

  virtual bool is_system_string_scope() const
  {
    return enclosing_type_ != nullptr &&
           enclosing_type_->is_system_string_scope();
  }  /* is_system_string_scope */

  virtual wstring generic_header(
                        a_boolean out_of_class_definition = false,
                        a_boolean omit_constraints = false,
                        a_boolean use_pending_constraint_clause = false) const
  {
    wstring header;

    if (out_of_class_definition) {
      header += enclosing_type_->generic_header(
                                     out_of_class_definition,
                                     /*omit_constraints=*/true,
                                     /*use_pending_constraint_clause=*/false);
    }  /* if */
    if (generic_arity() > 0) {
      header += form_generic_parameter_list(generic_parameters_.begin(),
                                            generic_parameters_.end());
      if (!omit_constraints) {
        header += generic_constraint_clause_list(
                                               use_pending_constraint_clause);
      }  /* if */
    }  /* if */
    return header;
  }  /* generic_header */

  virtual BYTE generic_arity() const
  {
    return generic_parameter_count();
  }  /* generic_arity */

  const a_type_definition *enclosing_type() const
  {
    return enclosing_type_;
  }  /* enclosing_type_definition */

  virtual const a_generic_argument_list &generic_type_arguments() const
  {
    check_assertion(enclosing_type_ != nullptr);
    return enclosing_type_ != nullptr
                             ? enclosing_type_->generic_type_arguments()
                             : no_generic_arguments;
  }  /* generic_type_arguments */

  virtual const a_generic_argument_list &generic_method_arguments() const
  {
    return generic_arguments();
  }  /* generic_method_arguments */

private:
  virtual void fixup_parameter_names() const
  {
    /* When a generic method is nested within a generic class, parameter
       names associated with the enclosing generic class can be the same as
       parameter names associated with the nested generic method.  For
       example:
         generic <typename T>
         public ref struct GenericClass {
           typedef T OUTER_T;
           generic <typename T>
           void NestedGenericMethod(T t, OUTER_T ot) {}
         };
       However, the generated definition would not be valid if duplicate
       names exist.  For example, the following would be generated as the
       definition of GenericClass:
         generic <typename T>
         generic <typename T>
         ref struct GenericClass<T>::NestedGenericClass {
           generic <typename T>
           void NestedGenericMethod(T t, T ot) {}
         };
       To handle this case, rename any duplicate generic parameter names
       associated with the generic method. */
    auto enclosing_type_parameters = enclosing_type_->generic_parameters();
    for (auto param_iter = generic_parameters_.begin();
         param_iter != generic_parameters_.end();
         ++param_iter) {
      a_generic_parameter &method_parameter = *param_iter;
      for (auto encl_param_iter = enclosing_type_parameters.begin();
           encl_param_iter != enclosing_type_parameters.end();
           ++encl_param_iter) {
        a_generic_parameter &enclosing_type_parameter = *encl_param_iter;
        if (method_parameter.name() == enclosing_type_parameter.name()) {
          wstring param_name = method_parameter.name();
          param_name += L"__method_param";
          method_parameter.set_name(move(param_name));
          break;
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* fixup_parameter_names */

private:
  DWORD                                attributes_;
  const a_type_definition              *enclosing_type_;
  mutable wstring                      name_;
  mutable an_accessibility             accessibility_;
  mutable a_function_type_wrapper_ptr  function_type_;
  mutable mdToken                      event_or_property_token_;
  mutable a_cli_operator_kind          cli_operator_kind_;
};  /* a_method_definition */


an_import_scope::an_import_scope(a_scope_index           scope_index,
                                 an_import_interface_ptr import_interface,
                                 a_const_assembly_ptr    containing_assembly)
/*
Create a representation of an import scope and get the information about the
scope that we will need later.  Currently this is just the name of the scope.
*/
  : scope_index_(scope_index)
  , import_interface_(import_interface)
  , containing_assembly_(containing_assembly)
{
  HRESULT hr;

  check_assertion(containing_assembly != nullptr);
  hr = import_interface_->GetScopeProps(scope_name_, /*pmvid=*/nullptr);
  CHECK_API_RESULT(hr, GetScopeProps);
}  /* an_import_scope::an_import_scope */


a_const_method_definition_ptr an_import_scope::get_method_definition(
                                                      mdMethodDef token) const
{
  return a_method_definition_ptr(new a_method_definition(*this, token));
}  /* an_import_scope::get_method_definition */


void an_import_scope::import_all_types(ostringstream& buffer)
/*
Import all the types from an import scope.
*/
{
  HCORENUM                       enum_typedefs = nullptr;
  mdTypeDef                      typedefs[64];
  ULONG                          count_of_typedefs;
  auto                           import_flags =
                                         containing_assembly_->import_flags();
  bool                           use_pending_constraint_clauses;
  a_pending_constraint_type_list pending_constraint_types;

  /* Only use the pending constraint clause for generic types if we're not
     defining all types.  Doing so wouldn't have any benefit because the
     constraint clause on nested generic types or methods may refer to
     other types that have not been imported. */
  use_pending_constraint_clauses =
                               (import_flags & cpp_cli_define_all_types) == 0;
  do {
    HRESULT hr = import_interface_->EnumTypeDefs(&enum_typedefs, typedefs,
                                                 _countof(typedefs),
                                                 &count_of_typedefs);

    CHECK_API_RESULT(hr, EnumTypeDefs);
    for (ULONG i = 0; i < count_of_typedefs; ++i) {
      import_one_type(buffer, typedefs[i],
                      /*at_top_level=*/true,
                      /*want_definition=*/false,
                      /*class_body_only=*/false,
                      use_pending_constraint_clauses ?
                                         &pending_constraint_types : nullptr,
                      /*is_delegate=*/nullptr);
    }  /* for */
  } while (count_of_typedefs > 0);
  import_interface_->CloseEnum(enum_typedefs);
  /* Now that all types have been imported, re-declare all generic types that
     were declared with a pending constraint clause, this time with the
     complete constraint clause. */
  for (auto &pending_constraint_type : pending_constraint_types) {
    import_one_type(buffer, pending_constraint_type,
                    /*at_top_level=*/true,
                    /*want_definition=*/false,
                    /*class_body_only=*/false,
                    /*pending_constraint_types=*/nullptr,
                    /*is_delegate=*/nullptr);
  }  /* for */
  close_all_namespace_scopes(buffer);
}  /* an_import_scope::import_all_types */


void an_import_scope::open_namespace(
                                  ostringstream           &buffer,
                                  wstring::const_iterator namespace_begin,
                                  wstring::const_iterator namespace_end) const
/*
Emit the text to open a namespace scope.
*/
{
  buffer << "namespace " << wstring(namespace_begin, namespace_end)
         << " {" << END_OF_LINE;
}  /* an_import_scope::open_namespace */


void an_import_scope::close_namespace(
                                  ostringstream           &buffer,
                                  wstring::const_iterator namespace_begin,
                                  wstring::const_iterator namespace_end) const
/*
Emit the text to close a namespace scope.
*/
{
  buffer << '}';
#if DEBUG
  buffer << "  /* namespace " << wstring(namespace_begin, namespace_end)
         << " */";
#endif /* DEBUG */
  buffer << END_OF_LINE;
}  /* an_import_scope::close_namespace */


void an_import_scope::set_namespace_scope(
                                        ostringstream    &buffer,
                                        a_qualified_name namespace_name) const
/*
Open or close namespace scopes to set the current namespace to namespace_name.
*/
{
  auto old_name_begin = active_namespace_.as_string().begin();
  auto old_name_end = active_namespace_.as_string().end();
  auto old_name_iter = old_name_begin;
  auto old_separators_iter = active_namespace_.separator_offsets().begin();
  auto old_separators_end = active_namespace_.separator_offsets().end();
  auto new_name_begin = namespace_name.as_string().begin();
  auto new_name_end = namespace_name.as_string().end();
  auto new_name_iter = new_name_begin;
  auto new_separators_iter = namespace_name.separator_offsets().begin();
  auto new_separators_end = namespace_name.separator_offsets().end();
  if (old_name_iter != old_name_end && new_name_iter != new_name_end) {
    for (;;) {
      auto old_component_end = (old_separators_iter == old_separators_end)
                                       ? old_name_end
                                       : old_name_begin + *old_separators_iter;
      auto new_component_end = (new_separators_iter == new_separators_end)
                                       ? new_name_end
                                       : new_name_begin + *new_separators_iter;
      if (std::distance(old_name_iter, old_component_end) !=
                            std::distance(new_name_iter, new_component_end) ||
          !equal(old_name_iter, old_component_end, new_name_iter)) {
        /* The namespaces differ. */
        break;
      }  /* if */
      /* Advance to the next component. */
      old_name_iter = old_component_end;
      if (old_name_iter != old_name_end) {
        ++old_separators_iter;
        old_name_iter += a_qualified_name::separator_length;
      }  /* if */
      new_name_iter = new_component_end;
      if (new_name_iter != new_name_end) {
        ++new_separators_iter;
        new_name_iter += a_qualified_name::separator_length;
      }  /* if */
      if (old_name_iter == old_name_end || new_name_iter == new_name_end) {
        /* We have reached the end of at least one of the namespaces. */
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  /* [old_name_iter, old_name_end) addresses the (possibly empty) range of
     characters of the namespace(s) that should be closed.
     [old_separators_iter, old_separators_end) addresses the (possibly
     empty) range of separator offsets, relative to old_name_begin, of any
     separators within that range.  The new_* iterators correspond in a
     similar manner to the namespace(s) that should be opened. */
  if (old_name_iter != old_name_end) {
    for (;;) {
      auto old_component_begin = (old_separators_iter == old_separators_end)
                                         ? old_name_iter
                                         : old_name_begin +
                                           *(old_separators_end - 1) +
                                           a_qualified_name::separator_length;
      close_namespace(buffer, old_component_begin, old_name_end);
      if (old_name_iter == old_component_begin) {
        break;
      }  /* if */
      --old_separators_end;
      old_name_end = old_component_begin - a_qualified_name::separator_length;
    }  /* for */
  }  /* if */
  if (new_name_iter != new_name_end) {
    for (;;) {
      auto new_component_end = (new_separators_iter == new_separators_end)
                                       ? new_name_end
                                       : new_name_begin + *new_separators_iter;
      open_namespace(buffer, new_name_iter, new_component_end);
      if (new_component_end == new_name_end) {
        break;
      }  /* if */
      ++new_separators_iter;
      new_name_iter = new_component_end + a_qualified_name::separator_length;
    }  /* for */
  }  /* if */
  active_namespace_ = move(namespace_name);
}  /* an_import_scope::set_namespace_scope */


bool a_custom_attribute::is_standard_attribute(
                                          a_standard_attribute_flag saf) const
{
  bool result = false;
  switch (saf) {
  case saf_attribute_usage_attribute:
    result = type_name() == ATTRIBUTE_USAGE_ATTRIBUTE;
    break;
  case saf_obsolete_attribute:
    result = type_name() == OBSOLETE_ATTRIBUTE;
    break;
  case saf_default_member_attribute:
    result = type_name() == DEFAULT_MEMBER_ATTRIBUTE;
    break;
  case saf_flags_attribute:
    result = type_name() == FLAGS_ATTRIBUTE;
    break;
  case saf_browsable_attribute:
    result = type_name() == BROWSABLE_ATTRIBUTE;
    break;
  case saf_editor_browsable_attribute:
    result = type_name() == EDITOR_BROWSABLE_ATTRIBUTE;
    break;
  case saf_description_attribute:
    result = type_name() == DESCRIPTION_ATTRIBUTE;
    break;
  case saf_help_keyword_attribute:
    result = type_name() == HELP_KEYWORD_ATTRIBUTE;
    break;
  case saf_display_name_attribute:
    result = type_name() == DISPLAY_NAME_ATTRIBUTE;
    break;
  case saf_allow_multiple_attribute:
    result = type_name() == ALLOW_MULTIPLE_ATTRIBUTE;
    break;
  case saf_deprecated_attribute:
    result = type_name() == DEPRECATED_ATTRIBUTE;
    break;
  default:
    unexpected_condition();
    break;
  }  /* switch */
  return result;
}  /* a_custom_attribute::is_standard_attribute */


bool a_custom_attribute::requires_type_definition(
                               const a_type_definition &type_definition) const
{
  return class_type() == type_definition.class_type();
}  /* a_custom_attribute::requires_type_definition */


an_import_interface_ptr a_custom_attribute::import_interface() const
{
  return import_scope_->import_interface();
}  /* a_custom_attribute::import_interface */


void a_custom_attribute::decode_type() const
{
  if (IsNilToken(ctor_token_)) {
    HRESULT    hr;
    const BYTE *data_ptr;
    ULONG      bytes_in_data;
    USHORT     prolog;

    check_assertion(import_interface() != nullptr && !IsNilToken(token_));
    hr = import_interface()->GetCustomAttributeProps(
                                    token_, /*ptkObj=*/nullptr, &ctor_token_,
                                    reinterpret_cast<const void**>(&data_ptr),
                                    &bytes_in_data);
    CHECK_API_RESULT(hr, GetCustomAttributeProps);
    if (IsNilToken(ctor_token_)) {
      unexpected_condition();
    } else {
      /* Determine the function type of the attribute's constructor. */
      mdToken type_token = mdTypeDefNil;
      switch (TypeFromToken(ctor_token_)) {
        case mdtMethodDef:
          hr = import_interface()->GetMethodProps(ctor_token_,
                                                  &type_token,
                                                  /*pdwAttr=*/nullptr,
                                                  &signature_,
                                                  &bytes_in_signature_,
                                                  /*pulCodeRVA=*/nullptr,
                                                  /*pdwImplFlags=*/nullptr);
          CHECK_API_RESULT(hr, GetMethodProps);
          break;
        case mdtMemberRef:
          hr = import_interface()->GetMemberRefProps(ctor_token_,
                                                     &type_token,
                                                     &signature_,
                                                     &bytes_in_signature_);
          CHECK_API_RESULT(hr, GetMethodProps);
          break;
        default:
          unexpected_condition();
          break;
      }  /* switch */
      /* The global scope can be used to decode type of the custom attribute,
         as it will not contain any references to generic arguments. */
      if (!IsNilToken(type_token)) {
        class_type_ =
                   a_global_scope(*import_scope_).type_from_token(type_token);
        if (!class_type_->is_unresolved_type() &&
          data_ptr != nullptr && bytes_in_data > 0) {
          data_ = a_custom_attribute_data(import_scope_, data_ptr,
            bytes_in_data);
          /* Read the prolog that starts the custom attribute data. */
          data_.read_and_advance(prolog);
          if (prolog != 0x0001) {
            unexpected_condition();
          }  /* if */
        }  /* if */
      } else {
        unexpected_condition();
      }  /* if */
    }  /* if */
  }  /* if */
}  /* a_custom_attribute::decode_type */


void a_custom_attribute::decode_fixed_args() const
{
  /* Ensure the type has been decoded. */
  decode_type();
  if (!data_.empty()) {
    auto type = a_signature_decoder::decode_method(
                                             a_global_scope(*import_scope_),
                                             ctor_token_,
                                             signature_, bytes_in_signature_);
    if (!type->is_invalid()) {
      a_function_type_wrapper_ptr ctor_type = type->as_function();
      check_assertion(ctor_type != nullptr);
      auto &parameter_list = ctor_type->parameter_list();
      /* The number of fixed arguments is equal to the number of parameters in
         the constructor. */
      fixed_args_.reserve(parameter_list.size());
      for (auto &param : parameter_list) {
        auto arg_type = param.type();
        if (arg_type != nullptr) {
          an_attribute_argument          fixed_arg(data_, arg_type);
          a_const_class_type_wrapper_ptr unresolved_type;
          if (fixed_arg.type()->uses_unresolved_type(unresolved_type)) {
            class_type_ = unresolved_type;
            fixed_args_.clear();
            break;
          }  /* if */
          fixed_args_.emplace_back(move(fixed_arg));
        } else {
          unexpected_condition();
          break;
        }  /* if */
      }  /* for */
    }  /* if */
  }  /* if */
}  /* a_custom_attribute::decode_fixed_args */


void a_custom_attribute::decode_named_args() const
{
  /* Ensure the fixed arguments have been decoded. */
  decode_fixed_args();
  if (!data_.empty()) {
    USHORT num_named;
    data_.read_and_advance(num_named);
    named_args_.reserve(num_named);
    for (UINT index = 0; index < num_named; ++index) {
      auto arg_type = data_.read_serialized_type_and_advance();
      /* Read the field or property name. */
      unique_ptr<wstring> name;
      data_.read_and_advance(name);
      if (name != nullptr) {
        an_attribute_argument          named_arg(data_, arg_type, *name);
        a_const_class_type_wrapper_ptr unresolved_type;
        if (named_arg.type()->uses_unresolved_type(unresolved_type)) {
          class_type_ = unresolved_type;
          fixed_args_.clear();
          named_args_.clear();
          break;
        }  /* if */
        named_args_.emplace_back(move(named_arg));
      } else {
        unexpected_condition();
        break;
      }  /* if */
    }  /* for */
    /* Ensure we have consumed all of the custom attribute data. */
    check_assertion(data_.empty());
  }  /* if */
}  /* a_custom_attribute::decode_named_args */


a_custom_attribute_list::a_custom_attribute_list(
                                        a_const_import_scope_ptr import_scope,
                                        mdToken                  token)
  : import_scope_(import_scope)
  , token_(token)
  , standard_attribute_flags_(saf_none)
{
  HCORENUM          enum_custom_attributes = nullptr;
  mdCustomAttribute custom_attributes[16];
  ULONG             count_of_custom_attributes;
  do {
    HRESULT hr = import_interface()->EnumCustomAttributes(
                                                 &enum_custom_attributes,
                                                 token_,
                                                 /*tkType=*/0,
                                                 custom_attributes,
                                                 _countof(custom_attributes),
                                                 &count_of_custom_attributes);
    CHECK_API_RESULT(hr, EnumCustomAttributes);
    for (ULONG i = 0; i < count_of_custom_attributes; ++i) {
      a_custom_attribute custom_attribute(import_scope_,
                                          custom_attributes[i]);
      if (process_attribute(custom_attribute)) {
        custom_attributes_.emplace_back(move(custom_attribute));
      }  /* if */
    }  /* for */
  } while (count_of_custom_attributes > 0);
  import_interface()->CloseEnum(enum_custom_attributes);
}  /* a_custom_attribute_list Constructor. */


wstring a_custom_attribute_list::get_source_code(
                              const a_type_definition *type_context,
                              const wstring           &attribute_target) const
/*
Returns the source code for the list of custom attributes.
*/
{
  wstring result;
  bool    skip = false;

  if (type_context != nullptr) {
    auto type_context_name = type_context->qualified_name();
    if (type_context->kind() == a_type_definition::tdk_ref_class) {
      skip = type_context_name == MAKE_CLASS_STRING(Object) ||
             type_context_name == ATTRIBUTE_ATTRIBUTE ||
             type_context_name == ATTRIBUTE_USAGE_ATTRIBUTE ||
             type_context_name == DEFAULT_MEMBER_ATTRIBUTE;
    } else if (type_context->kind() ==
                                     a_type_definition::tdk_interface_class) {
      if (!is_cppcx_metadata) {
        skip = type_context_name == ATTRIBUTE_INTERFACE;
      }  /* if */
    }  /* if */
  }  /* if */
  if (!skip) {
    for (auto &custom_attribute : custom_attributes_) {
      if (type_context == nullptr ||
          !custom_attribute.requires_type_definition(*type_context)) {
        result += custom_attribute.get_source_code(attribute_target);
      }  /* if */
    }  /* for */
  }  /* if */
  return result;
}  /* a_custom_attribute_list::get_source_code */


bool a_custom_attribute_list::process_standard_attribute(
                                  a_standard_attribute_flag saf,
                                  const a_custom_attribute  &custom_attribute)
{
  bool result = false;
  switch (saf) {
    case saf_default_member_attribute:
      result = TypeFromToken(token()) == mdtTypeDef &&
               a_default_member_attribute::create(custom_attribute,
                                                  default_member_attribute_);
      break;
    case saf_allow_multiple_attribute:
      result = TypeFromToken(token()) == mdtTypeDef &&
               custom_attribute.is_standard_attribute(saf);
      break;
    default:
      result = custom_attribute.is_standard_attribute(saf);
      break;
  }  /* switch */
  if (result) standard_attribute_flags_ |= saf;
  return result;
}  /* a_custom_attribute_list::process_standard_attribute */


bool a_custom_attribute_list::process_attribute(
                                   const a_custom_attribute &custom_attribute)
{
  auto import_flags = import_scope_->containing_assembly().import_flags();
  bool import_all_attributes =
                          (import_flags & cpp_cli_all_custom_attributes) != 0;
  bool import_ide_attributes =
                          (import_flags & cpp_cli_ide_custom_attributes) != 0;
  bool import_cppcx_attributes =
                     import_scope_->containing_assembly().is_cppcx_metadata();
  bool import_cli_attributes = !import_cppcx_attributes;
  bool result = import_all_attributes;

  a_standard_attribute_flag standard_attributes[] = {
    saf_attribute_usage_attribute,
    saf_default_member_attribute,
    saf_obsolete_attribute,
    saf_flags_attribute,
    saf_browsable_attribute,
    saf_editor_browsable_attribute,
    saf_description_attribute,
    saf_help_keyword_attribute,
    saf_display_name_attribute,
    saf_allow_multiple_attribute,
    saf_deprecated_attribute,
  };
  for (const auto &saf : standard_attributes) {
    bool is_ide_attribute = (saf & saf_ide_custom_attribute) != 0;
    bool is_cli_attribute = (saf & saf_cli_custom_attribute) != 0;
    bool is_cppcx_attribute = (saf & saf_cppcx_custom_attribute) != 0;
    if (!has_attribute(saf) &&
        (import_all_attributes ||
         ((!is_ide_attribute || import_ide_attributes) &&
          (!is_cli_attribute || import_cli_attributes) &&
          (!is_cppcx_attribute || import_cppcx_attributes)))) {
      result = process_standard_attribute(saf, custom_attribute);
      if (result) break;
    }  /* if */
  }  /* for */
  if (result) {
    auto attribute_type = custom_attribute.class_type();
    check_assertion(attribute_type != nullptr);
    if (attribute_type->is_of_class_kind(a_class_type_wrapper::ck_class)) {
      auto type_definition = attribute_type->type_definition();
      if (type_definition == nullptr ||
          !type_definition->accessibility().is_accessible()) {
        /* Skip the custom attribute if the class is inaccessible. */
        result = false;
      } else {
        /* We should skip the custom attribute if the constructor is
           inaccessible.  This will require doing something similar to
           an_import_scope::get_overridden_name to map an mdtMemberRef
           to an mdtMethodDef.  Because the standard attributes have public
           constructors, this is only necessary in when the flag
           cpp_cli_all_custom_attributes is used, which is not currently the
           case (therefore, implementing this behavior is not an immediate
           priority). */
      }  /* if */
    }  /* if */
  }  /* if */
  return result;
}  /*  a_custom_attribute_list::process_attribute */


an_import_interface_ptr a_custom_attribute_list::import_interface() const
{
  return import_scope_->import_interface();
}  /* a_custom_attribute_list::import_interface */


void an_assembly::import_all_types(ostringstream &buffer)
/*
Import the types from all of the scopes (aka modules) in the assembly.
*/
{
  for (auto &scope : imported_scopes_) {
    scope->import_all_types(buffer);
  }  /* for */
}  /* an_assembly::import_all_types */


bool an_assembly::get_assembly_info()
/*
Get all the pertinent information associated with this assembly.
*/
{
  HRESULT hr = E_FAIL;
  DWORD   dwOpenFlags = 0;
  CComQIPtr<IALink2> alink2_interface = alink_interface_;

  if (alink2_interface != nullptr) {
    if (cppcx_enabled) {
      const unsigned ofCPPNoTransformElementType = 0x80000000;
      dwOpenFlags = ofNoTransform | ofCPPNoTransformElementType;
    } else if (cppcli_enabled) {
      dwOpenFlags = ofReadOnly | ofNoTypeLib;
    } else {
      unexpected_condition();
    }  /* if */
    hr = alink2_interface->ImportFileEx(assembly_path_.c_str(),
                                        /*pszTargetName=*/nullptr,
                                        /*fSmartImport=*/FALSE,
                                        dwOpenFlags,
                                        &alink_token_,
                                        &md_assembly_import_interface_,
                                        &count_of_scopes_);
  }  /* if */
  if (SUCCEEDED(hr)) {
    hr = alink_interface_->GetResolutionScope(AssemblyIsUBM, file_token_,
                                              alink_token_,
                                              &resolution_scope_);
    CHECK_API_RESULT(hr, GetResolutionScope);
  }  /* if */
  /* If the metadata originates from an assembly, as opposed to a netmodule,
     initialize the assembly name so that it can be located by
     find_assembly_by_name when resolving references to types located in other
     assemblies. */
  if (SUCCEEDED(hr) && md_assembly_import_interface_ != nullptr) {
    init_assembly_name();
  }  /* if */
  return SUCCEEDED(hr);
}  /* an_assembly::get_assembly_info */


bool an_assembly::init_assembly_import_interface()
/*
Initialize the IMetaDataAssemblyImport interface.  For assemblies, this is
initialized by the call to IALink2::ImportFileEx when the assembly is opened.
However, for netmodules, this interface is provided by the same object that
provides the IMetaDataImport2 interface.
*/
{
  if (md_assembly_import_interface_ == nullptr) {
    if (count_of_scopes_ == 1) {
      HRESULT hr;
      hr = import_scope_from_index(0).import_interface()->QueryInterface(
                    IID_IMetaDataAssemblyImport,
                    reinterpret_cast<void**>(&md_assembly_import_interface_));
      CHECK_API_RESULT(hr, QueryInterface);
    } else {
      unexpected_condition();
    }  /* if */
  }  /* if */
  return md_assembly_import_interface_ != nullptr;
}  /* an_assembly::init_assembly_import_interface */


an_import_scope& an_assembly::import_scope_from_index(int scope_index)
{
  return *imported_scopes_[scope_index];
}  /* an_assembly::import_scope_from_index */


a_const_import_scope_ptr an_assembly::find_import_scope_by_name(
                                              const wstring &scope_name) const
{
  a_const_import_scope_ptr import_scope_ptr = nullptr;
  for (auto &import_scope : imported_scopes_) {
    if (import_scope->scope_name() == scope_name) {
      import_scope_ptr = import_scope.get();
      break;
    }  /* if */
  }  /* for */
  return import_scope_ptr;
}  /* an_assembly::find_import_scope_by_name */


bool an_assembly::find_nested_type_by_name(
                               a_const_import_scope_ptr import_scope,
                               mdTypeDef                parent_token,
                               const wstring            &type_name,
                               mdTypeDef                &resolved_token) const
{
  bool result = false;
  auto type_name_begin = type_name.cbegin();
  auto type_name_end = type_name.cend();
  auto plus_iter = find_unescaped_character(L'+', type_name_begin,
                                            type_name_end);
  if (plus_iter != type_name_end) {
    wstring parent_type_name(type_name_begin, plus_iter);
    HRESULT hr = import_scope->import_interface()->
                                   FindTypeDefByName(parent_type_name.c_str(),
                                                     parent_token,
                                                     &parent_token);
    if (SUCCEEDED(hr)) {
      wstring nested_type_name(plus_iter + 1, type_name_end);
      result = find_nested_type_by_name(import_scope, parent_token,
                                        nested_type_name, resolved_token);
    }  /* if */
  } else {
    mdTypeDef typedef_token;
    HRESULT   hr = import_scope->import_interface()->
                                          FindTypeDefByName(type_name.c_str(),
                                                            parent_token,
                                                            &typedef_token);
    if (SUCCEEDED(hr)) {
      resolved_token = typedef_token;
      result = true;
    }  /* if */
  }  /* if */
  return result;
}  /* an_assembly::find_nested_type_by_name */


bool an_assembly::find_type_by_name(
                              const wstring            &type_name,
                              a_const_import_scope_ptr &resolved_import_scope,
                              mdTypeDef                &resolved_token) const
{
  bool result = false;
  auto type_name_begin = type_name.cbegin();
  auto type_name_end = type_name.cend();
  auto plus_iter = find_unescaped_character(L'+', type_name_begin,
                                                  type_name_end);
  if (plus_iter != type_name_end) {
    /* If the type is a nested type, type_name will contain an unescaped '+'
       character that separates the enclosing type name from the nested type
       name. */
    a_const_import_scope_ptr import_scope = nullptr;
    mdTypeDef                parent_token = mdTypeDefNil;
    wstring                  parent_type_name(type_name_begin, plus_iter);
    if (find_type_by_name(parent_type_name, import_scope, parent_token)) {
      wstring nested_type_name(plus_iter + 1, type_name_end);
      result = find_nested_type_by_name(import_scope, parent_token,
                                        nested_type_name, resolved_token);
    }  /* if */
  } else {
    mdTypeDef typedef_token;
    for (auto &import_scope : imported_scopes_) {
      HRESULT hr = import_scope->import_interface()->
                            FindTypeDefByName(type_name.c_str(),
                                              /*tkEnclosingClass=*/mdTokenNil,
                                              &typedef_token);
      if (SUCCEEDED(hr)) {
        resolved_import_scope = import_scope.get();
        resolved_token = typedef_token;
        result = true;
        break;
      }  /* if */
    }  /* for */
  }  /* if */
  check_assertion(!result ||
                  (resolved_import_scope != nullptr &&
                   !IsNilToken(resolved_token) &&
                   TypeFromToken(resolved_token) == mdtTypeDef));
  return result;
}  /* an_assembly::find_type_by_name */


bool an_assembly::import_all_scopes()
/*
Import all the import scopes associated with this assembly.
*/
{
  bool processed_an_interesting_scope = false;
  a_scope_index imported_scope_index = 0;

  for (DWORD scope_index = 0; scope_index < count_of_scopes_; ++scope_index) {
    CComPtr<IMetaDataImport>         md_import_inferface;
    CComQIPtr<an_import_interface,
              &IID_IMetaDataImport2> md_import2_inferface;
    HRESULT                          hr;
    hr = alink_interface_->GetScope(AssemblyIsUBM, alink_token_, scope_index,
                                    &md_import_inferface);
    if (FAILED(hr)) {
      continue;
    } else if ((hr == S_FALSE) || (md_import_inferface == nullptr)) {
      /* There are no types in this scope.  Skip it. */
      continue;
    }  /* if */
    /* Query interface to the new, improved interface. */
    md_import2_inferface = md_import_inferface;
    if (md_import2_inferface == nullptr) {
      continue;
    }  /* if */
    /* Determine if this is C++/CX metadata. */
    if (is_platform_winmd()) {
      is_cppcx_metadata_ = true;
    } else {
      wchar_t metaDataVersion[1024];
      hr = md_import2_inferface->GetVersionString(metaDataVersion,
                                                  _countof(metaDataVersion),
                                                  /*pccBufSize=*/nullptr);
      if (SUCCEEDED(hr) &&
          (wcsstr(metaDataVersion, L"WindowsRuntime 1.4") != nullptr ||
           wcsstr(metaDataVersion, L"WindowsRuntime 1.3") != nullptr ||
           wcsstr(metaDataVersion, L"WindowsRuntime 1.2") != nullptr)) {
        is_cppcx_metadata_ = true;
      }  /* if */
    }  /* if */
    /* Create an import scope and import all the types. */
    auto import_scope = make_unique_ptr<an_import_scope>(
                                                     imported_scope_index++,
                                                     md_import2_inferface,
                                                     this);
    imported_scopes_.emplace_back(move(import_scope));
    processed_an_interesting_scope = true;
  }  /* for */
  return processed_an_interesting_scope;
}  /* an_assembly::import_all_scopes */


bool an_assembly::process()
/*
Process a single assembly.  This includes getting information about the
assembly and importing its scopes.
*/
{
  bool result = false;

  if (get_assembly_info()) {
    check_assertion(count_of_scopes_ > 0);
    if (import_all_scopes() && init_assembly_import_interface()) {
      result = true;
    }  /* if */
  }  /* if */
  return result;
}  /* an_assembly::process */


auto a_type_definition::get_enumerators() const -> a_const_enumerator_list_ref
/*
Import the list of enumerators for a enum type definition.
*/
{
  check_assertion(kind() == tdk_enum_class);
  if (!enumerators_initialized_) {
    HCORENUM              enum_fields = nullptr;
    mdFieldDef            fields[64];
    ULONG                 count_of_fields;
    HRESULT               hr;
    do {
      hr = import_interface()->EnumFields(&enum_fields, token_,
                                          fields, _countof(fields),
                                          &count_of_fields);
      CHECK_API_RESULT(hr, EnumFields);
      /* Reserve the necessary space. */
      enumerators_.reserve(enumerators_.size() + count_of_fields);
      for (ULONG i = 0; i < count_of_fields; ++i) {
        wstring         field_name;
        ULONG           characters_in_constant;
        DWORD           field_attributes, constant_type;
        PCCOR_SIGNATURE signature;
        ULONG           bytes_in_signature;
        UVCP_CONSTANT   constant_value;
        /* Get the name, the type and the constant associated with this
           enumerator. */
        hr = import_interface()->GetFieldProps(
                                              fields[i], /*pClass=*/nullptr,
                                              field_name, &field_attributes,
                                              &signature, &bytes_in_signature,
                                              &constant_type, &constant_value,
                                              &characters_in_constant);
        CHECK_API_RESULT(hr, GetFieldProps);
        if (IsFdLiteral(field_attributes)) {
          a_constant_value constant(constant_type, constant_value,
                                    characters_in_constant);
          /* Get the constant value associated with it. */
          escape_invalid_identifier(field_name);
          enumerators_.emplace_back(field_name, move(constant));
        } else {
          check_assertion(field_name == COR_ENUM_FIELD_NAME_W);
        }  /* if */
      }  /* for */
    } while (count_of_fields > 0);
    import_interface()->CloseEnum(enum_fields);
    enumerators_initialized_ = true;
  }  /* if */
  return enumerators_;
}  /* a_type_definition::get_enumerators */


void a_type_definition::import_enum_definition(ostringstream &buffer) const
/*
Import the definition of an enumeration and emit the code for the definition.
*/
{
  auto const &underlying_type = this->underlying_type();
  if (underlying_type != nullptr) {
    buffer << type_name_ << " : " << underlying_type->get_string();
    if (kind_ == tdk_native_enum) {
      /* Only emit a forward declaration for a native enum. */
      buffer << ";" << END_OF_LINE;
    } else if (kind_ == tdk_enum_class) {
      /* At the moment we can't forward declare a C++/CLI enumeration so we
         need to import (and emit) the full definition. */
      buffer << " {" << END_OF_LINE;
      auto &enumerators = get_enumerators();
      for (auto enum_iter = enumerators.cbegin();
           enum_iter != enumerators.cend();
           ++enum_iter) {
        auto &field_name = enum_iter->first;
        auto &constant = enum_iter->second;
        buffer << field_name << " = ";
        buffer << constant.get_source_code(underlying_type);
        if (enum_iter + 1 != enumerators.cend()) {
          buffer << ",";
        }  /* if */
        buffer << END_OF_LINE;
      }  /* for */
      buffer << "};";
#if DEBUG
      buffer << "  /* enum " << type_name_ << " */";
#endif /* DEBUG */
      buffer << END_OF_LINE;
    } else {
      unexpected_condition();
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
}  /* a_type_definition::import_enum_definition */


void a_type_definition::import_delegate_definition(
                                                  ostringstream &buffer) const
/*
Import the definition of a delegate.  This is essentially the signature of
the Invoke method - which every delegate must have.
*/
{
  HCORENUM                  enum_methods = nullptr;
  mdMethodDef               methods[2];
  ULONG                     count_of_methods;
  HRESULT                   hr;

  hr = import_interface()->EnumMethodsWithName(&enum_methods,
                                               token_,
                                               L"Invoke", methods,
                                               _countof(methods),
                                               &count_of_methods);
  CHECK_API_RESULT(hr, EnumMethodsWithName);
  /* There should be only one Invoke method.  Get the signature and
     decode it. */
  if (count_of_methods == 1) {
    auto method_definition = import_scope_.get_method_definition(methods[0]);
    check_assertion(method_definition->name() == L"Invoke");
    auto function_type = method_definition->function_type();
    if (!function_type->is_invalid()) {
      buffer << method_definition->generic_header()
             << function_type->get_string(type_name_) << ';'
             << END_OF_LINE;
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
  import_interface()->CloseEnum(enum_methods);
}  /* a_type_definition::import_delegate_definition */


BYTE an_import_scope::get_generic_parameter_count(mdTypeDef token) const
/*
Return the number of generic parameters associated with the specified type
token.  For nested types, this may differ from the generic arity, as generic
parameters from enclosing types are included in the count.
*/
{
  HRESULT  hr;
  HCORENUM enum_parameters = nullptr;
  ULONG    count_of_parameters;

  check_assertion(TypeFromToken(token) == mdtTypeDef);
  hr = import_interface_->EnumGenericParams(&enum_parameters,
                                            token,
                                            /*rGenericParams=*/nullptr,
                                            /*cMax=*/0,
                                            &count_of_parameters);
  CHECK_API_RESULT(hr, EnumGenericParams);
  hr = import_interface_->CountEnum(enum_parameters, &count_of_parameters);
  CHECK_API_RESULT(hr, CountEnum);
  import_interface_->CloseEnum(enum_parameters);
  return static_cast<BYTE>(count_of_parameters);
}  /* an_import_scope::get_generic_parameter_count */


an_import_scope& an_import_scope::operator=(an_import_scope&& other)
{
  scope_index_ = move(other.scope_index_);
  scope_name_ = move(other.scope_name_);
  containing_assembly_ = move(other.containing_assembly_);
  import_interface_ = move(other.import_interface_);
  active_namespace_ = move(other.active_namespace_);
  map_typedef_to_definition_ = move(other.map_typedef_to_definition_);
  map_typeref_to_class_type_ = move(other.map_typeref_to_class_type_);
  map_token_to_custom_attributes_ =
                                  move(other.map_token_to_custom_attributes_);
  return *this;
}  /* an_import_scope::operator= */


string a_type_definition::process_base_class_list(ostringstream& buffer) const
{
  ostringstream interface_list;
  auto import_flags = import_scope_.containing_assembly().import_flags();
  bool use_pending_implements_clause =
                                   !(import_flags & cpp_cli_define_all_types);
  bool base_class_processed = process_base_class(buffer);

  process_interfaces(interface_list);
  string pending_interface_list = interface_list.str();
  if (!pending_interface_list.empty()) {
    buffer << (base_class_processed ? ", " : " : ");
    if (use_pending_implements_clause) {
      buffer << "__implements ...";
    } else {
      buffer << pending_interface_list;
      pending_interface_list.clear();
    }  /* if */
  }  /* if */
  return pending_interface_list;
}  /* a_type_definition::process_base_class_list */


bool a_type_definition::process_base_class(ostringstream& buffer) const
/*
Emit the appropriate text for the base class.
*/
{
  bool base_class_processed = false;
  auto base_class = base_class_type();

  if (base_class != nullptr) {
    a_qualified_name base_class_name = base_class->name();
    if (kind() == tdk_value_class) {
      /* By definition all value types extend System.ValueType so there is no
         need to explicitly add it as a base-class. */
      check_assertion(base_class_name == MAKE_CLASS_STRING(ValueType));
    } else {
      /* Similarly with ref classes: by definition they all extend (directly
         or indirectly) System.Object. */
      if (kind() != tdk_ref_class ||
          base_class_name != MAKE_CLASS_STRING(Object)) {
        buffer << " : " << base_class_name.as_string();
        base_class_processed = true;
      }  /* if */
    }  /* if */
  }  /* if */
  return base_class_processed;
}  /* a_type_definition::process_base_class */


void a_type_definition::process_interfaces(ostringstream &buffer) const
/*
Decode the interface tokens (if there are any) and emit the appropriate text.
*/
{
  HCORENUM enum_interfaces = nullptr;
  mdToken  interfaces[8];
  ULONG    count_of_interfaces;
  HRESULT  hr;
  bool     is_first_interface = true;

  do {
    hr = import_interface()->EnumInterfaceImpls(&enum_interfaces,
                                                token_, interfaces,
                                                _countof(interfaces),
                                                &count_of_interfaces);
    CHECK_API_RESULT(hr, EnumInterfaceImpls);
    for (ULONG i = 0; i < count_of_interfaces; ++i) {
      auto interface_type = type_from_token(interfaces[i]);
      bool skip_interface = false;
      if (interface_type->is_unresolved_type()) {
        /* The assembly containing this interface has not been imported. */
        skip_interface = true;
      } else {
        /* Inaccessible interface definitions are not imported, but accessible
           types are permitted to implement such interfaces.  Therefore, we
           must also skip them here. */
        auto &interface_definition = interface_type->import_scope()->
                                 get_type_definition(interface_type->token());
        if (!interface_definition.accessibility().is_accessible()) {
          skip_interface = true;
        }  /* if */
      }  /* if */
      if (!skip_interface) {
        if (is_first_interface) {
          is_first_interface = false;
        } else {
          buffer << ", ";
        }  /* if */
        buffer << interface_type->name().as_string();
      }  /* if */
    }  /* for */
  } while (count_of_interfaces > 0);
  import_interface()->CloseEnum(enum_interfaces);
}  /* a_type_definition::process_interfaces */


void a_type_definition::write_custom_attributes(
                                        ostringstream &buffer,
                                        mdToken       token,
                                        const wstring &attribute_target) const
{
  if (!IsNilToken(token)) {
    auto &custom_attributes = import_scope_.get_custom_attributes(token);
    buffer << custom_attributes.get_source_code(this, attribute_target);
  }  /* if */
}  /* a_type_definition::write_custom_attributes */


void a_type_definition::write_method_decl_specifiers(
                                        ostringstream &buffer,
                                        mdToken       token,
                                        DWORD         method_attributes) const
/*
Write the decl specifiers for a property or method.
*/
{
  if ((import_scope_.containing_assembly().import_flags()
                                       & cpp_cli_declspec_member_info) != 0) {
    buffer << "__declspec(member_info(";
    buffer << "0x" << setw(8) << setfill('0') << hex << token;
    buffer << ")) ";
  }  /* if */
  /* Emit any decl specifiers. */
  if (IsMdStatic(method_attributes)) {
    buffer << "static ";
    check_assertion(!IsMdVirtual(method_attributes));
  } else if (IsMdVirtual(method_attributes)) {
    buffer << "virtual ";
  }  /* if */
}  /* a_type_definition::write_method_decl_specifiers */


void a_type_definition::import_one_method(
                           ostringstream             &buffer,
                           const a_method_definition &method_definition) const
/*
Import a single member of a type.
*/
{
  if (method_definition.accessibility().is_accessible()) {
    auto method_type = method_definition.function_type();
    if (!method_type->is_invalid()) {
      auto method_attributes = method_definition.attributes();
      buffer << method_definition.accessibility().get_string() << ": ";
      buffer << method_definition.generic_header();
      /* Emit any custom attributes for the method. */
      write_custom_attributes(buffer, method_definition.token());
      /* Emit any custom attributes for the method's return value. */
      write_custom_attributes(buffer, method_type->return_value().token(),
                              L"returnvalue: ");
      /* Emit any decl specifiers. */
      write_method_decl_specifiers(buffer, method_definition.token(),
                                   method_attributes);
      if (method_definition.cli_operator_kind() == cok_explicit) {
        /* Emit an explicit user-defined conversion operator. */
        buffer << "explicit ";
      }  /* if */
      /* Emit the declaration proper (the "type" part of the decl-specifiers
         and the declarator). */
      buffer << method_type->get_string(method_definition.name());
      if (IsMdFinal(method_attributes)) {
        buffer << " sealed";
      }  /* if */
      if (IsMdNewSlot(method_attributes)) {
        if (kind() != tdk_interface_class) {
          buffer << " new";
        }  /* if */
      } else if (IsMdVirtual(method_attributes)) {
        buffer << " override";
      }  /* if */
      /* Emit any named overrides. */
      if (IsMdVirtual(method_attributes)) {
        const auto &named_overrides = get_named_overrides();
        auto named_override_iter = named_overrides.find(
                                                   method_definition.token());
        if (named_override_iter != named_overrides.end()) {
          buffer << named_override_iter->second;
        }  /* if */
      }  /* if */
      buffer << ';' << END_OF_LINE;
    }  /* if */
  }  /* if */
}  /* a_type_definition::import_one_method */


a_type_definition::a_const_named_override_map_ref
a_type_definition::get_named_overrides() const
/*
Initialize named_overrides_, which maps a virtual function in this class to
the list of accessible base class virtual functions that it explicitly
overrides.
*/
{
  if (!named_overrides_initialized_) {
    HRESULT            hr;
    HCORENUM           enum_method_impls = NULL;
    static const ULONG max_method_impls = 64;
    mdToken            method_bodies[max_method_impls];
    mdToken            method_decls[max_method_impls];
    ULONG              count_method_impls = 64;
    switch (kind()) {
      case tdk_ref_class:
      case tdk_value_class:
      case tdk_interface_class:
        do {
          hr = import_interface()->EnumMethodImpls(&enum_method_impls,
                                                   token_,
                                                   method_bodies,
                                                   method_decls,
                                                   max_method_impls,
                                                   &count_method_impls);
          CHECK_API_RESULT(hr, EnumMethodImpls);
          for (ULONG i = 0; i < count_method_impls; ++i) {
            /* The MethodImpl table in the metadata is permitted to use an
               mdtMemberRef token to refer to a named-override method in this
               class.  This is currently not supported.  Adding such support
               would require doing something similar to get_overridden_name to
               map an mdtMemberRef to an mdtMethodDef. */
            check_assertion(TypeFromToken(method_bodies[i]) == mdtMethodDef);
            auto method_definition = import_scope_.get_method_definition(
                                                            method_bodies[i]);
            wstring overridden_name = import_scope_.get_overridden_name(
                                                           *method_definition,
                                                           method_decls[i]);
            if (!overridden_name.empty()) {
              auto &overridden_name_list = named_overrides_[method_bodies[i]];
              overridden_name_list += overridden_name_list.empty() ? L" = "
                                                                   : L", ";
              overridden_name_list += overridden_name;
            }  /* if */
          }  /* for */
        } while (count_method_impls > 0);
        import_interface()->CloseEnum(enum_method_impls);
        break;
      default:
        break;
    }  /* switch */
    named_overrides_initialized_ = true;
  }  /* if */
  return named_overrides_;
}  /* a_type_definition::get_named_overrides */


void a_type_definition::import_all_methods(ostringstream &buffer) const
/*
Import all the members associated with a single type.
*/
{
  HCORENUM    enum_methods = NULL;
  mdMethodDef methods[64];
  ULONG       count_of_methods;
  HRESULT     hr;

  do {
    hr = import_interface()->EnumMethods(&enum_methods, token_,
                                         methods, _countof(methods),
                                         &count_of_methods);
    CHECK_API_RESULT(hr, EnumMethods);
    for (ULONG i = 0; i < count_of_methods; ++i) {
      auto method_definition = import_scope_.get_method_definition(
                                                                  methods[i]);
      /* Skip property/event accessor methods, as they are imported by the
         corresponding property/event. */
      if (!method_definition->is_event_or_property_accessor()) {
        import_one_method(buffer, *method_definition);
      }  /* if */
    }  /* for */
  } while (count_of_methods > 0);
  import_interface()->CloseEnum(enum_methods);
}  /* a_type_definition::import_all_methods */


void a_type_definition::import_one_field(ostringstream &buffer,
                                         mdFieldDef    field_token) const
/*
Import a single field of a type.
*/
{
  bool                      skip_member = false;
  HRESULT                   hr;
  ULONG                     bytes_in_signature;
  ULONG                     characters_in_constant;
  DWORD                     field_attributes, constant_type;
  PCCOR_SIGNATURE           signature;
  UVCP_CONSTANT             constant_value;
  wstring                   field_name;
  an_accessibility          accessibility;
  a_cpp_cli_import_flag_set import_flags;

  import_flags = import_scope_.containing_assembly().import_flags();
  hr = import_interface()->GetFieldProps(field_token, /*pClass=*/nullptr,
                                         field_name, &field_attributes,
                                         &signature, &bytes_in_signature,
                                         &constant_type, &constant_value,
                                         &characters_in_constant);
  CHECK_API_RESULT(hr, GetFieldProps);
  /* Get the accessibility.  Skip those fields that are not accessible. */
  accessibility = an_accessibility(import_scope_, field_token,
                                   field_attributes, this);
  skip_member = !accessibility.is_accessible();
  if (!skip_member && IsFdSpecialName(field_attributes)) {
    skip_member = true;
  }  /* if  */
  if (!skip_member) {
    /* Decode the signature and create the appropriate declaration.  This
       can either be a method or a field. */
    auto field_type = a_signature_decoder::decode_field(*this,
                                                        signature,
                                                        bytes_in_signature);
    if (field_type) {
      escape_invalid_identifier(field_name);
      buffer << accessibility.get_string() << ": ";
      /* Emit any custom attributes for the field. */
      write_custom_attributes(buffer, field_token);
      if ((import_flags & cpp_cli_declspec_member_info) != 0) {
        buffer << "__declspec(member_info(";
        buffer << "0x" << setw(8) << setfill('0') << hex << field_token;
        buffer << ")) ";
      }  /* if */
      if (IsFdInitOnly(field_attributes)) {
        buffer << "initonly ";
      } else if (IsFdLiteral(field_attributes)) {
        buffer << "literal ";
      }  /* if */
      /* Emit the storage class.  "literal" implies "static". */
      if (IsFdStatic(field_attributes) && !IsFdLiteral(field_attributes)) {
        buffer << "static ";
      }  /* if */
      buffer << field_type->get_string(field_name);
      if (IsFdHasDefault(field_attributes)) {
        a_constant_value constant(constant_type, constant_value,
                                  characters_in_constant);
        buffer << " = " << constant.get_source_code(field_type);
      }  /* if */
      buffer << ';' << END_OF_LINE;
    }  /* if */
  }  /* if */
}  /* a_type_definition::import_one_field */


void a_type_definition::import_all_fields(ostringstream &buffer) const
/*
Import all the fields associated with a single type.
*/
{
  HCORENUM    enum_fields = NULL;
  mdFieldDef  fields[64];
  ULONG       count_of_fields;
  HRESULT     hr;

  do {
    hr = import_interface()->EnumFields(&enum_fields, token_,
                                        fields, _countof(fields),
                                        &count_of_fields);
    CHECK_API_RESULT(hr, EnumFields);
    for (ULONG i = 0; i < count_of_fields; ++i) {
      import_one_field(buffer, fields[i]);
    }  /* for */
  } while (count_of_fields > 0);
  import_interface()->CloseEnum(enum_fields);
}  /* a_type_definition::import_all_fields */


void a_type_definition::import_one_property(
                                           ostringstream &buffer,
                                           mdProperty    property_token) const
/*
Import the definition of single property - this includes both the property
itself and its associated accessor methods.
*/
{
  HRESULT                       hr;
  ULONG                         bytes_in_signature;
  DWORD                         property_attributes;
  PCCOR_SIGNATURE               signature;
  mdMethodDef                   set_method_token;
  mdMethodDef                   get_method_token;
  wstring                       property_name;
  a_const_method_definition_ptr set_method;
  a_const_method_definition_ptr get_method;
  an_accessibility              accessibility;

  hr = import_interface()->GetPropertyProps(property_token,
                                            /*pClass=*/nullptr,
                                            property_name,
                                            &property_attributes,
                                            &signature, &bytes_in_signature,
                                            /*pdwCPlusTypeFlag=*/nullptr,
                                            /*ppDefaultValue=*/nullptr,
                                            /*pcchDefaultValue=*/nullptr,
                                            &set_method_token,
                                            &get_method_token,
                                            /*rmdOtherMethod=*/nullptr,
                                            /*cMax=*/0,
                                            /*pcOtherMethod =*/nullptr);
  CHECK_API_RESULT(hr, GetPropertyProps);
  /* If there is a set method and/or a get method then import the necessary
     information about the method.  The accessibility of the property itself
     is the wider accessibility of its associated accessor methods.*/
  if (!IsNilToken(set_method_token)) {
    set_method = import_scope_.get_method_definition(set_method_token);
    accessibility = set_method->accessibility();
  }  /* if */
  if (!IsNilToken(get_method_token)) {
    get_method = import_scope_.get_method_definition(get_method_token);
    if (set_method != nullptr) {
      accessibility = an_accessibility::wider_accessibility(
                                                 accessibility,
                                                 get_method->accessibility());
    } else {
      accessibility = get_method->accessibility();
    }  /* if */
  }  /* if */
  if (accessibility.is_accessible()) {
    /* Now that we have everything we need emit the definition of the
       property. */
    DWORD               method_attributes_mask = mdStatic;
    DWORD               method_attributes = 0;
    if (get_method != nullptr) {
      method_attributes = get_method->attributes() & method_attributes_mask;
      if (set_method != nullptr) {
        check_assertion(method_attributes ==
                        (set_method->attributes() & method_attributes_mask));
      }  /* if */
    } else if (set_method != nullptr) {
      method_attributes = set_method->attributes() & method_attributes_mask;
    }  /* if */
    auto type = a_signature_decoder::decode_method(*this,
                                                   property_token,
                                                   signature,
                                                   bytes_in_signature);
    if (!type->is_invalid()) {
      buffer << accessibility.get_string() << ": ";
      /* Emit any custom attributes for the property. */
      write_custom_attributes(buffer, property_token);
      /* Emit any decl specifiers. */
      write_method_decl_specifiers(buffer, property_token, method_attributes);
      /* Modify the name of this property if it is the default-indexed
         property. */
      bool is_default_indexed_property = false;
      if (!type->parameter_list().empty()) {
        /* The property is an indexed property, determine if it is the
           default-indexed property. */
        auto &attributes = import_scope_.get_custom_attributes(token_);
        auto default_member_attribute = attributes.default_member_attribute();
        if (default_member_attribute != nullptr &&
            property_name == default_member_attribute->member_name()) {
          /* The property matches the default member name of this class. */
          is_default_indexed_property = true;
        }  /* if */
      }  /* if */
      if (is_default_indexed_property) {
        property_name = L"default";
      } else {
        escape_invalid_identifier(property_name);
      }  /* */
      buffer << "property ";
      if (type->parameter_list().empty()) {
        /* The property is not an indexed property, so emit it as a field. */
        buffer << type->return_type()->get_string(property_name);
      } else {
        buffer << type->get_string(property_name);
      }  /* if */
      buffer << " {" << END_OF_LINE;
      /* Import the get and/or set method. */
      if (get_method != nullptr) {
        import_one_method(buffer, *get_method);
      }  /* if */
      if (set_method != nullptr) {
        import_one_method(buffer, *set_method);
      }  /* if */
      buffer << '}' << END_OF_LINE;
    }  /* if */
  }  /* if */
}  /* a_type_definition::import_one_property */


void a_type_definition::import_properties(ostringstream &buffer) const
/*
Import all the properties associated with this type.
*/
{
  HCORENUM   enum_properties = nullptr;
  mdProperty properties[16];
  ULONG      count_of_properties;
  HRESULT    hr;

  do {
    hr = import_interface()->EnumProperties(&enum_properties,
                                            token_,
                                            properties,
                                            _countof(properties),
                                            &count_of_properties);
    CHECK_API_RESULT(hr, EnumProperties);
    for (ULONG i = 0; i < count_of_properties; ++i) {
      import_one_property(buffer, properties[i]);
    }  /* for */
  } while (count_of_properties > 0);
  import_interface()->CloseEnum(enum_properties);
}  /* a_type_definition::import_properties */


void a_type_definition::import_one_event(ostringstream  &buffer,
                                         mdEvent        event_token) const
/*
Import the definition of single event - this includes both the event
itself and its associated methods.
*/
{
  HRESULT                       hr;
  wstring                       event_name;
  DWORD                         event_attributes;
  mdToken                       event_type_token;
  mdMethodDef                   add_method_token, remove_method_token,
                                raise_method_token;
  a_const_method_definition_ptr add_method, remove_method, raise_method;
  an_accessibility              accessibility;
  DWORD                         method_attributes;

  hr = import_interface()->GetEventProps(event_token, /*pClass=*/nullptr,
                                         event_name, &event_attributes,
                                         &event_type_token, &add_method_token,
                                         &remove_method_token,
                                         &raise_method_token,
                                         /*rmdOtherMethod=*/nullptr,
                                         /*cMax=*/0,
                                         /*pcOtherMethod=*/nullptr);
  CHECK_API_RESULT(hr, GetEventProps);
  check_assertion(!IsNilToken(add_method_token) &&
                  !IsNilToken(remove_method_token));
  /* If there is an add/remove/raise method then import the necessary
     information about the method. */
  add_method = import_scope_.get_method_definition(add_method_token);
  remove_method = import_scope_.get_method_definition(remove_method_token);
  if (!IsNilToken(raise_method_token)) {
    raise_method = import_scope_.get_method_definition(raise_method_token);
  }  /* if */
  /* Set the accessibility of the event itself.  While is it expected that the
     add and remove methods have the same accessibility, if that isn't the
     case, we'll use the wider accessibility of the two. */
  check_assertion(add_method->accessibility() ==
                                              remove_method->accessibility());
  accessibility = an_accessibility::wider_accessibility(
                                              add_method->accessibility(),
                                              remove_method->accessibility());
  DWORD method_attributes_mask = mdStatic | mdVirtual;
  check_assertion((add_method->attributes() & method_attributes_mask) ==
                     (remove_method->attributes() & method_attributes_mask) &&
                  (IsNilToken(raise_method_token) ||
                   (add_method->attributes() & method_attributes_mask) ==
                      (raise_method->attributes() & method_attributes_mask)));
  method_attributes = add_method->attributes();
  if (accessibility.is_accessible()) {
    /* Now that we have everything we need emit the definition of the
       event. */
    auto event_type = type_from_token(event_type_token);
    if (event_type != nullptr) {
      escape_invalid_identifier(event_name);
      buffer << accessibility.get_string() << ": ";
      if (IsMdStatic(method_attributes)) {
        check_assertion(!IsMdVirtual(method_attributes));
        buffer << "static ";
      } else if (IsMdVirtual(method_attributes)) {
        buffer << "virtual ";
      }  /* if */
      buffer << "event ";
      buffer << event_type->get_string() << "^ " << event_name;
      buffer << " {" << END_OF_LINE;
      import_one_method(buffer, *add_method);
      import_one_method(buffer, *remove_method);
      if (raise_method != nullptr) {
        import_one_method(buffer, *raise_method);
      }  /* if */
      buffer << '}' << END_OF_LINE;
    }  /* if */
  }  /* if */
}  /* a_type_definition::import_one_event */


void a_type_definition::import_events(ostringstream &buffer) const
/*
Import all the events associated with this type.
*/
{
  HCORENUM enum_events = nullptr;
  mdEvent  events[8];
  ULONG    count_of_events;
  HRESULT  hr;

  do {
    hr = import_interface()->EnumEvents(&enum_events, token_,
                                        events, _countof(events),
                                        &count_of_events);
    CHECK_API_RESULT(hr, EnumEvents);
    for (ULONG i = 0; i < count_of_events; ++i) {
      import_one_event(buffer, events[i]);
    }  /* for */
  } while (count_of_events > 0);
  import_interface()->CloseEnum(enum_events);
}  /* a_type_definition::import_events */


void a_type_definition::import_nested_classes(ostringstream &buffer) const
/*
Import all the nested classes enclosed by this type.
*/
{
  HCORENUM                       enum_typedefs = nullptr;
  mdTypeDef                      typedefs[64];
  ULONG                          count_of_typedefs;
  a_pending_constraint_type_list pending_constraint_types;

  do {
    HRESULT hr = import_interface()->EnumTypeDefs(&enum_typedefs, typedefs,
                                                  _countof(typedefs),
                                                  &count_of_typedefs);
    CHECK_API_RESULT(hr, EnumTypeDefs);
    for (ULONG i = 0; i < count_of_typedefs; ++i) {
      mdTypeDef enclosing_typedef;
      hr = import_interface()->GetNestedClassProps(typedefs[i],
                                                   &enclosing_typedef);
      if (SUCCEEDED(hr) && enclosing_typedef == token_) {
        import_scope().import_one_type(buffer,
                                       typedefs[i],
                                       /*at_top_level=*/false,
                                       /*want_definition=*/false,
                                       /*class_body_only=*/false,
                                       &pending_constraint_types,
                                       /*is_delegate=*/nullptr);
      }  /* if */
    }  /* for */
  } while (count_of_typedefs > 0);
  import_interface()->CloseEnum(enum_typedefs);
  /* Now that all nested types have been imported, re-declare all nested
     generic types that were declared with a pending constraint clause, this
     time with the complete constraint clause. */
  for (auto &pending_constraint_type : pending_constraint_types) {
    import_scope().import_one_type(buffer,
                                   pending_constraint_type,
                                   /*at_top_level=*/false,
                                   /*want_definition=*/false,
                                   /*class_body_only=*/false,
                                   /*pending_constraint_types=*/nullptr,
                                   /*is_delegate=*/nullptr);
  }  /* for */
}  /* a_type_definition::import_nested_classes */


void a_type_definition::import_definition(ostringstream &buffer) const
/*
Create the definition for the current type.
*/
{
  if (IsTdAbstract(attributes_)) {
    buffer << " abstract";
  }  /* if */
  if (IsTdSealed(attributes_)) {
    buffer << " sealed";
  }  /* if */
  string pending_interface_list = process_base_class_list(buffer);
  buffer << " {" << END_OF_LINE;
  import_nested_classes(buffer);
  if (!pending_interface_list.empty())
  {
    buffer << "__implements "
           << pending_interface_list << ";" << END_OF_LINE;
  }  /* if */
  import_all_fields(buffer);
  import_properties(buffer);
  import_events(buffer);
  import_all_methods(buffer);
  buffer << "};";
#if DEBUG
  buffer << "  /* " << qualified_name().as_string() << " */";
#endif /* DEBUG */
  buffer << END_OF_LINE;
}  /* a_type_definition::import_definition */


void an_import_scope::import_one_type(
               ostringstream                  &buffer,
               mdTypeDef                      typedef_token,
               bool                           at_top_level,
               bool                           want_definition,
               bool                           class_body_only,
               a_pending_constraint_type_list *pending_constraint_types,
               a_boolean                      *is_delegate) const
/*
Import a single type from an import scope and create either a declaration or
a definition for the type depending on want_definition.  Note, in some cases
(like enumerations) we always need to create a definition.  at_top_level
denotes  whether this declaration/definition is at file scope.  If so,
namespace scopes and top-level-visibility will be emitted and nested classes
will be suppressed.

If class_body_only is true, the class head and the namespace scopes will be
omitted.

If is_delegate is non-NULL, *is_delegate is set to TRUE if the class is a
delegate and to FALSE otherwise.
*/
{
  is_cppcx_metadata = containing_assembly_->is_cppcx_metadata();
  auto import_flags = containing_assembly_->import_flags();
  bool define_all_types = (import_flags & cpp_cli_define_all_types) != 0;
  bool import_as_friend = (import_flags & cpp_cli_as_friend_assembly) != 0;
  auto &type_definition = get_type_definition(typedef_token);
  bool is_nested = type_definition.is_nested();
  if (is_delegate) *is_delegate = FALSE; /* Assume. */

  if (is_nested && at_top_level && !class_body_only) {
    /* Do not emit nested types at top level scopes.  Nested types are
       emitted in their enclosing type. */
  } else if (!type_definition.accessibility().is_accessible()) {
    /* Do not emit inaccessible types. */
  } else {
    auto kind = type_definition.kind();
    bool is_generic = type_definition.generic_parameter_count() > 0;
    auto full_type_name = type_definition.qualified_name();
    full_type_name.strip_generic_arguments();
    if (full_type_name == L"_GUID") {
      /* _GUID is a built-in type in Microsoft mode.  Skip it. */
      goto done;
    }  /* if */
    if (kind == a_type_definition::tdk_enum_class) {
      /* Ensure any custom attributes and assembly level visibility are
         emitted for enum classes. */
      want_definition = true;
    } else if (kind == a_type_definition::tdk_delegate &&
        !want_definition && !define_all_types) {
      /* If no definition is required, treat the delegate as a ref class
         since "delegate ..." is always a definition.  Doing so avoids
         declaration ordering problems. */
      kind = a_type_definition::tdk_ref_class;
    }  /* if */
    /* Emit the namespace scopes and class head if required.  These are
       present on the original declaration and also on the definitions of
       generics. */
    if (!class_body_only || is_generic) {
      if (at_top_level && !class_body_only) {
        /* Make sure that the correct namespace scopes are opened. */
        set_namespace_scope(buffer, full_type_name.namespace_name());
      }  /* if */
      if (is_nested && !at_top_level) {
        /* Emit the access specifier for nested types. */
        buffer << type_definition.accessibility().get_string() << ": ";
      }  /* if */
      /* Emit the generic header if this is directly or indirectly a generic
         type.  When obtaining the body of such a type, the generated code
         should be in the form of an out-of-class definition. */
      if (is_generic) {
        a_boolean out_of_class_definition = class_body_only;
        a_boolean use_pending_constraint_clause = false;
        if (pending_constraint_types != nullptr &&
            type_definition.has_type_constraints()) {
          use_pending_constraint_clause = true;
          pending_constraint_types->push_back(typedef_token);
        }  /* if */
        buffer << type_definition.generic_header(
                                               out_of_class_definition,
                                               /*omit_constraints=*/false,
                                               use_pending_constraint_clause);
      }  /* if */
      if (want_definition) {
        /* Emit any custom attributes for the type. */
        if (!is_nested && !define_all_types &&
            kind == a_type_definition::tdk_enum_class) {
          /* Importing custom attributes on namespace-scoped enum types would
             currently cause declaration ordering problems.  This can be
             solved if we first import enums as forward declarations and
             import their definitions on-demand, as is done for other types. */
        } else {
          type_definition.write_custom_attributes(buffer, typedef_token);
        }  /* if */
        if (!is_nested) {
          /* Emit the assembly level visibility - either public or private. */
          buffer << type_definition.accessibility().get_string() << ' ';
        }  /* if */
      }  /* if */
      /* Emit the tokens that represent the kind. */
      buffer << a_type_definition::string_from_kind(kind) << ' ';
      /* Emit the assembly_info declspec. */
      if (!class_body_only &&
          (import_flags & cpp_cli_declspec_assembly_info) != 0) {
        buffer << "__declspec(assembly_info(0x";
        buffer << setw(8) << setfill('0') << hex << assembly_scope_index();
        buffer << ", 0x" << setw(8) << setfill('0') << hex << typedef_token;
        buffer << ")) ";
      }  /* if */
    } else {
      /* Emit any custom attributes for the type. */
      if (want_definition) {
        type_definition.write_custom_attributes(buffer, typedef_token);
      }  /* if */
      if (kind == a_type_definition::tdk_delegate) {
        /* Even when class_body_only is TRUE, the context-sensitive keyword
           "delegate" is needed so that a delegate class definition can be
           easily distinguished from a more traditional (managed) class
           definition.  This is not needed in the case of a generic delegate
           since its definition is a complete declaration (including the
           generic<...> header and the keyword "delegate"). */
        buffer << "delegate ";
        if (is_delegate) *is_delegate = TRUE;
      }  /* if  */
    }  /* if */
    if (kind == a_type_definition::tdk_delegate) {
      type_definition.import_delegate_definition(buffer);
    } else if (kind == a_type_definition::tdk_enum_class ||
               kind == a_type_definition::tdk_native_enum) {
      type_definition.import_enum_definition(buffer);
    } else {
      /* Emit the name of the type. */
      if (!class_body_only || is_generic) {
        /* Emit the enclosing type name qualifiers for definitions of nested
           generic types. */
        if (class_body_only && is_nested) {
          buffer << full_type_name.as_string();
        } else {
          buffer << type_definition.type_name();
        }  /* if */
      }  /* if */
      if (want_definition || define_all_types) {
        type_definition.import_definition(buffer);
      } else {
        buffer << ';' << END_OF_LINE;
      }  /* if */
      if (at_top_level && want_definition && !class_body_only) {
        close_all_namespace_scopes(buffer);
      }  /* if */
    }  /* if */
  }  /* if */
done:
  return;
}  /* an_import_scope::import_one_type */


mdToken an_import_scope::get_associated_event_or_property(
                                               mdMethodDef method_token) const
/*
Return the mdProperty or mdEvent to which this method is associated, or
mdTokenNil if the method is not associated with an event or property.
*/
{
  HRESULT            hr;
  HCORENUM           enum_method_semantics = NULL;
  static const ULONG max_tokens = 2;
  mdToken            tokens[max_tokens];
  ULONG              count_tokens = _countof(tokens);
  mdToken            token = mdTokenNil;

  check_assertion(TypeFromToken(method_token) == mdtMethodDef);
  hr = import_interface_->EnumMethodSemantics(&enum_method_semantics,
                                              method_token,
                                              tokens,
                                              max_tokens,
                                              &count_tokens);
  CHECK_API_RESULT(hr, EnumMethodSemantics);
  import_interface_->CloseEnum(enum_method_semantics);
  if (count_tokens == 1) {
    token = tokens[0];
  } else if (count_tokens > 0) {
    /* A specific member should only map to a single event or property. */
    unexpected_condition();
  }  /* if */
  return token;
}  /* an_import_scope::get_associated_event_or_property */


a_cli_operator_kind an_import_scope::rename_cli_operator(
                                              wstring &method_name,
                                              DWORD   method_attributes) const
/*
Rename any CLI operators to their corresponding C++/CLI operator name and
return the CLI operator kind of the operator, or cok_none if it is not a CLI
operator.  User-defined conversion operators (cok_implicit and cok_explicit)
require additional processing, as their names contain the method's return
type.
*/
{
  a_cli_operator_kind cok = cok_none;

  if ((IsMdSpecialName(method_attributes) || is_cppcx_metadata) &&
      wcsncmp(method_name.c_str(), L"op_", sizeof("op_")-1) == 0) {
    /* This might be a CLI operator that needs to be converted to a C++
       operator. */
    string utf8_method_name = conv_wide_to_utf8(
                                   const_cast<wchar_t*>(method_name.c_str()));
    if (method_name.length() != utf8_method_name.length()) {
      /* The method name contains non-ASCII characters.  Because all of the
         CLI operator names are comprised only of ASCII characters, don't
         bother calling find_cli_operator_kind. */
      cok = cok_none;
    } else {
      cok = find_cli_operator_kind(
                                 const_cast<char*>(utf8_method_name.c_str()));
    }  /* if */
    switch (cok) {
      case cok_none:
        /* Not a CLI operator. */
        break;
      case cok_implicit:
      case cok_explicit:
        /* Implicit or explicit user-defined conversion operator. */
        method_name.clear();
        break;
      default:
        { a_cli_operator_info_ptr info = cli_operator_info_from_kind(cok);
          if (IsMdStatic(method_attributes) &&
              info->is_assignment_operator) {
            /* This is a static CLI assignment operator.  Import it using
               the CLI operator name.  We can't consume assignment operators
               with CLR semantics, like static R^ op_Assign(R^, R^). */
          } else if (info->cpp_name == NULL) {
            /* This is a CLI operator for which there is no C++ mapping.
               Import it using the CLI operator name. */
          } else {
            /* This is a CLI operator for which there is a C++ mapping.
               Import it using the C++ operator name. */
            locale   loc;
            wchar_t  cpp_name[100];
            size_t   len = strlen(info->cpp_name);
            check_assertion(len < sizeof(cpp_name)/sizeof(cpp_name[0]));
            use_facet< ctype<wchar_t> >(loc).widen(
                              info->cpp_name, info->cpp_name+len+1, cpp_name);
            method_name = cpp_name;
          }  /* if */
          break;
        }
    }  /* switch */
  }  /* if */
  return cok;
}  /* an_import_scope::rename_cli_operator */


wstring an_import_scope::get_overridden_name(
                   const a_signature_decoder_scope &scope,
                   mdToken                         method_token,
                   a_const_class_type_wrapper_ptr  enclosing_class_type) const
/*
Get the name for the specified member for use in the overridden name list of
an override specifier.
*/
{
  wstring                        overridden_name;
  HRESULT                        hr;
  wstring                        method_name;

  switch (TypeFromToken(method_token)) {
    case mdtMethodDef:
      { auto method_definition = get_method_definition(method_token);
        if (!method_definition->accessibility().is_accessible()) {
          /* The type containing the method is inaccessible.  This is possible
             because accessible types are permitted to implement inaccessible
             interfaces.  Therefore, skip any named overrides of members of
             such interfaces, as they are skipped by process_interfaces. */
          goto done;
        }  /* if */
        method_name = method_definition->name();
        /* Determine if this member is an event or property method.  Note
           that, although CLS Rules 24 and 29 in ECMA-335 indicate that
           methods that implement a property or event shall be marked
           SpecialName in the metadata, this has been shown to not always be
           the case; hence no IsMdSpecialName check. */
        if (method_definition->is_event_or_property_accessor()) {
          /* Get the name of the event or property. */
          mdToken event_or_property_token = method_definition->
                                                    event_or_property_token();
          wstring event_or_property_name;
          bool    is_default_indexed_property = false;
          switch (TypeFromToken(event_or_property_token)) {
            case mdtEvent:
              hr = import_interface_->GetEventProps(
                                                   event_or_property_token,
                                                   /*pClass=*/nullptr,
                                                   event_or_property_name,
                                                   /*dwEventFlags=*/nullptr,
                                                   /*tkEventType=*/nullptr,
                                                   /*mdAddOn=*/nullptr,
                                                   /*mdRemoveOn=*/nullptr,
                                                   /*mdFire=*/nullptr,
                                                   /*rmdOtherMethod=*/nullptr,
                                                   /*cMax=*/0,
                                                   /*pcOtherMethod=*/nullptr);
              CHECK_API_RESULT(hr, GetEventProps);
              break;
            case mdtProperty:
              { mdTypeDef       property_class_token;
                PCCOR_SIGNATURE property_signature;
                ULONG           bytes_in_property_signature;
                hr = import_interface_->GetPropertyProps(
                                                event_or_property_token,
                                                &property_class_token,
                                                event_or_property_name,
                                                /*pdwPropFlags=*/nullptr,
                                                &property_signature,
                                                &bytes_in_property_signature,
                                                /*pdwCPlusTypeFlag=*/nullptr,
                                                /*ppDefaultValue=*/nullptr,
                                                /*pcchDefaultValue=*/nullptr,
                                                /*pmdSetter=*/nullptr,
                                                /*pmdGetter*/nullptr,
                                                /*rmdOtherMethod=*/nullptr,
                                                /*cMax=*/0,
                                                /*pcOtherMethod =*/nullptr);
                CHECK_API_RESULT(hr, GetPropertyProps);
                /* Get the property signature to determine if it is an indexed
                   property. */
                auto type = a_signature_decoder::decode_method(
                                         *method_definition->enclosing_type(),
                                         event_or_property_token,
                                         property_signature,
                                         bytes_in_property_signature);
                if (!type->is_invalid() && !type->parameter_list().empty()) {
                  /* The property is an indexed property, determine if it is
                     the default-indexed property. */
                  auto &attributes = get_custom_attributes(
                                                        property_class_token);
                  auto default_member_attribute =
                                        attributes.default_member_attribute();
                  if (default_member_attribute != nullptr && 
                      event_or_property_name ==
                                    default_member_attribute->member_name()) {
                    /* The property matches the default member name of the
                        class of which it is a member. */
                    is_default_indexed_property = true;
                  }  /* if */
                }  /* if */
                break;
              }
            default:
              unexpected_condition();
              break;
          }  /* switch */
          check_assertion(!event_or_property_name.empty());
          /* Modify the name of this property if it is the default-indexed
             property. */
          if (is_default_indexed_property) {
            event_or_property_name = L"default";
          } else {
            escape_invalid_identifier(event_or_property_name);
          }  /* if */
          /* Determine which kind of event or property method it is. */
          DWORD method_semantics;
          hr = import_interface_->GetMethodSemantics(method_token,
                                                     event_or_property_token,
                                                     &method_semantics);
          CHECK_API_RESULT(hr, GetMethodSemantics);
          method_name = event_or_property_name + L"::" +
                                name_from_method_semantics(method_semantics);
        } else {
          /* The overridden name of a CLI operator is the corresponding
             C++/CLI operator name. */
          a_cli_operator_kind cok = rename_cli_operator(
                                             method_name,
                                             method_definition->attributes());
          if (cok == cok_implicit || cok == cok_explicit) {
            /* Obtain the method's return type to handle user-defined
               conversion operators. */
            auto function_type = method_definition->function_type();
            if (function_type->is_invalid()) {
              /* The function was not successfully decoded.  Return an empty
                 name, which will result in the method not being added to the
                 named override list. */
              goto done;
            } else {
              auto return_type = function_type->return_type();
              if (return_type->is_invalid()) {
                /* The return type was not successfully decoded.  Return an
                   empty name, which will result in the method not being added
                   to the named override list. */
                goto done;
              } else {
                method_name = L"operator " + return_type->get_string();
              }  /* if */
            }  /* if */
          }  /* if */
        }  /* if */
        if (enclosing_class_type == nullptr) {
          enclosing_class_type = method_definition->enclosing_type()
                                                  ->class_type();
        }  /* if */
        overridden_name = enclosing_class_type->name().as_string() + L"::" +
                                                                  method_name;
      }
      break;
    case mdtMemberRef:
      {
        mdToken         parent_token;
        PCCOR_SIGNATURE signature;
        ULONG           bytes_in_signature;
        hr = import_interface_->GetMemberRefProps(method_token, &parent_token,
                                                  method_name,
                                                  &signature,
                                                  &bytes_in_signature);
        CHECK_API_RESULT(hr, GetMemberRefProps);
        /* Try to find the definition of the type that contains this method. */
        enclosing_class_type = scope.type_from_token(parent_token);
        if (!enclosing_class_type->is_unresolved_type()) {
          /* Obtain the method's type from its signature. */
          a_function_type_wrapper_ptr function_type;
          if (enclosing_class_type->generic_instance_scope() != nullptr) {
            /* The enclosing type is a generic instance.  Therefore, any
               generic type arguments in the method signature refer to the
               generic arguments from that generic instance.  However, any
               other types referenced in the signature are decoded in the
               context in which the MemberRef occurred. */
            a_generic_instance_scope generic_instance_scope(
                              scope.import_scope(),
                              enclosing_class_type->generic_instance_scope()
                                                  ->generic_type_arguments(),
                              scope.generic_method_arguments());
            function_type = a_signature_decoder::decode_method(
                                                       generic_instance_scope,
                                                       method_token,
                                                       signature,
                                                       bytes_in_signature);
          } else {
            /* The enclosing type is not a generic instance.  Any types
               referenced in the signature are decoded in the context in which
               the MemberRef occurred. */
            function_type = a_signature_decoder::decode_method(
                                                          scope,
                                                          method_token,
                                                          signature,
                                                          bytes_in_signature);
          }  /* if */
          if (function_type->is_invalid()) {
            /* The function was not successfully decoded.  Return an empty
               name, which will result in the method not being added to the
               named override list. */
            goto done;
          }  /* if */
          a_const_method_definition_ptr ref_method;
          HCORENUM    enum_methods = nullptr;
          mdMethodDef methods[16];
          ULONG       count_of_methods;
          auto        &import_scope = *enclosing_class_type->import_scope();
          auto        import_interface = import_scope.import_interface();
          auto        &enclosing_type = import_scope.get_type_definition(
                                               enclosing_class_type->token());
          if (!enclosing_type.accessibility().is_accessible()) {
            /* The type containing the method is inaccessible.  This is
               possible because accessible types are permitted to implement
               inaccessible interfaces.  Therefore, skip any named overrides
               of members of such interfaces, as they are skipped by
               process_interfaces. */
            goto done;
          }  /* if */
          /* Find the method in the enclosing class to which this MemberRef is
             referring. */
          do {
            hr = import_interface->EnumMethodsWithName(&enum_methods,
                                                       enclosing_type.token(),
                                                       method_name.c_str(),
                                                       methods,
                                                       _countof(methods),
                                                       &count_of_methods);
            CHECK_API_RESULT(hr, EnumMethodsWithName);
            for (ULONG index = 0; index < count_of_methods; ++index) {
              auto method_definition = import_scope.get_method_definition(
                                                              methods[index]);
              /* Skip non-virtual methods, as those can't be overridden with a
                 named override.  Also skip inaccessible methods, as they are
                 not imported. */
              if (IsMdVirtual(method_definition->attributes()) &&
                  method_definition->accessibility().is_accessible()) {
                /* Find a method in the class with the same generic arity and
                   number of parameters. */
                auto other_function_type = method_definition->function_type();
                if (!other_function_type->is_invalid() &&
                    function_type->generic_arity() ==
                                       other_function_type->generic_arity() &&
                    function_type->parameter_list().size() ==
                               other_function_type->parameter_list().size()) {
                  ref_method = move(method_definition);
                  break;
                }  /* if */
              }  /* if */
            }  /* for */
          } while (count_of_methods > 0);
          import_interface->CloseEnum(enum_methods);
          if (ref_method != nullptr) {
            /* Ideally, ref_method should be a unique method at this point.
               However, the code above is only checking for methods with the
               same name, arity, and number of parameters rather than
               performing full overload resolution.  Since we are only
               interested in the C++/CLI name of the method and not its
               parameters, this is sufficient if all of the methods are all
               "regular" virtual member functions or all the same kind of
               virtual property or event accessor function, because this call
               to get_overridden_name would return the same name anyway.
               However, it is technically possible for a virtual member
               function and a virtual property or event accessor method to have
               the same name, arity, and number of parameters.  This call to
               get_overridden_name would potentially return the wrong method
               name in that highly unlikely situation. */
            overridden_name = import_scope.get_overridden_name(
                                                         *ref_method,
                                                         ref_method->token(),
                                                         enclosing_class_type);
          } else {
            /* ref_method could be null if all candidates were inaccessible. */
            goto done;
          }  /* if */
        } else {
          /* The assembly containing this method has not been imported.
             Return an empty name, which will result in the method not being
             added to the named override list. */
          goto done;
        }  /* if */
        break;
      }  /* mdtMemberRef */
    default:
      unexpected_condition();
      break;
  }  /* switch */
done:
  return overridden_name;
}  /* an_import_scope::get_overridden_name */


a_method_parameter::a_method_parameter(const an_import_scope &import_scope,
                                       a_type_wrapper_ptr    type,
                                       mdParamDef            token)
  : token_(token)
  , type_(move(type))
  , attributes_(0)
{
  HRESULT hr;
  auto    import_interface = import_scope.import_interface();

  if (!IsNilToken(token_)) {
    hr = import_interface->GetParamProps(token_,
                                         /*method_token=*/nullptr,
                                         /*param_index=*/nullptr,
                                         name_,
                                         &attributes_,
                                         /*constant_type=*/nullptr,
                                         /*constant_value=*/nullptr,
                                         /*characters_in_constant=*/nullptr);
    CHECK_API_RESULT(hr, GetParamProps);
    escape_invalid_identifier(name_);
    /* Adjust the type of C++/CX array parameters to account for [in] or
       [out] attributes. */
    if (is_cppcx_metadata && (IsPdIn(attributes_) || IsPdOut(attributes_))) {
      auto array_type = type_->as_handle_to_array();
      if (array_type != nullptr) {
        if (IsPdOut(attributes_)) {
          /* Change the type to "Platform::WriteOnlyArray<T>". */
          array_type->set_array_kind(
                                  an_array_type_wrapper::ak_write_only_array);
          check_assertion(!IsPdIn(attributes_));
          attributes_ &= ~(pdIn | pdOut);
        } else if IsPdIn(attributes_) {
          /* Change the type to "const Platform::Array<T>^". */
          array_type->add_qualifier_flags(a_type_wrapper::qf_const);
          attributes_ &= ~pdIn;
        }  /* if */
      }  /* if */
    }  /* if */
  }  /* if */
}  /* a_method_parameter::a_method_parameter */


a_boolean a_method_parameter::is_parameter_array(
                                    const an_import_scope &import_scope) const
/*
Return TRUE if the method parameter is a parameter array.
*/
{
  HRESULT hr = S_FALSE;
  if (!IsNilToken(token_)) {
    hr = import_scope.import_interface()->GetCustomAttributeByName(
                                                token_,
                                                L"System.ParamArrayAttribute",
                                                /*ppData=*/nullptr,
                                                /*pcbData=*/nullptr);
    CHECK_API_RESULT(hr, GetCustomAttributeByName);
  }  /* if */
  return hr == S_OK;
}  /* a_method_parameter::is_parameter_array */


a_generic_instance_scope_ptr a_signature_decoder::decode_generic_arguments()
/*
Decode the generic arguments associated with a type.  Note, it is the caller's
responsibility to ensure that there is at least one generic argument.
*/
{
  a_generic_argument_list generic_arguments;
  BYTE count_of_generic_arguments = read_one_byte();

  check_assertion(count_of_generic_arguments > 0);
  generic_arguments.reserve(count_of_generic_arguments);
  for (BYTE i = 0; i < count_of_generic_arguments; ++i) {
    generic_arguments.push_back(decode_type());
  }  /* for */
  return make_shared<a_generic_instance_scope>(scope_.import_scope(),
                                               move(generic_arguments));
}  /* a_signature_decoder::decode_generic_arguments */


typedef unsigned int a_type_modifier_flag_set;
enum a_type_modifier_flag : a_type_modifier_flag_set
{
  tmf_none                       = 0x0000,
  tmf_compiler_marshal_override  = 0x0001,
  tmf_is_boxed                   = 0x0002,
  tmf_is_by_value                = 0x0004,
  tmf_is_const                   = 0x0008,
  tmf_is_copy_ctor               = 0x0010,
  tmf_is_cxx_reference           = 0x0020,
  tmf_is_cxx_udt_return          = 0x0040,
  tmf_is_explicitly_dereferenced = 0x0080,
  tmf_is_implicitly_dereferenced = 0x0100,
  tmf_is_long                    = 0x0200,
  tmf_is_rvalue_reference        = 0x0400,
  tmf_is_signed                  = 0x0800,
  tmf_is_sign_unspecified_byte   = 0x1000,
  tmf_is_volatile                = 0x2000,
  tmf_unknown                    = 0x4000,
  tmf_is_copy_constructed        = 0x8000,
};


static a_type_modifier_flag type_name_to_modifier_flag(const wstring &name)
/*
Return the type modifier flag that corresponds to the specified class name, or
tmf_unknown if no such mapping exists.
*/
{
  static const struct a_type_name_to_modifier_flag_map
  {
    LPCWSTR              name;
    a_type_modifier_flag modifier_flag;
  } type_name_to_modifier_flag_map[] = {
    { L"Microsoft::VisualC::IsMarshalWorkaround",
                                              tmf_compiler_marshal_override },
    { L"System::Runtime::CompilerServices::CompilerMarshalOverride",
                                              tmf_compiler_marshal_override },
    { L"Platform::Runtime::CompilerServices::CompilerMarshalOverride",
                                              tmf_compiler_marshal_override },
    { L"Microsoft::VisualC::IsBoxedModifier", tmf_is_boxed },
    { L"System::Runtime::CompilerServices::IsBoxed", tmf_is_boxed },
    { L"Platform::Runtime::CompilerServices::IsBoxed", tmf_is_boxed },
    { L"Microsoft::VisualC::IsByValueModifier", tmf_is_by_value },
    { L"System::Runtime::CompilerServices::IsByValue", tmf_is_by_value },
    { L"Platform::Runtime::CompilerServices::IsByValue", tmf_is_by_value },
    { L"Microsoft::VisualC::IsConstModifier", tmf_is_const },
    { L"System::Runtime::CompilerServices::IsConst", tmf_is_const },
    { L"Platform::Runtime::CompilerServices::IsConst", tmf_is_const },
    { L"Microsoft::VisualC::IsCopyCtorModifier", tmf_is_copy_ctor },
    { L"Microsoft::VisualC::IsCXXReferenceModifier", tmf_is_cxx_reference },
    { L"Microsoft::VisualC::CxxUdtReturnStyleModifier",
                                                      tmf_is_cxx_udt_return },
    { L"System::Runtime::CompilerServices::IsUdtReturn",
                                                      tmf_is_cxx_udt_return },
    { L"Platform::Runtime::CompilerServices::IsUdtReturn",
                                                      tmf_is_cxx_udt_return },
    { L"Microsoft::VisualC::IsCXXPointerModifier",
                                             tmf_is_explicitly_dereferenced },
    { L"System::Runtime::CompilerServices::IsExplicitlyDereferenced",
                                             tmf_is_explicitly_dereferenced },
    { L"Platform::Runtime::CompilerServices::IsExplicitlyDereferenced",
                                             tmf_is_explicitly_dereferenced },
    { L"Microsoft::VisualC::IsImplicitlyDereferencedModifier",
                                             tmf_is_implicitly_dereferenced },
    { L"System::Runtime::CompilerServices::IsImplicitlyDereferenced",
                                             tmf_is_implicitly_dereferenced },
    { L"Platform::Runtime::CompilerServices::IsImplicitlyDereferenced",
                                             tmf_is_implicitly_dereferenced },
    { L"Microsoft::VisualC::IsLongModifier", tmf_is_long },
    { L"System::Runtime::CompilerServices::IsLong", tmf_is_long },
    { L"Platform::Runtime::CompilerServices::IsLong", tmf_is_long },
    { L"Microsoft::VisualC::IsSignedModifier", tmf_is_signed },
    { L"Microsoft::VisualC::NoSignSpecifiedModifier",
                                               tmf_is_sign_unspecified_byte },
    { L"System::Runtime::CompilerServices::IsSignUnspecifiedByte",
                                               tmf_is_sign_unspecified_byte },
    { L"Platform::Runtime::CompilerServices::IsSignUnspecifiedByte",
                                               tmf_is_sign_unspecified_byte },
    { L"Microsoft::VisualC::IsVolatileModifier", tmf_is_volatile },
    { L"System::Runtime::CompilerServices::IsVolatile", tmf_is_volatile },
    { L"Platform::Runtime::CompilerServices::IsVolatile", tmf_is_volatile },
    { L"System::Runtime::CompilerServices::IsCopyConstructed",
                                                    tmf_is_copy_constructed },
    { L"Platform::Runtime::CompilerServices::IsCopyConstructed",
                                                    tmf_is_copy_constructed },
  };
  a_type_modifier_flag modifier_flag = tmf_unknown;

  for (int i = 0; i < _countof(type_name_to_modifier_flag_map); ++i) {
    if (name == type_name_to_modifier_flag_map[i].name) {
      modifier_flag = type_name_to_modifier_flag_map[i].modifier_flag;
      break;
    }  /* if */
  }  /* for */
  return modifier_flag;
}  /* type_name_to_modifier_flag */


a_type_wrapper_ptr a_signature_decoder::decode_modified_type(
                                                  CorElementType element_type)
/*
Decode a type signature that is modified with a custom type modifier.
*/
{
  a_type_modifier_flag_set modifier_flags = tmf_none;
  a_type_wrapper_ptr       type;
  a_type_wrapper_ptr       boxed_type;

  /* Accumulate the type modifiers. */
  check_assertion(element_type == ELEMENT_TYPE_CMOD_REQD ||
                  element_type == ELEMENT_TYPE_CMOD_OPT);
  for(;;) {
    a_type_modifier_flag modifier_flag = tmf_unknown;
    mdToken type_token = read_token();
    if (TypeFromToken(type_token) == mdtTypeDef ||
        TypeFromToken(type_token) == mdtTypeRef) {
      a_qualified_name type_name = scope_.type_from_token(type_token)->name();
      modifier_flag = type_name_to_modifier_flag(type_name.as_string());
    }  /* if */
    if (modifier_flag == tmf_unknown) {
      if (element_type == ELEMENT_TYPE_CMOD_REQD) {
        /* This is an unknown required type modifier. */
        goto done;
      } else {
        /* Ignore any unknown optional type modifiers, but keep track of the
           fact that one was encountered. */
        contains_unknown_optional_type_modifiers_ = true;
      }  /* if */
    } else if (modifier_flag == tmf_is_implicitly_dereferenced &&
               (modifier_flags & tmf_is_implicitly_dereferenced) != 0) {
      /* This is the second "IsImplicitlyDereferenced" modifier we have
         encountered, which indicates it is an rvalue reference. */
      modifier_flags |= tmf_is_rvalue_reference;
    } else {
      modifier_flags |= modifier_flag;
    }  /* if */
    if (modifier_flag == tmf_is_boxed) {
      /* This is the "IsBoxed" modifier.  The next modifier is the boxed enum
         or value class type. */
      element_type = get_element_type();
      check_assertion(element_type == ELEMENT_TYPE_CMOD_OPT);
      mdToken boxed_type_token = read_token();
      auto class_type = scope_.type_from_token(boxed_type_token);
      boxed_type = class_type->copy();
      /* All of the modifiers that apply to the handle to System::Object,
         System::ValueType, or System::Enum that follows have already been
         accumulated.  Break out of the loop now so that any other modifiers
         are applied to the boxed type itself.  This is necessary to
         differentiate "const V^" from "V^ const":
         const V^:
           modopt(Boxed) modopt(V) modopt(Const) Class System::ValueType
         V^ const:
           modopt(Const) modopt(Boxed) modopt(V) Class System::ValueType */
      break;
    }  /* if */
    element_type = peek_element_type();
    if (element_type != ELEMENT_TYPE_CMOD_REQD &&
        element_type != ELEMENT_TYPE_CMOD_OPT) {
      break;
    }  /* if */
    element_type = get_element_type();
  }  /* for */
  /* Decode the modified type. */
  type = decode_raw_type();
  if (type->is_invalid()) {
    goto done;
  }  /* if */
  /* Resolve the ByRef indirection early so that all checks below can process
     the final indirection kind correctly. */
  if (type->is_of_kind(a_type_wrapper::twk_indirection)) {
    auto indirection = type->as_indirection();
    if (indirection->is_of_indirection_kind(
                                   a_type_indirection::tik_tentative_byref)) {
      if (is_cppcx_metadata) {
        if ((modifier_flags & tmf_is_const) != 0 &&
            indirection->underlying_type()->is_of_kind(
                                                 a_type_wrapper::twk_class)) {
          /* The specific sequence of CMOD_OPT[const], BYREF, VALUETYPE
             designates a by-ref struct, which projects in C++ to const T&. */
          indirection->underlying_type()->add_qualifier_flags(
                                                    a_type_wrapper::qf_const);
          indirection->set_indirection_kind(
                                           a_type_indirection::tik_reference);
          modifier_flags &= ~tmf_is_const;
        } else {
          indirection->set_indirection_kind(a_type_indirection::tik_pointer);
        }  /* if */
      } else {
        indirection->set_indirection_kind(
                                  a_type_indirection::tik_tracking_reference);
      }  /* if */
    }  /* if */
  }  /* if */
  /* Apply the modifiers to the decoded type. */
  if ((modifier_flags & tmf_is_boxed) != 0) {
    check_assertion(boxed_type);
    auto indirection = type->as_indirection();
    if (indirection != nullptr &&
        indirection->is_of_indirection_kind(a_type_indirection::tik_handle)) {
      auto class_type = indirection->underlying_type()->as_class();
      if (class_type != nullptr &&
          (class_type->name() == MAKE_CLASS_STRING(Object) ||
           class_type->name() == MAKE_CLASS_STRING(ValueType) ||
           class_type->name() == MAKE_CLASS_STRING(Enum))) {
        /* Any cv-qualifiers on the System::Object, System::ValueType, or
           System::Enum handle apply to the boxed handle type. */
        auto qualifier_flags = type->qualifier_flags();
        type = make_shared<a_type_indirection>(a_type_indirection::tik_handle,
                                               move(boxed_type));
        type->set_qualifier_flags(qualifier_flags);
      } else {
        unexpected_condition();
        type.reset();
        goto done;
      }  /* if */
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
  if ((modifier_flags & tmf_is_long) != 0) {
    if ((modifier_flags &
         (tmf_is_signed | tmf_is_sign_unspecified_byte)) != 0) {
      unexpected_condition();
      type.reset();
      goto done;
    } else if (type->is_of_kind(a_type_wrapper::twk_int)) {
      type->set_kind(a_type_wrapper::twk_long);
    } else if (type->is_of_kind(a_type_wrapper::twk_unsigned_int)) {
      type->set_kind(a_type_wrapper::twk_unsigned_long);
    } else if (type->is_of_kind(a_type_wrapper::twk_double)) {
      type->set_kind(a_type_wrapper::twk_long_double);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* */
  } else if ((modifier_flags & tmf_is_signed) != 0) {
    if ((modifier_flags & tmf_is_sign_unspecified_byte) != 0) {
      unexpected_condition();
      type.reset();
      goto done;
    } else if (type->is_of_kind(a_type_wrapper::twk_char)) {
      type->set_kind(a_type_wrapper::twk_signed_char);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* */
  } else if ((modifier_flags & tmf_is_sign_unspecified_byte) != 0) {
    if (type->is_of_kind(a_type_wrapper::twk_signed_char) ||
        type->is_of_kind(a_type_wrapper::twk_unsigned_char)) {
      type->set_kind(a_type_wrapper::twk_char);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* */
  }  /* if */
  if ((modifier_flags & tmf_is_implicitly_dereferenced) != 0) {
    /* Reference types are encoded as implicitly dereferenced handles or
       pointers. */
    auto indirection = type->as_indirection();
    if (indirection != nullptr &&
        (indirection->is_of_indirection_kind(
                                           a_type_indirection::tik_pointer) ||
         indirection->is_of_indirection_kind(
                                           a_type_indirection::tik_handle))) {
      auto underlying_type = indirection->underlying_type();
      /* Any cv-qualifiers apply to the underlying type, not the type
         indirection, so apply them here rather than below. */
      if ((modifier_flags & tmf_is_const) != 0) {
        underlying_type->add_qualifier_flags(a_type_wrapper::qf_const);
        modifier_flags &= ~tmf_is_const;
      }  /* if */
      if ((modifier_flags & tmf_is_volatile) != 0) {
        underlying_type->add_qualifier_flags(a_type_wrapper::qf_volatile);
        modifier_flags &= ~tmf_is_volatile;
      }  /* if */
      if (indirection->is_of_indirection_kind(
                                            a_type_indirection::tik_handle)) {
        /* A tracking reference is encoded as an implicitly dereferenced
           handle. */
        indirection->set_indirection_kind(
                                  a_type_indirection::tik_tracking_reference);
      } else {
        check_assertion(indirection->is_of_indirection_kind(
                                            a_type_indirection::tik_pointer));
        if ((modifier_flags & tmf_is_rvalue_reference) != 0) {
          /* An rvalue reference is encoded as a twice implicitly dereferenced
             pointer. */
          indirection->set_indirection_kind(
                                    a_type_indirection::tik_rvalue_reference);
        } else {
          /* A reference is encoded as an implicitly dereferenced pointer. */
          indirection->set_indirection_kind(
                                           a_type_indirection::tik_reference);
        }  /* if */
      }  /* if */
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
  if ((modifier_flags & tmf_is_by_value) != 0) {
    /* A ref class passed by value is encoded as a handle with the
       IsByValue modifier. */
    auto indirection = type->as_indirection();
    if (indirection != nullptr &&
        indirection->is_of_indirection_kind(a_type_indirection::tik_handle) &&
        indirection->underlying_type()->is_of_kind(
                                                 a_type_wrapper::twk_class)) {
      type = indirection->underlying_type();
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
  if ((modifier_flags & tmf_is_const) != 0) {
    type->add_qualifier_flags(a_type_wrapper::qf_const);
  }  /* if */
  if ((modifier_flags & tmf_is_volatile) != 0) {
    type->add_qualifier_flags(a_type_wrapper::qf_volatile);
  }  /* if */
  if ((modifier_flags & tmf_is_cxx_reference) != 0) {
    /* This modifier was formerly used to encode reference types when
       compiling with Microsoft's Managed Extensions for C++ (now superseded
       by C++/CLI).  Importing such metadata is not supported. */
  }  /* if */
  if ((modifier_flags & tmf_is_explicitly_dereferenced) != 0) {
    /* An interior_ptr is encoded as an explicitly dereferenced tracking
       reference. */
    auto indirection = type->as_indirection();
    if (indirection != nullptr &&
        indirection->is_of_indirection_kind(
                                a_type_indirection::tik_tracking_reference)) {
      indirection->set_indirection_kind(
                                    a_type_indirection::tik_interior_pointer);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
  if ((modifier_flags & tmf_compiler_marshal_override) != 0) {
    if (type->is_of_kind(a_type_wrapper::twk_unsigned_char)) {
      type->set_kind(a_type_wrapper::twk_bool);
    } else if (type->is_of_kind(a_type_wrapper::twk_unsigned_short)) {
      type->set_kind(a_type_wrapper::twk_wchar_t);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
  if ((modifier_flags & tmf_is_cxx_udt_return) != 0) {
    type = a_type_wrapper::create(a_type_wrapper::twk_cxx_udt_return);
  }  /* if */
  if ((modifier_flags & tmf_is_copy_ctor) != 0) {
    if (type->is_of_kind(a_type_wrapper::twk_void)) {
      type->set_kind(a_type_wrapper::twk_copy_ctor);
    } else {
      unexpected_condition();
      type.reset();
      goto done;
    }  /* if */
  }  /* if */
done:
  return type;
}  /* a_signature_decoder::decode_modified_type */


an_attribute_argument::an_attribute_argument(a_custom_attribute_data  &data,
                                             a_const_type_wrapper_ptr type,
                                             wstring                  name)
  : type_(move(type))
  , serialization_type_(SERIALIZATION_TYPE_UNDEFINED)
  , name_(move(name))
{
  check_assertion(type_ != nullptr && !data.empty());
  if (type_->kind() == a_type_wrapper::twk_indirection) {
    auto indirection = type_->as_indirection();
    if (indirection != nullptr &&
        indirection->is_of_indirection_kind(a_type_indirection::tik_handle)) {
      auto underlying_type = indirection->underlying_type();
      if (underlying_type != nullptr) {
        if (underlying_type->kind() == a_type_wrapper::twk_class) {
          auto class_type = indirection->underlying_type()->as_class();
          if (class_type->name() == MAKE_CLASS_STRING(String)) {
            /* The argument is a System::String^. */
            unique_ptr<wstring> string_value;
            data.read_and_advance(string_value);
            value_ = string_value;
            serialization_type_ = SERIALIZATION_TYPE_STRING;
          } else if (class_type->name() == MAKE_CLASS_STRING(Type)) {
            /* The argument is a System::Type^. */
            auto typeid_type = data.read_named_type_and_advance();
            type_ = typeid_type;
            serialization_type_ = SERIALIZATION_TYPE_TYPE;
          } else if (class_type->name() == MAKE_CLASS_STRING(Object)) {
            /* The argument is a boxed value type. */
            auto boxed_type = data.read_serialized_type_and_advance();
            an_attribute_argument boxed_arg(data, boxed_type);
            type_ = make_shared<a_type_indirection>(
                                               a_type_indirection::tik_handle,
                                               boxed_arg.type_->copy());
            value_ = boxed_arg.value_;
            serialization_type_ = SERIALIZATION_TYPE_TAGGED_OBJECT;
          } else {
            unexpected_condition();
          }  /* if */
        } else if (underlying_type->kind() == a_type_wrapper::twk_array) {
          /* The argument is an array type.  Determine its underlying
             type. */
          auto array_type = underlying_type->as_handle_to_array();
          auto element_type = array_type->underlying_type();
          check_assertion(element_type != nullptr);
          a_const_class_type_wrapper_ptr unresolved_type;
          serialization_type_ = SERIALIZATION_TYPE_SZARRAY;
          /* Determine how many elements are contained in the array. */
          USHORT array_size;
          data.read_and_advance(array_size);
          if (array_size == static_cast<USHORT>(-1)) {
            /* The array is NULL as opposed to empty. */
          } else {
            init_list_ = make_shared<an_attribute_argument_list>();
            init_list_->reserve(array_size);
            for (USHORT index = 0; index < array_size; ++index) {
              an_attribute_argument element_arg(data, element_type);
              if (element_arg.type()->uses_unresolved_type(unresolved_type)) {
                type_ = unresolved_type;
                init_list_.reset();
                serialization_type_ = SERIALIZATION_TYPE_UNDEFINED;
                break;
              }  /* if */
              init_list_->emplace_back(move(element_arg));
            }  /* for */
          }  /* if */
        } else {
          unexpected_condition();
        }  /* if */
      } else {
        unexpected_condition();
      }  /* if */
    } else {
      unexpected_condition();
    }  /* if */
  } else {
    /* The argument is a value type. */
    switch (type_->kind()) {
      case a_type_wrapper::twk_bool:
        data.read_and_advance(value_.bool_value());
        serialization_type_ = SERIALIZATION_TYPE_BOOLEAN;
        break;
      case a_type_wrapper::twk_wchar_t:
        data.read_and_advance(value_.unsigned_short_value());
        serialization_type_ = SERIALIZATION_TYPE_CHAR;
        break;
      case a_type_wrapper::twk_char:
      case a_type_wrapper::twk_signed_char:
        data.read_and_advance(value_.signed_char_value());
        serialization_type_ = SERIALIZATION_TYPE_I1;
        break;
      case a_type_wrapper::twk_unsigned_char:
        data.read_and_advance(value_.unsigned_char_value());
        serialization_type_ = SERIALIZATION_TYPE_U1;
        break;
      case a_type_wrapper::twk_short:
        data.read_and_advance(value_.short_value());
        serialization_type_ = SERIALIZATION_TYPE_I2;
        break;
      case a_type_wrapper::twk_unsigned_short:
        data.read_and_advance(value_.unsigned_short_value());
        serialization_type_ = SERIALIZATION_TYPE_U2;
        break;
      case a_type_wrapper::twk_int:
      case a_type_wrapper::twk_long:
        data.read_and_advance(value_.int_value());
        serialization_type_ = SERIALIZATION_TYPE_I4;
        break;
      case a_type_wrapper::twk_unsigned_int:
      case a_type_wrapper::twk_unsigned_long:
        data.read_and_advance(value_.unsigned_int_value());
        serialization_type_ = SERIALIZATION_TYPE_U4;
        break;
      case a_type_wrapper::twk_long_long:
        data.read_and_advance(value_.long_long_value());
        serialization_type_ = SERIALIZATION_TYPE_I8;
        break;
      case a_type_wrapper::twk_unsigned_long_long:
        data.read_and_advance(value_.unsigned_long_long_value());
        serialization_type_ = SERIALIZATION_TYPE_U8;
        break;
      case a_type_wrapper::twk_float:
        data.read_and_advance(value_.float_value());
        serialization_type_ = SERIALIZATION_TYPE_R4;
        break;
      case a_type_wrapper::twk_double:
      case a_type_wrapper::twk_long_double:
        data.read_and_advance(value_.double_value());
        serialization_type_ = SERIALIZATION_TYPE_R8;
        break;
      case a_type_wrapper::twk_class:
        { auto class_type = type_->as_class();
          check_assertion(class_type != nullptr);
          if (class_type->is_of_class_kind(a_class_type_wrapper::ck_class)) {
            auto type_definition = class_type->type_definition();
            if (type_definition != nullptr &&
                type_definition->kind() ==a_type_definition::tdk_enum_class &&
                type_definition->accessibility().is_publically_accessible()) {
              auto underlying_type = type_definition->underlying_type();
              check_assertion(underlying_type != nullptr);
              an_attribute_argument enum_argument(data, underlying_type);
              value_ = enum_argument.value_;
              serialization_type_ = SERIALIZATION_TYPE_ENUM;
            } else {
              unexpected_condition();
            }  /* if */
          } else if (class_type->is_of_class_kind(
                                       a_class_type_wrapper::ck_unresolved)) {
            /* The attribute argument makes use of an unresolved enum type.
               The enum's underlying type, which can only be determined via
               the type definition, needs to be known to decode this or any
               subsequent arguments. */
            data = a_custom_attribute_data();
          } else {
            unexpected_condition();
          }  /* if */
          break;
        }
      default:
        unexpected_condition();
        break;
    }  /* switch */
  }  /* if */
}  /* an_attribute_argument constructor. */


an_attribute_argument::an_attribute_argument(a_const_type_wrapper_ptr type,
                                             wstring                  name)
  : serialization_type_(SERIALIZATION_TYPE_TYPE)
  , type_(move(type))
{
  check_assertion(type_ != nullptr);
  switch (type_->kind()) {
    case a_type_wrapper::twk_bool:
    case a_type_wrapper::twk_char:
    case a_type_wrapper::twk_signed_char:
    case a_type_wrapper::twk_unsigned_char:
    case a_type_wrapper::twk_short:
    case a_type_wrapper::twk_unsigned_short:
    case a_type_wrapper::twk_wchar_t:
    case a_type_wrapper::twk_int:
    case a_type_wrapper::twk_unsigned_int:
    case a_type_wrapper::twk_long:
    case a_type_wrapper::twk_unsigned_long:
    case a_type_wrapper::twk_long_long:
    case a_type_wrapper::twk_unsigned_long_long:
    case a_type_wrapper::twk_float:
    case a_type_wrapper::twk_double:
    case a_type_wrapper::twk_long_double:
    case a_type_wrapper::twk_class:
      /* The argument is "type::typeid". */
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
}  /* an_attribute_argument Constructor. */


void an_attribute_argument::value_constructor_helper()
{
  switch (value_.element_type()) {
    case ELEMENT_TYPE_BOOLEAN:
    case ELEMENT_TYPE_CHAR:
    case ELEMENT_TYPE_I1:
    case ELEMENT_TYPE_U1:
    case ELEMENT_TYPE_I2:
    case ELEMENT_TYPE_U2:
    case ELEMENT_TYPE_I4:
    case ELEMENT_TYPE_U4:
    case ELEMENT_TYPE_I8:
    case ELEMENT_TYPE_U8:
    case ELEMENT_TYPE_STRING:
      /* The argument is a fundamental type or System::String^. */
      type_ = a_type_wrapper::create(value_.element_type());
      serialization_type_ = static_cast<CorSerializationType>(
                                                     value_.element_type());
      break;
    case ELEMENT_TYPE_OBJECT:
      /* The argument is "static_cast<System::Object^>(nullptr)". */
      check_assertion(value_.unsigned_int_value() == 0);
      type_ = a_type_wrapper::create(value_.element_type());
      serialization_type_ = SERIALIZATION_TYPE_TAGGED_OBJECT;
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
}  /* value_constructor_helper */


an_attribute_argument::an_attribute_argument(an_element_value value,
                                             wstring          name)
{
  value_constructor_helper();
}  /* an_attribute_argument Constructor. */


an_attribute_argument::an_attribute_argument(a_const_type_wrapper_ptr type,
                                             an_element_value         value,
                                             wstring                  name)
  : value_(move(value))
  , name_(move(name))
{
  value_constructor_helper();
  check_assertion(type != nullptr);
  if (type->kind() == a_type_wrapper::twk_indirection) {
    auto indirection = type->as_indirection();
    if (indirection != nullptr &&
        indirection->is_of_indirection_kind(a_type_indirection::tik_handle)) {
      auto underlying_type = indirection->underlying_type();
      if (underlying_type != nullptr) {
        if (underlying_type->kind() == a_type_wrapper::twk_class) {
          auto class_type = indirection->underlying_type()->as_class();
          check_assertion(class_type != nullptr);
          if (class_type->is_of_class_kind(a_class_type_wrapper::ck_class)) {
            if (class_type->name() == MAKE_CLASS_STRING(Type)) {
              /* The argument is "static_cast<System::Type^>(nullptr)". */
              check_assertion(value_.element_type() == ELEMENT_TYPE_OBJECT);
              type_ = move(type);
              serialization_type_ = SERIALIZATION_TYPE_TAGGED_OBJECT;
            } else if (!IsNilToken(class_type->token()) &&
                       TypeFromToken(class_type->token()) == mdtTypeDef) {
              auto type_definition = class_type->type_definition();
              if (type_definition != nullptr &&
                  type_definition->kind() ==
                                          a_type_definition::tdk_enum_class &&
                  type_definition->accessibility().
                                                 is_publically_accessible()) {
                switch (value_.element_type()) {
                  case ELEMENT_TYPE_BOOLEAN:
                  case ELEMENT_TYPE_CHAR:
                  case ELEMENT_TYPE_I1:
                  case ELEMENT_TYPE_U1:
                  case ELEMENT_TYPE_I2:
                  case ELEMENT_TYPE_U2:
                  case ELEMENT_TYPE_I4:
                  case ELEMENT_TYPE_U4:
                  case ELEMENT_TYPE_I8:
                  case ELEMENT_TYPE_U8:
                  case ELEMENT_TYPE_OBJECT:
                    /* The argument is "static_cast<enum_type^>(value)". */
                    type_ = move(type);
                    serialization_type_ = SERIALIZATION_TYPE_TAGGED_OBJECT;
                    break;
                  default:
                    unexpected_condition();
                    break;
                }  /* switch */
              } else {
                unexpected_condition();
              }  /* if */
            } else {
              unexpected_condition();
            }  /* if */
          } else {
            unexpected_condition();
          }  /* if */
        } else {
          switch (value_.element_type()) {
            case ELEMENT_TYPE_BOOLEAN:
            case ELEMENT_TYPE_CHAR:
            case ELEMENT_TYPE_I1:
            case ELEMENT_TYPE_U1:
            case ELEMENT_TYPE_I2:
            case ELEMENT_TYPE_U2:
            case ELEMENT_TYPE_I4:
            case ELEMENT_TYPE_U4:
            case ELEMENT_TYPE_I8:
            case ELEMENT_TYPE_U8:
            case ELEMENT_TYPE_R4:
            case ELEMENT_TYPE_R8:
            case ELEMENT_TYPE_OBJECT:
              /* The argument is "static_cast<fundamental_type^>(value)". */
              serialization_type_ = SERIALIZATION_TYPE_TAGGED_OBJECT;
              break;
            default:
              unexpected_condition();
              break;
          }  /* switch */
        }  /* if */
      } else {
        unexpected_condition();
      }  /* if */
    } else {
      unexpected_condition();
    }  /* if */
  } else if (type->kind() == a_type_wrapper::twk_class) {
    auto class_type = type_->as_class();
    check_assertion(class_type != nullptr);
    if (class_type->is_of_class_kind(a_class_type_wrapper::ck_class)) {
      auto type_definition = class_type->type_definition();
      if (type_definition != nullptr &&
          type_definition->kind() == a_type_definition::tdk_enum_class &&
          type_definition->accessibility().is_publically_accessible()) {
        switch (value_.element_type()) {
          case ELEMENT_TYPE_BOOLEAN:
          case ELEMENT_TYPE_CHAR:
          case ELEMENT_TYPE_I1:
          case ELEMENT_TYPE_U1:
          case ELEMENT_TYPE_I2:
          case ELEMENT_TYPE_U2:
          case ELEMENT_TYPE_I4:
          case ELEMENT_TYPE_U4:
          case ELEMENT_TYPE_I8:
          case ELEMENT_TYPE_U8:
            /* The argument is "static_cast<enum_type>(value)". */
            type_ = move(type);
            serialization_type_ = SERIALIZATION_TYPE_ENUM;
            break;
          default:
            unexpected_condition();
            break;
        }  /* switch */
      } else {
        unexpected_condition();
      }  /* if */
    } else {
      unexpected_condition();
    }  /* if */
  } else {
    unexpected_condition();
  }  /* if */
}  /* an_attribute_argument Constructor. */


a_type_wrapper_ptr a_signature_decoder::decode_raw_type()
/*
Decode a type signature, but preserve any ByRef type indirections so that
decode_modified_type and decode_type can correctly perform language-specific
type mappings.
*/
{
  a_type_wrapper_ptr type;
  wostringstream buffer;
  bool           is_generic;
  CorElementType element_type = get_element_type();

  if (element_type == ELEMENT_TYPE_GENERICINST) {
    is_generic = true;
    element_type = get_element_type();
  } else {
    is_generic = false;
  }  /* if  */
  switch (element_type) {
    case ELEMENT_TYPE_VOID:
    case ELEMENT_TYPE_BOOLEAN:
    case ELEMENT_TYPE_I1:
    case ELEMENT_TYPE_U1:
    case ELEMENT_TYPE_I2:
    case ELEMENT_TYPE_U2:
    case ELEMENT_TYPE_I4:
    case ELEMENT_TYPE_U4:
    case ELEMENT_TYPE_I8:
    case ELEMENT_TYPE_U8:
    case ELEMENT_TYPE_R4:
    case ELEMENT_TYPE_R8:
    case ELEMENT_TYPE_STRING:
    case ELEMENT_TYPE_TYPEDBYREF:
    case ELEMENT_TYPE_I:
    case ELEMENT_TYPE_U:
    case ELEMENT_TYPE_OBJECT:
      type = a_type_wrapper::create(element_type);
      break;
    case ELEMENT_TYPE_PTR:
      { a_type_wrapper_ptr underlying_type;
        bool               is_system_string_member =
                                              scope_.is_system_string_scope();

        /* If we are decoding a type signature associated with a member of
           System::String, then we should perform the following conversions:
            ELEMENT_TYPE_PTR ELEMENT_TYPE_I1   -> 'const char*'
            ELEMENT_TYPE_PTR ELEMENT_TYPE_CHAR -> 'const __wchar_t*' */
        if (is_system_string_member &&
            peek_element_type() == ELEMENT_TYPE_I1) {
          /* ELEMENT_TYPE_I1 would normally be converted to "signed char", but
              we only want "char", so we don't recursively call decode_type
              in this case. */
          (void)get_element_type();
          underlying_type = a_type_wrapper::create(a_type_wrapper::twk_char);
          underlying_type->add_qualifier_flags(a_type_wrapper::qf_const);
        } else if (is_system_string_member &&
                   peek_element_type() == ELEMENT_TYPE_CHAR) {
          underlying_type = decode_type();
          underlying_type->add_qualifier_flags(a_type_wrapper::qf_const);
        } else {
          underlying_type = decode_type();
        }  /* if */
        if (!underlying_type->is_invalid()) {
          type = make_shared<a_type_indirection>(
                                              a_type_indirection::tik_pointer,
                                              move(underlying_type));
        }  /* if */
        break;
      }
    case ELEMENT_TYPE_BYREF:
      { a_type_wrapper_ptr underlying_type = decode_type();
        if (!underlying_type->is_invalid()) {
          type = make_shared<a_type_indirection>(
                               a_type_indirection::tik_tentative_byref,
                               move(underlying_type));
        }  /* if */
        break;
      }
    case ELEMENT_TYPE_VALUETYPE:
    case ELEMENT_TYPE_CLASS:
      { a_class_type_wrapper_ptr class_type;
        mdToken token = read_token();
        check_assertion(TypeFromToken(token) == mdtTypeDef ||
                        TypeFromToken(token) == mdtTypeRef);
        if (is_generic) {
          a_generic_instance_scope_ptr generic_instance_scope =
                                                   decode_generic_arguments();
          class_type = generic_instance_scope->type_from_token(token)->
                                                           copy()->as_class();
          class_type->set_generic_instance_scope(
                                                move(generic_instance_scope));
        } else {
          class_type = scope_.type_from_token(token)->copy()->as_class();
        }  /* if */
        if (element_type == ELEMENT_TYPE_CLASS) {
          type = make_shared<a_type_indirection>(
                                               a_type_indirection::tik_handle,
                                               move(class_type));
        } else {
          type = move(class_type);
        }  /* if */
        break;
      }
    case ELEMENT_TYPE_VAR:
      type = scope_.generic_type_arguments()[read_one_byte()]->copy();
      break;
    case ELEMENT_TYPE_CHAR:
      { auto import_flags =
                   scope_.import_scope().containing_assembly().import_flags();
        bool builtin_wchar_t =
                             (import_flags & cpp_cli_wchar_t_is_keyword) != 0;
        type = a_type_wrapper::create(element_type, builtin_wchar_t);
        break;
      }
    case ELEMENT_TYPE_SZARRAY:
      { a_type_wrapper_ptr underlying_type = decode_type();
        if (!underlying_type->is_invalid()) {
          type = an_array_type_wrapper::create_handle_to_array(
                                                             underlying_type);
        }  /* if */
        break;
      }
    case ELEMENT_TYPE_ARRAY:
      { /* Get the underlying type. */
        a_type_wrapper_ptr underlying_type = decode_type();
        if (!underlying_type->is_invalid()) {
          /* Get the Rank of the array. */
          ULONG rank, num_of_sizes, num_of_lower_bounds;
          rank = read_four_bytes();
          num_of_sizes = read_four_bytes();
          for (ULONG i = 0; i < num_of_sizes; ++i) {
            /* We don't need the sizes.  They don't affect the type name.
               However, we need to consume these bytes in the signature
               blob. */
            (void)read_four_bytes();
          }  /* if */
          num_of_lower_bounds = read_four_bytes();
          for (ULONG i = 0; i < num_of_lower_bounds; ++i) {
            /* We don't need the lower bounds.  They don't affect the
               type name.  However, we need to consume these bytes in the
               signature blob. */
            (void)read_four_bytes();
          }  /* if */
          type = an_array_type_wrapper::create_handle_to_array(
                                                       underlying_type, rank);
        }  /* if */
        break;
      }
    case ELEMENT_TYPE_MVAR:
      type = scope_.generic_method_arguments()[read_one_byte()]->copy();
      break;
    case ELEMENT_TYPE_CMOD_REQD:
    case ELEMENT_TYPE_CMOD_OPT:
      type = decode_modified_type(element_type);
      break;
    case ELEMENT_TYPE_INTERNAL:
      break;
    case ELEMENT_TYPE_FNPTR:
      { auto function_type = decode_method(*this, mdTokenNil);
        if (!function_type->is_invalid()) {
          type = make_shared<a_type_indirection>(
                                              a_type_indirection::tik_pointer,
                                              move(function_type));
        }  /* if */
        break;
      }
    default:
      unexpected_condition();
      break;
  }  /* switch */
  if (type == nullptr) {
    type = a_type_wrapper::create(a_type_wrapper::twk_invalid);
  }  /* if */
  return type;
}  /* a_signature_decoder::decode_raw_type */


a_type_wrapper_ptr a_signature_decoder::decode_type()
/*
Decode a type signature.
*/
{
  auto type = decode_raw_type();
  auto indirection = type->as_indirection();

  if (indirection != nullptr && indirection->is_of_indirection_kind(
                                   a_type_indirection::tik_tentative_byref)) {
    /* Perform ByRef to language-specific type mapping that can't be performed
       earlier because decode_modified_type needs to see the raw type in some
       cases. */
    if (is_cppcx_metadata) {
      indirection->set_indirection_kind(a_type_indirection::tik_pointer);
    } else {
      indirection->set_indirection_kind(
                                  a_type_indirection::tik_tracking_reference);
    }  /* if */
  }  /* if */
  return type;
}  /* a_signature_decoder::decode_type */


a_function_type_wrapper_ptr a_signature_decoder::decode_method(
                                                 a_signature_decoder &decoder,
                                                 mdToken             token)
/*
Decode a function signature and then combine the various elements along with
the name of the function to create a declaration which we return as an
a_function_type_wrapper_ptr.  Note: this function also handles decoding a
property signature which is almost the same as a method signature.
*/
{
  auto                    &import_scope = decoder.scope_.import_scope();
  auto                    import_interface = import_scope.import_interface();
  a_function_type_wrapper_ptr
                          type;
  a_type_wrapper_ptr      return_type;
  a_method_parameter_list parameter_list;
  BYTE                    first_byte;
  BYTE                    calling_convention;
  BYTE                    generic_arity = 0;
  ULONG                   number_of_parameters;
  ULONG                   param_index = 1;
  bool                    is_method_def = TypeFromToken(token) == mdtMethodDef;
  bool                    is_member_ref = TypeFromToken(token) == mdtMemberRef;
  bool                    is_property = TypeFromToken(token) == mdtProperty;

  first_byte = decoder.read_one_byte();
  calling_convention = first_byte & IMAGE_CEE_CS_CALLCONV_MASK;
  check_assertion(IsNilToken(token) || is_method_def || is_member_ref ||
                  (is_property &&
                   calling_convention == IMAGE_CEE_CS_CALLCONV_PROPERTY));
  /* If this is a generic method, read the count of generic parameters. */
  if ((first_byte & IMAGE_CEE_CS_CALLCONV_GENERIC) != 0) {
    generic_arity = decoder.read_one_byte();
  }  /* if */
  number_of_parameters = decoder.read_four_bytes();
  parameter_list.reserve(number_of_parameters);
  /* Decode the return type. */
  return_type = decoder.decode_type();
  if (!return_type->is_invalid()) {
    if (return_type->is_of_kind(a_type_wrapper::twk_copy_ctor)) {
      /* Legacy encodings of the copy constructor encoded it as a regular
         function (not a .ctor) with a void return type marked with the
         IsCopyCtorModifier modifier. */
      return_type.reset();
    } else if (return_type->is_of_kind(a_type_wrapper::twk_cxx_udt_return)) {
      /* A function that returns a ref class by value is encoded in metadata
         as a function with a void return type (marked with the IsUdtReturn
         modifier) and whose first parameter is a tracking reference to
         a handle to the ref class.  A function that returns a native class by
         value is encoded in metadata as a function whose return type and
         first parameter are pointers to the native class; the return type is
         also marked with the IsUdtReturn modifier.  decode_modified_type
         transforms the return type in both cases to the special
         twk_cxx_udt_return type. */
      if (number_of_parameters > 0) {
        ++param_index;
        a_type_wrapper_ptr param_type = decoder.decode_type();
        if (param_type->is_invalid()) {
          goto done;
        }  /* if */
        auto indirection = param_type->as_indirection();
        if (indirection != nullptr &&
            indirection->is_of_indirection_kind(
                                a_type_indirection::tik_tracking_reference)) {
          auto handle_type = indirection->underlying_type()->as_indirection();
          if (handle_type != nullptr &&
              handle_type->is_of_indirection_kind(
                                            a_type_indirection::tik_handle)) {
            auto class_type = handle_type->underlying_type()->as_class();
            if (class_type != nullptr) {
              return_type = move(class_type);
              goto have_return_type;
            }  /* if */
          }  /* if */
        } else if (indirection != nullptr &&
                   indirection->is_of_indirection_kind(
                                           a_type_indirection::tik_pointer)) {
          auto class_type = indirection->underlying_type()->as_class();
          if (class_type != nullptr) {
            return_type = move(class_type);
            goto have_return_type;
          }  /* if */
        }  /* if */
      }  /* if */
      unexpected_condition();
      goto done;
    }  /* if */
have_return_type:
    /* Add the return type to the parameter list. */
    mdParamDef return_param_token = mdParamDefNil;
    if (is_method_def) {
      /* If the method's return value has any custom attributes, they will be
         attached to the parameter at index 0. */
      import_interface->GetParamForMethodIndex(token, 0, &return_param_token);
    }  /* if */
    a_method_parameter return_value(import_scope, return_type,
                                    return_param_token);
    /* Add the real parameters to the parameter list. */
    for (; param_index <= number_of_parameters; ++param_index) {
      mdParamDef         param_token = mdParamDefNil;
      a_type_wrapper_ptr param_type = decoder.decode_type();
      if (param_type->is_invalid()) {
        goto done;
      }  /* if */
      if (is_method_def) {
        HRESULT hr = import_interface->GetParamForMethodIndex(token,
                                                              param_index,
                                                              &param_token);
        CHECK_API_RESULT(hr, GetParamForMethodIndex);
      }  /* if */
      parameter_list.emplace_back(a_method_parameter(import_scope,
                                                     move(param_type),
                                                     param_token));
    }  /* for */
    /* Mark the last parameter as a parameter array if appropriate. */
    if (!parameter_list.empty()) {
      auto &last_parameter = parameter_list.back();
      auto array_type = last_parameter.type()->as_handle_to_array();
      if (array_type != nullptr &&
          last_parameter.is_parameter_array(import_scope)) {
        array_type->set_array_kind(an_array_type_wrapper::ak_param_array);
      }  /* if */
    }  /* if */
    type = make_shared<a_function_type_wrapper>(generic_arity,
                                                calling_convention,
                                                move(return_value),
                                                move(parameter_list));
  }  /* if */
done:
  if (type == nullptr) {
    type = make_shared<a_function_type_wrapper>();
  }  /* if */
  return type;
}  /* a_signature_decoder::decode_method */


wstring an_element_value::get_source_code(
                                     a_const_type_wrapper_ptr cast_type) const
/*
Returns the source code for the value.
*/
{
  wostringstream buffer;
  bool           cast_emitted = false;

  if (cast_type != nullptr) {
    a_const_type_wrapper_ptr underlying_type;
    auto                     indirection = cast_type->as_indirection();
    if (indirection != nullptr) {
      underlying_type = indirection->underlying_type();
    } else {
      underlying_type = cast_type;
    }  /* if */
    if (underlying_type->is_of_kind(a_type_wrapper::twk_wchar_t) ||
        (underlying_type->is_of_kind(a_type_wrapper::twk_class) &&
         underlying_type->as_class()->type_definition() != nullptr &&
         underlying_type->as_class()->type_definition()->kind() ==
                                         a_type_definition::tdk_enum_class)) {
      buffer << L"static_cast<" << underlying_type->get_string() << L">(";
      cast_emitted = true;
    }  /* if */
  }  /* if */
  switch (element_type_) {
    case ELEMENT_TYPE_BOOLEAN:
      buffer << (value_.bool_value ? L"true" : L"false");
      break;
    case ELEMENT_TYPE_I1:
      if (value_.signed_char_value == CHAR_MIN) {
        buffer << L"static_cast<char>(";
        cast_emitted = true;
      } else if (value_.signed_char_value < 0) {
        buffer << L'-';
      }  /* if */
      buffer << L"0x" << hex << abs(value_.signed_char_value);
      break;
    case ELEMENT_TYPE_U1:
      /* The static_cast ensures the hexadecimal value is emitted instead of
         the character value. */
      buffer << L"0x" << hex
             << static_cast<unsigned int>(value_.unsigned_char_value)
             << L'u';
      break;
    case ELEMENT_TYPE_I2:
      if (value_.short_value == SHRT_MIN) {
        buffer << L"static_cast<short>(";
        cast_emitted = true;
      } else if (value_.short_value < 0) {
        buffer << L'-';
      }  /* if */
      buffer << L"0x" << hex << abs(value_.short_value);
      break;
    case ELEMENT_TYPE_CHAR:
    case ELEMENT_TYPE_U2:
      /* The static_cast ensures the hexadecimal value is emitted instead of
         the character value. */
      buffer << L"0x" << hex
             << static_cast<unsigned int>(value_.unsigned_short_value)
             << L'u';
      break;
    case ELEMENT_TYPE_I4:
      if (value_.int_value == INT_MIN) {
        buffer << L"static_cast<int>(";
        cast_emitted = true;
      } else if (value_.int_value < 0) {
        buffer << L'-';
      }  /* if */
      buffer << L"0x" << hex << abs(value_.int_value);
      break;
    case ELEMENT_TYPE_U4:
      buffer << L"0x" << hex << value_.unsigned_int_value << L'u';
      break;
    case ELEMENT_TYPE_I8:
      if (value_.int_value == LLONG_MIN) {
        buffer << L"static_cast<long long>(";
        cast_emitted = true;
      } else if (value_.int_value < 0) {
        buffer << L'-';
      }  /* if */
      buffer << L"0x" << hex << abs(value_.long_long_value) << L"ll";
      break;
    case ELEMENT_TYPE_U8:
      buffer << L"0x" << hex << value_.unsigned_long_long_value << L"ull";
      break;
    case ELEMENT_TYPE_R4:
      { UINT      f = *reinterpret_cast<const UINT*>(&value_.float_value);
        bool      sign =         (f & 0x80000000U) != 0;
        int       exponent = int((f & 0x7F800000U) >> 23);
        ULONGLONG fraction =      f & 0x007FFFFFU;
        int       bias = 127;

        if (sign) buffer << L'-';
        if (exponent == 0xFF) {
          const ULONGLONG quiet_bit = 0x00400000ULL;

          if (fraction == 0) {
            buffer << "__builtin_huge_valf()";
          } else if (fraction & quiet_bit) {
            buffer << "__builtin_nanf(\"" << (fraction & ~quiet_bit) << "\")";
          } else {
            buffer << "__builtin_nansf(\"" << fraction << "\")";
          }  /* if */
        } else {
          if (exponent == 0) {
            buffer << L"0x0.";
            if (fraction == 0) bias = 0;
            else bias--;
          } else {
            buffer << L"0x1.";
          }  /* if */
          buffer << setw(6) << setfill(L'0') << hex << (fraction << 1);
          buffer << 'p' << dec << (exponent - bias);
        }  /* if */
        break;
      }  /* case ELEMENT_TYPE_R4 */
    case ELEMENT_TYPE_R8:
      { ULONGLONG d = *reinterpret_cast<const ULONGLONG*>(
                                                        &value_.double_value);
        bool      sign =         (d & 0x8000000000000000ULL) != 0;
        int       exponent = int((d & 0x7FF0000000000000ULL) >> 52);
        ULONGLONG fraction =      d & 0x000FFFFFFFFFFFFFULL;
        int       bias = 1023;

        if (sign) buffer << L'-';
        if (exponent == 0x7FF) {
          const ULONGLONG quiet_bit = 0x0008000000000000ULL;

          if (fraction == 0) {
            buffer << "__builtin_huge_val()";
          } else if (fraction & quiet_bit) {
            buffer << "__builtin_nan(\"" << (fraction & ~quiet_bit) << "\")";
          } else {
            buffer << "__builtin_nans(\"" << fraction << "\")";
          }  /* if */
        } else {
          if (exponent == 0) {
            buffer << L"0x0.";
            if (fraction == 0) bias = 0;
            else bias--;
          } else {
            buffer << L"0x1.";
          }  /* if */
          buffer << setw(13) << setfill(L'0') << hex << fraction;
          buffer << 'p' << dec << (exponent - bias);
        }  /* if */
        break;
      }  /* case ELEMENT_TYPE_R8 */
    case ELEMENT_TYPE_STRING:
      { wchar_t *string_value = value_.string.value;
        size_t  string_length = value_.string.length;
        if (string_value == nullptr) {
          buffer << L"nullptr";
        } else {
          buffer << L"L\"";
          for (ULONG i = 0; i < string_length; ++i) {
            append_to_string_literal(buffer, string_value[i]);
          }  /* for */
          buffer << L'\"';
        }
        break;
      }  /* case ELEMENT_TYPE_STRING */
    case ELEMENT_TYPE_OBJECT:
      /* The nullptr constant. */
      check_assertion(value_.unsigned_int_value == 0);
      buffer << L"nullptr";
      break;
    default:
      unexpected_condition();
      break;
  }  /* switch */
  if (cast_emitted) {
    buffer << L')';
  }  /* if */
  return buffer.str();
}  /* an_element_value::get_source_code */


/*
The class that handles the reading of metadata.
*/
class a_metadata_reader {
public:
  a_metadata_reader()
    : alink_handle_(nullptr),
      pfn_create_assembly_name_object_(nullptr),
      pfn_compare_assembly_identity_(nullptr)
  {
    initialized_ = initialize();
  }  /* constructor */


  bool is_initialized() const
  {
    return initialized_;
  }  /* is_initialized */


  an_assembly_index import_assembly(
                                a_const_char              *full_assembly_path,
                                a_cpp_cli_import_flag_set import_flags,
                                bool                      *is_duplicated);

  void import_all_types(an_assembly_index assembly_index,
                        char              *buffer,
                        size_t            *buffer_size)
  /*
  Import all the types defined in the specified assembly.  *buffer_size
  describes the allocated size of *buffer.  If there is enough space, the
  generated code is returned in *buffer and the amount of buffer used is
  returned in *buffer_size.  Otherwise, *buffer is null terminated and
  *buffer_size contains the required size.
  */
  {
    check_assertion(is_initialized() && buffer_size != nullptr);
    auto &assembly = assembly_from_index(assembly_index);
    assembly.import_all_types(buffer, buffer_size);
  }  /* import_all_types */

  void import_all_types_to_stream(an_assembly_index assembly_index,
                                  ostringstream     &os)
  /*
  Import all the types defined in the specified assembly.  Returns the result
  in the ostringstream, encoded in UTF8.
  */
  {
    check_assertion(is_initialized());
    auto &assembly = assembly_from_index(assembly_index);
    assembly.import_all_types(os);
  }  /* import_all_types_to_stream */

  void import_class_definition(ostringstream           &buffer,
                               an_assembly_scope_index assembly_scope_index,
                               a_cpp_cli_token         typedef_token,
                               bool                    class_body_only,
                               a_boolean               *is_delegate);
  bool initialize();
  bool trans_unit_init(a_const_char *tu_file_name);
  void trans_unit_wrapup();

  HRESULT create_assembly_name_object(IAssemblyName **assembly_name_object,
                                      LPCWSTR       assembly_name,
                                      DWORD         flags,
                                      LPVOID        reserved)
  /*
  A wrapper for the CreateAssemblyNameObject function in fusion.dll.
  */
  {
    check_assertion(pfn_create_assembly_name_object_ != nullptr);
    return pfn_create_assembly_name_object_(assembly_name_object,
                                            assembly_name, flags, reserved);
  }  /* create_assembly_name_object */

  HRESULT compare_assembly_identity(
                                LPCWSTR                  pwzAssemblyIdentity1,
                                BOOL                     fUnified1,
                                LPCWSTR                  pwzAssemblyIdentity2,
                                BOOL                     fUnified2,
                                BOOL                     *pfEquivalent,
                                AssemblyComparisonResult *pResult)
  /*
  A wrapper for the CompareAssemblyIdentity function in fusion.dll.
  */
  {
    check_assertion(pfn_compare_assembly_identity_ != nullptr);
    return pfn_compare_assembly_identity_(pwzAssemblyIdentity1,
                                          fUnified1,
                                          pwzAssemblyIdentity2,
                                          fUnified2,
                                          pfEquivalent,
                                          pResult);
  }  /* compare_assembly_identity */
  
  
  an_assembly_ptr find_assembly_by_name(const an_assembly_name &assembly_name)
  /*
  Returns the first imported assembly that has a name that matches
  assembly_name.  Returns nullptr if a matching assembly has not been imported.
  */
  {
    an_assembly_ptr assembly_ptr = nullptr;

    for (auto &assembly : assemblies_) {
      if (assembly_name.reference_matches_definition(
                                                 assembly->assembly_name())) {
        assembly_ptr = assembly.get();
        break;
      }  /* if */
    }  /* for */
    return assembly_ptr;
  }  /* find_assembly_by_name */

  an_assembly_index find_assembly_by_path(const wstring &assembly_path)
  /*
  Returns the 1-based assembly index of the assembly imported from
  assembly_path.  Returns 0 if the assembly has not been imported.
  */
  {
    auto begin = assemblies_.begin();
    auto end = assemblies_.end();
    auto result = find_if(begin, end,
                          [&](const unique_ptr<an_assembly> &assembly) {
                            return assembly->assembly_path() == assembly_path;
                          });
    return result == end ? 0 : result->get()->assembly_index();
  }  /* find_assembly_by_path */

  an_assembly& assembly_from_index(an_assembly_index assembly_index)
  /*
  Returns the assembly with the specified assembly_index.
  */
  {
    check_assertion(assembly_index > 0 &&
                    assembly_index <= assemblies_.size());
    return *assemblies_[assembly_index - 1];
  }  /* a_metadata_reader::assembly_from_index */

  a_const_class_type_wrapper_ptr find_type_by_name(
                                    wstring              type_name,
                                    a_const_assembly_ptr referencing_assembly)
  /*
  Attempt to find a type from a fully-qualified type name.  The type name is
  encoded as per the "Specifying Fully Qualified Type Names" section in the
  .NET System.Reflection API reference:
  http://msdn.microsoft.com/en-us/library/yfsftwz6.aspx.
  */
  {
    a_const_class_type_wrapper_ptr type;
    an_assembly_name assembly_name;
    auto type_name_begin = type_name.cbegin();
    auto type_name_end = type_name.cend();
    auto comma_iter = find_unescaped_character(L',', type_name_begin,
                                               type_name_end);
    if (comma_iter != type_name_end) {
      /* An unescaped comma in the full type name delineates the type name
         from the assembly name. */
      auto assembly_name_begin = find_if(comma_iter + 1, type_name_end,
                                       [](wchar_t ch) { return ch != L' '; });
      assembly_name = wstring(assembly_name_begin, type_name_end);
      type_name.resize(std::distance(type_name_begin, comma_iter));
    }  /* if */
    check_assertion(!type_name.empty());
    a_const_assembly_ptr     resolved_assembly = nullptr;
    a_const_import_scope_ptr resolved_import_scope = nullptr;
    mdTypeDef                resolved_token = mdTypeDefNil;
    if (!assembly_name.display_name().empty()) {
      auto named_assembly = find_assembly_by_name(assembly_name);
      if (named_assembly != nullptr &&
          named_assembly->find_type_by_name(type_name, resolved_import_scope,
                                            resolved_token)) {
        /* The type was found in the named assembly. */
        resolved_assembly = named_assembly;
      }  /* if */
    } else if (referencing_assembly != nullptr) {
      /* The fully-qualified type name did not specify an assembly. Attempt to
         find the type in the referencing assembly. */
      if (referencing_assembly->find_type_by_name(type_name,
                                                  resolved_import_scope,
                                                  resolved_token)) {
        /* The type was found in the referencing assembly. */
        resolved_assembly = referencing_assembly;
      } else {
        /* The type was not found in the referencing assembly.  Attempt to
           find it in the system assembly. */
        auto system_assembly = &assembly_from_index(1);
        if (system_assembly != referencing_assembly &&
            system_assembly->find_type_by_name(type_name,
                                               resolved_import_scope,
                                               resolved_token)) {
          /* The type was found in the system assembly. */
          resolved_assembly = system_assembly;
        }  /* if */
      }  /* if */
    }  /* if */
    if (resolved_assembly != nullptr) {
      check_assertion(resolved_import_scope != nullptr);
      check_assertion(!IsNilToken(resolved_token));
      check_assertion(TypeFromToken(resolved_token) == mdtTypeDef);
      auto &type_definition = resolved_import_scope->
                                          get_type_definition(resolved_token);
      type = type_definition.class_type();
    } else {
      /* The type was not found. */
      a_qualified_name name;
      name = a_qualified_name::from_dotted_name(type_name);
      type = make_shared<a_class_type_wrapper>(
                                          a_class_type_wrapper::ck_unresolved,
                                          name, nullptr, mdTypeDefNil);
    }  /* if */
    return type;
  }  /* find_type_by_name */

private:
  bool init_clr_host();
  bool init_clr_runtime_info();
  bool init_alink();
  bool init_assembly_functions();
  bool init_metadata_interfaces();

private:
  vector<unique_ptr<an_assembly>>
                assemblies_;
                        /* The assemblies that we have imported. */
  HMODULE       alink_handle_;
                        /* The handle for alink.dll. */
  CComPtr<IALink>
                alink_interface_;
                        /* The interface to the functionality provided by
                           alink.dll. */
  typedef HRESULT (WINAPI* a_pfn_create_assembly_name_object)(
                                            IAssemblyName **ppAssemblyNameObj,
                                            LPCWSTR       szAssemblyName,
                                            DWORD         dwFlags,
                                            LPVOID        pvReserved);
  a_pfn_create_assembly_name_object
                pfn_create_assembly_name_object_;
                        /* A pointer to the CreateAssemblyNameObject function
                           in fusion.dll. */
  typedef HRESULT (WINAPI* a_pfn_compare_assembly_identity)(
                                LPCWSTR                  pwzAssemblyIdentity1,
                                BOOL                     fUnified1,
                                LPCWSTR                  pwzAssemblyIdentity2,
                                BOOL                     fUnified2,
                                BOOL                     *pfEquivalent,
                                AssemblyComparisonResult *pResult);
  a_pfn_compare_assembly_identity
                pfn_compare_assembly_identity_;
                        /* A pointer to the CompareAssemblyIdentity function
                           in fusion.dll. */
  CComPtr<ICLRMetaHostPolicy>
                clr_metahost_policy_;
                        /* The ICLRMetaHostPolicy interface. */
  CComPtr<ICLRRuntimeInfo>
                clr_runtime_info_;
                        /* The ICLRRuntimeInfo interface. */
  CComPtr<IMetaDataDispenserEx>
                md_dispenser_interface_;
                        /* The IMetaDataDispenserEx interface. */
  CComPtr<IMetaDataEmit2>
                md_emit2_interface_;
                        /* The IMetaDataEmit2 interface. */
  CComQIPtr<IMetaDataImport2, &IID_IMetaDataImport2>
                md_import2_interface_;
                        /* The IMetaDataImport2 interface. */
  mdFile        file_token_;
                        /* The token for current translation unit. */
  bool          initialized_;
                        /* True if the metadata reader was initialized
                           properly. */
};  /* a_metadata_reader */

static unique_ptr<a_metadata_reader> metadata_reader;

bool a_metadata_reader::init_clr_host()
/*
Initialize the clr host.
*/
{
  HRESULT hr;

  hr = CLRCreateInstance(CLSID_CLRMetaHostPolicy, IID_ICLRMetaHostPolicy,
                         reinterpret_cast<LPVOID*>(&clr_metahost_policy_));
  CHECK_API_RESULT(hr, CLRCreateInstance);
  return (clr_metahost_policy_ != nullptr);
}  /* a_metadata_reader::init_clr_host */


bool a_metadata_reader::init_clr_runtime_info()
/*
Initialize the clr runtime info.
*/
{
  HRESULT   hr = E_FAIL;

  check_assertion(clr_metahost_policy_);
  hr = clr_metahost_policy_->GetRequestedRuntime(
                        METAHOST_POLICY_USE_PROCESS_IMAGE_PATH,
                        /*pwzBinary=*/NULL, /*pCfgStream=*/NULL,
                        /*pwzVersion=*/NULL, /*pcchVersion=*/NULL,
                        /*pwzImageVersion=*/NULL, /*pcchImageVersion=*/NULL,
                        /*pdwConfigFlags=*/NULL, IID_ICLRRuntimeInfo,
                        reinterpret_cast<LPVOID*>(&clr_runtime_info_));
  if (FAILED(hr) || clr_runtime_info_ == NULL) {
    const int SIZE = 128;
    WCHAR     version_buffer[SIZE];
    DWORD     version_size = SIZE;

    wcscpy_s(version_buffer, version_size, CLR_FALLBACK_VERSION);
    hr = clr_metahost_policy_->GetRequestedRuntime(
                          static_cast<METAHOST_POLICY_FLAGS>(
                            METAHOST_POLICY_USE_PROCESS_IMAGE_PATH |
                            METAHOST_POLICY_APPLY_UPGRADE_POLICY),
                          /*pwzBinary=*/NULL, /*pCfgStream=*/NULL,
                          version_buffer, &version_size,
                          /*pwzImageVersion=*/NULL, /*pcchImageVersion=*/NULL,
                          /*pdwConfigFlags=*/NULL, IID_ICLRRuntimeInfo,
                          reinterpret_cast<LPVOID*>(&clr_runtime_info_));
    CHECK_API_RESULT(hr, GetRequestedRuntime);
  }  /* if */
  return (clr_runtime_info_ != nullptr);
}  /* a_metadata_reader::init_clr_runtime_info */


bool a_metadata_reader::init_alink()
/*
Initialize the alink interface handle.
*/
{
  HRESULT            hr;

  check_assertion(clr_runtime_info_);
  hr = clr_runtime_info_->LoadLibrary(L"alink.dll", &alink_handle_);
  CHECK_API_RESULT(hr, LoadLibrary);
  if (alink_handle_) {
    typedef HRESULT (WINAPI* a_create_alink_ptr)(REFIID, IUnknown**);
    a_create_alink_ptr create_alink_ptr;

    create_alink_ptr = reinterpret_cast<a_create_alink_ptr>(GetProcAddress(
                                                              alink_handle_,
                                                              "CreateALink"));
    check_assertion(create_alink_ptr);
    hr = (*create_alink_ptr)(IID_IALink,
                             reinterpret_cast<IUnknown**>(&alink_interface_));
    CHECK_API_RESULT(hr, CreateALink);
  } else {
    unexpected_condition_str("ERROR: failed to find/load alink.dll");
  }  /* if */
  return (alink_interface_ != nullptr);
}  /* a_metadata_reader::init_alink */


bool a_metadata_reader::init_assembly_functions()
/*
Uses ICLRRuntimeInfo::GetProcAddress to initialize the function pointers
used to create IAssemblyName objects and compare assembly identities.
*/
{
  HRESULT hr;

  check_assertion(clr_runtime_info_);
  hr = clr_runtime_info_->GetProcAddress(
                "CreateAssemblyNameObject",
                reinterpret_cast<LPVOID*>(&pfn_create_assembly_name_object_));
  CHECK_API_RESULT(hr, GetProcAddress);
  hr = clr_runtime_info_->GetProcAddress(
                  "CompareAssemblyIdentity",
                  reinterpret_cast<LPVOID*>(&pfn_compare_assembly_identity_));
  CHECK_API_RESULT(hr, GetProcAddress);
  return pfn_create_assembly_name_object_ != nullptr &&
         pfn_compare_assembly_identity_ != nullptr;
}  /* a_metadata_reader::init_assembly_functions */


a_qualified_name an_import_scope::name_from_typedef(
           mdTypeDef                       token,
           const a_signature_decoder_scope &scope,
           BYTE                            &generic_parameter_count,
           a_const_class_type_wrapper_ptr  &unresolved_generic_argument) const
{
  a_qualified_name qualified_name;
  wstring          type_name;
  HRESULT          hr;
  DWORD            type_flags;
  BYTE             enclosing_type_generic_param_count = 0;

  generic_parameter_count = get_generic_parameter_count(token);
  hr = import_interface_->GetTypeDefProps(token, type_name, &type_flags,
                                          /*ptkExtends=*/nullptr);
  CHECK_API_RESULT(hr, GetTypeDefProps);
  if (IsTdNested(type_flags)) {
    mdTypeDef enclosing_typedef;
    hr = import_interface_->GetNestedClassProps(token,
                                                &enclosing_typedef);
    CHECK_API_RESULT(hr, GetNestedClassProps);
    qualified_name = name_from_typedef(enclosing_typedef,
                                       scope,
                                       enclosing_type_generic_param_count,
                                       unresolved_generic_argument);
  }  /* if */
  BYTE generic_arity = generic_parameter_count -
                                           enclosing_type_generic_param_count;
  if (generic_arity > 0) {
    /* Non-generic types can contain backticks in their name, so only strip
       the generic arity if it is a generic type. */
    strip_generic_arity(type_name);
  }  /* if */
  if (IsTdNested(type_flags)) {
    qualified_name.append_identifier(move(type_name));
  } else {
    qualified_name = a_qualified_name::from_dotted_name(type_name);
  }  /* if */
  if (generic_arity > 0) {
    auto &generic_arguments = scope.generic_type_arguments();
    auto first_generic_argument_iter = generic_arguments.begin() +
                                           enclosing_type_generic_param_count;
    auto last_generic_argument_iter = first_generic_argument_iter +
                                                                generic_arity;
    if (unresolved_generic_argument == nullptr) {
      (void)find_if(first_generic_argument_iter,
                    last_generic_argument_iter,
                    [&](const a_const_type_wrapper_ptr &generic_argument) {
                      return generic_argument->uses_unresolved_type(
                                                 unresolved_generic_argument);
                    });
    }  /* if */
    qualified_name.append_generic_arguments(first_generic_argument_iter,
                                            last_generic_argument_iter);
  }  /* if */
  return qualified_name;
}  /* an_import_scope::name_from_typedef */


a_const_class_type_wrapper_ptr an_import_scope::type_from_typedef(
                                  const a_signature_decoder_scope &scope,
                                  mdTypeDef                       token) const
{
  a_const_class_type_wrapper_ptr type;

  if (!scope.is_generic_instance_scope()) {
    auto &type_definition = get_type_definition(token);
    type = type_definition.class_type();
  } else {
    BYTE                           generic_parameter_count;
    a_const_class_type_wrapper_ptr unresolved_generic_argument;
    a_class_type_wrapper_ptr       class_type;
    auto                           type_name = name_from_typedef(
                                                 token,
                                                 scope,
                                                 generic_parameter_count,
                                                 unresolved_generic_argument);
    if (unresolved_generic_argument != nullptr) {
      class_type = unresolved_generic_argument->copy()->as_class();
    } else {
      class_type = get_type_definition(token).class_type()->copy()->as_class();
    }  /* if */
    class_type->set_name(type_name);
    type = class_type;
  }  /* if */
  return type;
}  /* an_import_scope::type_from_typedef */


a_const_class_type_wrapper_ptr an_import_scope::type_from_typeref(
                                  const a_signature_decoder_scope &scope,
                                  mdTypeRef                       token) const
{
  a_const_class_type_wrapper_ptr class_type;

  /* First check if the type for this token was cached by a previous call.  If
     it is an instance of a generic type, we can't cache the result because
     the generic arguments may differ in different contexts. */
  bool can_cache = !scope.is_generic_instance_scope();
  auto iter = can_cache ? map_typeref_to_class_type_.lower_bound(token)
                        : map_typeref_to_class_type_.end();
  if (iter == map_typeref_to_class_type_.end() ||
      map_typeref_to_class_type_.key_comp()(token, iter->first)) {
    a_const_class_type_wrapper_ptr enclosing_type;
    wstring                        type_name;
    HRESULT                        hr;
    mdToken                        resolution_scope;
    a_const_import_scope_ptr       resolved_import_scope = nullptr;
    mdTypeRef                      resolved_token = mdTypeDefNil;
    
    hr = import_interface_->GetTypeRefProps(token,
                                            &resolution_scope,
                                            type_name);
    CHECK_API_RESULT(hr, GetTypeRefProps);
    if (IsNilToken(resolution_scope)) {
      /* Exported types are not yet supported.  In this case, there shall be a
         row in the ExportedType table for this Type.  Its Implementation
         field shall contain a File token or an AssemblyRef token that says
         where the type is defined.  */
    } else {
      switch (TypeFromToken(resolution_scope)) {
        case mdtTypeRef:
          { /* The type is a nested type and resolution_scope indicates the
               enclosing type. */
            enclosing_type = type_from_typeref(scope, resolution_scope);
            if (!enclosing_type->is_unresolved_type()) {
              /* We found the typedef for the enclosing type; the nested type
                 will be in the same import scope. */
              resolved_import_scope = enclosing_type->import_scope();
              hr = resolved_import_scope->import_interface()->
                                    FindTypeDefByName(type_name.c_str(),
                                                      enclosing_type->token(),
                                                      &resolved_token);
              CHECK_API_RESULT(hr, FindTypeDefByName);
              check_assertion(!IsNilToken(resolved_token) &&
                              TypeFromToken(resolved_token) == mdtTypeDef);
            } else {
              /* The enclosing type could not be resolved to a typedef. */
            }  /* if */
            break;
          }
        case mdtModuleRef:
          { /* The type is defined in another module within the same
               assembly. */
            wstring module_name;
            hr = import_interface_->GetModuleRefProps(resolution_scope,
                                                      module_name);
            CHECK_API_RESULT(hr, GetModuleRefProps);
            resolved_import_scope = containing_assembly_->
                                       find_import_scope_by_name(module_name);
            if (resolved_import_scope != nullptr) {
              hr = resolved_import_scope->import_interface()->
                                          FindTypeDefByName(type_name.c_str(),
                                                            mdTokenNil,
                                                            &resolved_token);
              CHECK_API_RESULT(hr, FindTypeDefByName);
              check_assertion(!IsNilToken(resolved_token) &&
                              TypeFromToken(resolved_token) == mdtTypeDef);
            } else {
              /* The module containing the type was not found. */
              unexpected_condition();
            }  /* if */
            break;
          }
        case mdtModule:
          /* The type is defined in the current module. */
          resolved_import_scope = this;
          hr = import_interface_->FindTypeDefByName(type_name.c_str(),
                                                    mdTokenNil,
                                                    &resolved_token);
          CHECK_API_RESULT(hr, FindTypeDefByName);
          check_assertion(!IsNilToken(resolved_token) &&
                          TypeFromToken(resolved_token) == mdtTypeDef);
          break;
        case mdtAssemblyRef:
          { /* The type is defined in a different assembly. */
            an_assembly_name assembly_name;
            an_assembly_ptr  assembly;
            assembly_name = containing_assembly_->
                                      get_assembly_ref_name(resolution_scope);
            assembly = metadata_reader->find_assembly_by_name(assembly_name);
            if (assembly != nullptr) {
              /* The assembly containing this type was found. */
              if (assembly->find_type_by_name(type_name,
                                              resolved_import_scope,
                                              resolved_token)) {
                check_assertion(!IsNilToken(resolved_token) &&
                                TypeFromToken(resolved_token) == mdtTypeDef);
              } else {
                /* The type was not found in the assembly. */
              }  /* if */
            } else {
              /* The assembly containing this type was not found. */
            }  /* if */
            break;
          }
        default:
          unexpected_condition();
          break;
      }  /* switch */
    }  /* if */
    if (resolved_import_scope != nullptr) {
      check_assertion(!IsNilToken(resolved_token) &&
                      TypeFromToken(resolved_token) == mdtTypeDef);
      class_type = resolved_import_scope->type_from_typedef(scope,
                                                            resolved_token);
    } else {
      /* The type was not found.  Get the name of the type to display in the
         resulting error message.  For generic type instances, only the type
         name itself is used, as the fully-qualified name can't be reliably
         formed because the "`arity" encoding at the end of names is
         optional. */
      a_qualified_name name;
      if (scope.is_generic_instance_scope()) {
        strip_generic_arity(type_name);
      }  /* if */
      if (enclosing_type != nullptr) {
        if (!scope.is_generic_instance_scope()) {
          name = enclosing_type->name();
        }  /* if */
        name.append_identifier(move(type_name));
      } else {
        name = a_qualified_name::from_dotted_name(type_name);
      }  /* if */
      class_type = make_shared<a_class_type_wrapper>(
                                          a_class_type_wrapper::ck_unresolved,
                                          name, this, token);
    }  /* if */
    if (can_cache) {
      map_typeref_to_class_type_.emplace_hint(iter, token, class_type);
    }  /* if */
  } else {
    class_type = iter->second;
  }  /* if */
  return class_type;
}  /* an_import_scope::type_from_typeref */


a_const_class_type_wrapper_ptr an_import_scope::type_from_token(
                                  const a_signature_decoder_scope &scope,
                                  mdToken                         token) const
/*
Resolve the type given by the token and return its qualified name.
*/
{
  a_const_class_type_wrapper_ptr class_type;
  HRESULT                  hr;
  switch (TypeFromToken(token)) {
    case mdtTypeDef:
      class_type = type_from_typedef(scope, token);
      break;
    case mdtTypeRef:
      class_type = type_from_typeref(scope, token);
      break;
    case mdtInterfaceImpl:
      /* If this is an interface-impl token then use the token for the
         corresponding interface. */
      hr = import_interface_->GetInterfaceImplProps(token,
                                                    /*mdTypeDef=*/nullptr,
                                                    &token);
      CHECK_API_RESULT(hr, GetInterfaceImplProps);
      class_type = type_from_token(scope, token);
      break;
    case mdtTypeSpec:
      { PCCOR_SIGNATURE signature;
        ULONG           bytes_in_signature;
        hr = import_interface_->GetTypeSpecFromToken(token, &signature,
                                                     &bytes_in_signature);
        CHECK_API_RESULT(hr, GetTypeSpecFromToken);
        a_type_wrapper_ptr type = a_signature_decoder::decode_type(
                                             scope,
                                             signature, bytes_in_signature);
        if (type->is_invalid()) {
          class_type = make_shared<a_class_type_wrapper>();
        } else if (type->is_of_kind(a_type_wrapper::twk_class)) {
          class_type = type->as_class();
        } else if (type->is_of_kind(a_type_wrapper::twk_indirection)) {
          /* Ref class types are decoded as handle types, so we must get the
             class type from the underlying type. */
          auto indirection = type->as_indirection();
          check_assertion(indirection->is_of_indirection_kind(
                                             a_type_indirection::tik_handle));
          class_type = indirection->underlying_type()->as_class();
        }  /* if */
        check_assertion(class_type != nullptr);
        break;
      }  /* case mdtTypeSpec */
    default:
      unexpected_condition();
      break;
  }  /* switch */
  return class_type;
}  /* an_import_scope::type_from_token */


a_type_wrapper_ptr a_custom_attribute_data::read_serialized_type_and_advance()
{
  a_type_wrapper_ptr   type;
  CorSerializationType serialization_type;

  read_and_advance(serialization_type);
  if (serialization_type == SERIALIZATION_TYPE_FIELD ||
      serialization_type == SERIALIZATION_TYPE_PROPERTY) {
    /* The CLI allows fields and properties to have the same name, so two
       serialization types are encoded in succession for named arguments.  The
       first one provides a means to disambiguate them and the second one is
       the type of the field or property. */
    read_and_advance(serialization_type);
  }  /* if */
  switch (serialization_type) {
    case SERIALIZATION_TYPE_BOOLEAN:
    case SERIALIZATION_TYPE_I1:
    case SERIALIZATION_TYPE_U1:
    case SERIALIZATION_TYPE_I2:
    case SERIALIZATION_TYPE_U2:
    case SERIALIZATION_TYPE_I4:
    case SERIALIZATION_TYPE_U4:
    case SERIALIZATION_TYPE_I8:
    case SERIALIZATION_TYPE_U8:
    case SERIALIZATION_TYPE_R4:
    case SERIALIZATION_TYPE_R8:
    case SERIALIZATION_TYPE_STRING:
    case SERIALIZATION_TYPE_TAGGED_OBJECT:
        type = a_type_wrapper::create(serialization_type);
        break;
    case SERIALIZATION_TYPE_CHAR:
      { auto import_flags =
                        import_scope_->containing_assembly().import_flags();
        bool builtin_wchar_t =
                           (import_flags & cpp_cli_wchar_t_is_keyword) != 0;
        type = a_type_wrapper::create(serialization_type, builtin_wchar_t);
        break;
      }
    case SERIALIZATION_TYPE_SZARRAY:
      { /* The argument is an array type.  Determine is underlying type. */
        auto underlying_type = read_serialized_type_and_advance();
        /* Create the System::Array^ wrapper. */
        type = an_array_type_wrapper::create_handle_to_array(underlying_type);
        break;
      }
    case SERIALIZATION_TYPE_TYPE:
      { /* Create the System::Type^ wrapper. */
        auto type_type = a_class_type_wrapper::create_system_class(L"Type");
        type = make_shared<a_type_indirection>(a_type_indirection::tik_handle,
                                               move(type_type));
        break;
      }
    case SERIALIZATION_TYPE_ENUM:
      { /* The argument is an enum type. */
        auto named_type = read_named_type_and_advance();
        check_assertion(named_type != nullptr);
        type = named_type->copy();
        break;
      }
      break;
    case SERIALIZATION_TYPE_FIELD:
    case SERIALIZATION_TYPE_PROPERTY:
    default:
      unexpected_condition();
      break;
  }  /* switch */
  return type;
}  /* a_custom_attribute_data::read_serialized_type_and_advance */


a_const_class_type_wrapper_ptr
a_custom_attribute_data::read_named_type_and_advance()
{
  a_const_class_type_wrapper_ptr type;
  unique_ptr<wstring>            type_name_ptr;
  read_and_advance(type_name_ptr);
  if (type_name_ptr != nullptr) {
    type = metadata_reader->find_type_by_name(
                                       *type_name_ptr,
                                       &import_scope_->containing_assembly());
  }  /* if */
  return type;
}  /* a_custom_attribute_data::read_named_type_and_advance */


class DECLSPEC_UUID("5F535F97-A4B2-4A1A-B54A-1E05CFD14A80")
CPPMetadataDispenser;

/* Linker-provided pseudo variable that represents the DOS header of the
   module. */
extern "C" IMAGE_DOS_HEADER __ImageBase;

HINSTANCE relative_load_library(const wchar_t *relative_path)
/*
Utility to load a DLL relative to the location of the module that contains
this function.
*/
{
  HINSTANCE module_instance = reinterpret_cast<HINSTANCE>(&__ImageBase);
  HINSTANCE library_instance = nullptr;
  wchar_t   module_path[MAX_PATH];
  wchar_t   drive[_MAX_DRIVE];
  wchar_t   dir[MAX_PATH];

  check_assertion(relative_path != nullptr &&
                  *relative_path != '\0');
  if (GetModuleFileNameW(module_instance, module_path,
                         _countof(module_path)) != 0) {
    if (_wsplitpath_s(module_path, drive, _MAX_DRIVE, dir, MAX_PATH,
                      /*fname=*/NULL, /*fname_count=*/0, /*ext=*/NULL,
                      /*ext_count=*/0) == 0) {
      wcscpy_s(module_path, drive);
      wcscat_s(module_path, dir);
      if (*relative_path == '\\') ++relative_path;
      wcscat_s(module_path, relative_path);
      library_instance = LoadLibraryW(module_path);
    }  /* if */
  }  /* if */
  if (library_instance == nullptr) {
    unexpected_condition_str("can't find vcmeta.dll in relative path");
  }  /* if */
  return library_instance;
}  /* relative_load_library */


bool a_metadata_reader::init_metadata_interfaces()
/*
Initialize various metadata interfaces.
*/
{
  HRESULT hr;

  check_assertion(clr_runtime_info_);
  check_assertion(alink_interface_);
  if (cppcx_enabled) {
    HINSTANCE vcmeta_module;
    if (vcmeta_directory_name == nullptr) {
      /* Search for vcmeta.dll relative to this module. */
      vcmeta_module = relative_load_library(L"vcmeta.dll");
    } else {
      /* Search for vcmeta.dll in a directory specified by the user. */
      wchar_t   module_path[MAX_PATH];
      size_t    convertedChars = 0;  
      if (mbstowcs_s(&convertedChars, module_path, MAX_PATH,
                     vcmeta_directory_name, _TRUNCATE) != 0) {
        unexpected_condition_str("path for vcmeta.dll is too long");
      }  /* if */
      wcscat_s(module_path, L"\\vcmeta.dll");
      vcmeta_module = LoadLibraryW(module_path);
    }  /* if */
    if (vcmeta_module == nullptr) {
      hr = E_FAIL;
      unexpected_condition_str("failed to find suitable vcmeta.dll");
    } else {
      LPFNGETCLASSOBJECT vcmeta_get_class_object =
            reinterpret_cast<LPFNGETCLASSOBJECT>(GetProcAddress(
                                                        vcmeta_module,
                                                        "DllGetClassObject"));
      if (vcmeta_get_class_object == nullptr) {
        hr = E_FAIL;
        CHECK_API_RESULT(hr, GetProcAddress);
      } else {
        CComPtr<IClassFactory> vcmeta_class_factory;
        hr = (*vcmeta_get_class_object)(
                         __uuidof(CPPMetadataDispenser),
                         __uuidof(IClassFactory),
                         reinterpret_cast<void **>(&vcmeta_class_factory));
        CHECK_API_RESULT(hr, DllGetClassObject);
        if (SUCCEEDED(hr)) {
          hr = vcmeta_class_factory->CreateInstance(
                         /*pUnkOuter=*/nullptr,
                         IID_IMetaDataDispenserEx,
                         reinterpret_cast<void **>(&md_dispenser_interface_));
          CHECK_API_RESULT(hr, CreateInstance);
        }  /* if */
      }  /* if */
      if (FAILED(hr)) {
        FreeLibrary(vcmeta_module);
      }  /* if */
    }  /* if */
  } else {
    hr = clr_runtime_info_->GetInterface(CLSID_CorMetaDataDispenser,
                                         IID_IMetaDataDispenserEx,
                                         reinterpret_cast<LPVOID*>(
                                                   &md_dispenser_interface_));
    CHECK_API_RESULT(hr, GetInterface);
  }  /* if */
  if (SUCCEEDED(hr)) {
    hr = md_dispenser_interface_->DefineScope(CLSID_CLR_v2_MetaData, 0,
                                              IID_IMetaDataEmit2,
                                              reinterpret_cast<IUnknown **>(
                                                       &md_emit2_interface_));
    CHECK_API_RESULT(hr, DefineScope);
    if (SUCCEEDED(hr)) {
      md_import2_interface_ = md_emit2_interface_;
      if (md_import2_interface_ != nullptr) {
        hr = alink_interface_->Init(md_dispenser_interface_, nullptr);
        CHECK_API_RESULT(hr, Init);
      } else {
        hr = E_NOINTERFACE;
        CHECK_API_RESULT(hr, QueryInterface);
      }  /* if */
    }  /* if */
  }  /* if */
  return SUCCEEDED(hr);
}  /* a_metadata_reader::init_metadata_interfaces */


bool a_metadata_reader::initialize()
/*
Initialize the metadata reader - this means initializing clr host/runtime,
loading and initializing alink.dll, and creating the various metadata reading
interfaces.
*/
{
  bool result;

  if (init_clr_host() && init_clr_runtime_info() && init_alink() &&
      init_assembly_functions() && init_metadata_interfaces()) {
    result = true;
  } else {
    result = false;
  }  /* if */
  return result;
}  /* a_metadata_reader::initialize */


bool a_metadata_reader::trans_unit_init(a_const_char *tu_file_name)
/*
Do per-translation unit initialization.  This resets the assembly
index, sets the name of the translation unit, and retrieves a file token for
the translation unit.
*/
{
  HRESULT hr;
  wstring input_file(char_string_to_wstring(tu_file_name));

  check_assertion(is_initialized());
  assemblies_.clear();
  hr = alink_interface_->AddFile(AssemblyIsUBM, input_file.c_str(),
                                 ffContainsMetaData, md_emit2_interface_,
                                 &file_token_);
  CHECK_API_RESULT(hr, AddFile);
  return SUCCEEDED(hr);
}  /* a_metadata_reader::trans_unit_init */


void a_metadata_reader::trans_unit_wrapup()
/*
Perform clean up for the translation unit.  This involves freeing
the all the assemblies that were imported and resetting the assembly
index.
*/
{
  if (alink_interface_ != nullptr) {
    HRESULT hr = alink_interface_->CloseAssembly(AssemblyIsUBM);
    CHECK_API_RESULT(hr, CloseAssembly);
  }  /* if */
  assemblies_.clear();
}  /* a_metadata_reader::trans_unit_wrapup */


an_assembly_index a_metadata_reader::import_assembly(
                                a_const_char              *full_assembly_path,
                                a_cpp_cli_import_flag_set import_flags,
                                bool                      *is_duplicated)
/*
Import a single assembly.  full_assembly_path contains the full path to the
assembly to be processed.  If this assembly has been processed, *is_duplicated
is set to TRUE and the previous assembly index is returned.
*/
{
  wstring           assembly_path(char_string_to_wstring(full_assembly_path));
  an_assembly_index assembly_index = find_assembly_by_path(assembly_path);

  *is_duplicated = assembly_index != 0;
  if (!*is_duplicated) {
    assembly_index = (an_assembly_index)assemblies_.size() + 1;
    auto assembly = make_unique_ptr<an_assembly>(assembly_path,
                                                 assembly_index,
                                                 alink_interface_,
                                                 file_token_,
                                                 import_flags);
    assembly->process();
    assemblies_.emplace_back(move(assembly));
  }  /* if */
  return assembly_index;
}  /* a_metadata_reader::import_assembly */


void a_metadata_reader::import_class_definition(
                                 ostringstream           &buffer,
                                 an_assembly_scope_index assembly_scope_index,
                                 a_cpp_cli_token         typedef_token,
                                 bool                    class_body_only,
                                 a_boolean               *is_delegate)
/*
Import the definition of the class specified by the provided assembly scope
index and typedef token.

If class_body_only is true, the class head and the namespace scopes will be
omitted.

If is_delegate is non-NULL, *is_delegate is set to TRUE if the class is a
delegate and to FALSE otherwise.
*/
{
  auto assembly_index = assembly_index_from_assembly_scope_index(
                                                        assembly_scope_index);
  auto scope_index = scope_index_from_assembly_scope_index(
                                                        assembly_scope_index);
  auto &assembly = assembly_from_index(assembly_index);
  auto &import_scope = assembly.import_scope_from_index(scope_index);
  import_scope.import_one_type(buffer, typedef_token,
                               /*at_top_level=*/true,
                               /*want_definition=*/true,
                               class_body_only,
                               /*pending_constraint_types=*/nullptr,
                               is_delegate);
}  /* a_metadata_reader::import_class_definition */


an_assembly_name::an_assembly_name(wstring reference_name)
{
  if (!reference_name.empty()) {
    HRESULT  hr;
    hr = metadata_reader->create_assembly_name_object(
                                                     &name_interface_,
                                                     reference_name.c_str(),
                                                     CANOF_PARSE_DISPLAY_NAME,
                                                     /*pvReserved*/nullptr);
    CHECK_API_RESULT(hr, create_assembly_name_object);
    init_display_name();
  }  /* if */
}  /* an_assembly_name constructor */


an_assembly_name::an_assembly_name(
                          wstring                name,
                          const ASSEMBLYMETADATA &data,
                          const void             *public_key_or_token,
                          ULONG                  bytes_in_public_key_or_token,
                          DWORD                  flags)
{
  HRESULT hr;

  /* Create an IAssemblyName object. */
  hr = metadata_reader->create_assembly_name_object(&name_interface_,
                                                    /*assembly_name*/nullptr,
                                                    0,
                                                    /*pvReserved*/nullptr);
  CHECK_API_RESULT(hr, create_assembly_name_object);
  /* Set the name. */
  hr = name_interface_->SetProperty(ASM_NAME_NAME,
                                    const_cast<wchar_t*>(name.c_str()),
                                    (DWORD)((name.length() + 1) *
                                                              sizeof(WCHAR)));
  CHECK_API_RESULT(hr, SetProperty);
  /* Set the version. */
  hr = name_interface_->SetProperty(ASM_NAME_MAJOR_VERSION,
                                    const_cast<USHORT*>(&data.usMajorVersion),
                                    sizeof(data.usMajorVersion));
  CHECK_API_RESULT(hr, SetProperty);
  hr = name_interface_->SetProperty(ASM_NAME_MINOR_VERSION,
                                    const_cast<USHORT*>(&data.usMinorVersion),
                                    sizeof(data.usMinorVersion));
  CHECK_API_RESULT(hr, SetProperty);
  hr = name_interface_->SetProperty(ASM_NAME_BUILD_NUMBER,
                                    const_cast<USHORT*>(&data.usBuildNumber),
                                    sizeof(data.usBuildNumber));
  CHECK_API_RESULT(hr, SetProperty);
  hr = name_interface_->SetProperty(
                                  ASM_NAME_REVISION_NUMBER,
                                  const_cast<USHORT*>(&data.usRevisionNumber),
                                  sizeof(data.usRevisionNumber));
  CHECK_API_RESULT(hr, SetProperty);
  /* Set the culture.  Note: the cbLocale member is misnamed; it really
     represents the character count. */
  LPVOID locale = data.szLocale;
  DWORD characters_in_locale = data.cbLocale;
  if (data.szLocale == nullptr) {
    check_assertion(data.cbLocale == 0);
    locale = (LPVOID)L"";
    characters_in_locale = _countof(L"");
  }  /* if */
  hr = name_interface_->SetProperty(ASM_NAME_CULTURE, locale,
                                    characters_in_locale * sizeof(WCHAR));
  CHECK_API_RESULT(hr, SetProperty);
  /* Set the public key or token. */
  if (bytes_in_public_key_or_token == 0) {
    hr = name_interface_->SetProperty(ASM_NAME_NULL_PUBLIC_KEY_TOKEN,
                                     /*pvProperty=*/nullptr,
                                     /*cbProperty=*/0);
  } else if (!(flags & afPublicKey)) {
    hr = name_interface_->SetProperty(ASM_NAME_PUBLIC_KEY_TOKEN,
                                      const_cast<void*>(public_key_or_token),
                                      bytes_in_public_key_or_token);
  } else {
    hr = name_interface_->SetProperty(ASM_NAME_PUBLIC_KEY,
                                      const_cast<void*>(public_key_or_token),
                                      bytes_in_public_key_or_token);
  }  /* if */
  CHECK_API_RESULT(hr, SetProperty);
  /* Set if the name is retargetable. */
  if (flags & afRetargetable) {
    BOOL is_retargetable = TRUE;
    hr = name_interface_->SetProperty(ASM_NAME_RETARGET, &is_retargetable,
                                      sizeof(is_retargetable));
    CHECK_API_RESULT(hr, SetProperty);
  }  /* if */
  init_display_name();
}  /* an_assembly_name constructor. */


bool an_assembly_name::reference_matches_definition(
                                const an_assembly_name &definition_name) const
/*
Returns TRUE if this assembly reference name matches definition_name.
*/
{
  bool result = display_name() == definition_name.display_name();

  if (!result) {
    BOOL                     equivalent;
    AssemblyComparisonResult comparison_result;
    HRESULT hr = metadata_reader->compare_assembly_identity(
                                       display_name().c_str(),
                                       /*fUnified1=*/TRUE,
                                       definition_name.display_name().c_str(),
                                       /*fUnified2=*/TRUE,
                                       &equivalent,
                                       &comparison_result);
    result = SUCCEEDED(hr) && equivalent;
  }  /* if */
  return result;
}  /* an_assembly_name::reference_matches_definition */
/*
Interface functions to the front end proper.
*/

static a_boolean ms_metadata_init_if_needed()
/*
Helper function to initialize the metadata reader.
*/
{
  a_boolean result = TRUE;

  /* Reset the index counter. */
  if (metadata_reader == nullptr) {
    metadata_reader = make_unique_ptr<a_metadata_reader>();
    if (!metadata_reader->is_initialized()) {
      metadata_reader.reset();
      result = FALSE;
    }  /* if */
  }  /* if */
  return result;
}  /* ms_metadata_init_if_needed */

#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES

static a_portable_assembly_table_entry
                *pa_table = nullptr;
                        /* A dynamically-allocated list of entries used to
                           refer to metadata tokens and their associated
                           strings. */

static uint32_t pa_table_entries = 0;
                        /* The number of allocated entries in pa_table. */

static a_text_buffer_ptr
		metadata_string_buffer = nullptr;
			/* Text buffer used by create_portable_assembly. */

void an_assembly::create_portable_assembly(a_const_char *assembly_name)
/*
Create a "portable assembly" for this assembly.  A portable assembly contains
all of the metadata information contained by the assembly, but in string
format so that it can be used on non-Windows systems (for testing purposes).
assembly_name determines the file name for the generated portable assembly.
*/
{
  a_portable_assembly_header      header;
  a_portable_assembly_table_entry *table;
  string                          pa_name(start_of_file_name(assembly_name));
  FILE                            *f_pa;
  uint32_t                        cur_entry_no = 0;
  HCORENUM                        enum_typedefs = 0;
  mdTypeDef                       typedefs[64];
  ULONG                           count_of_typedefs;
  a_text_buffer_ptr               buffer;
  sizeof_t                        size;
  uint32_t                        i;

  /* The portable assembly is written in the current directory (without
     regard to an existing file).  This is usually okay because the assemblies
     we're concerned with are typically in system directories. */
  f_pa = fopen_with_error((char *)pa_name.c_str(), "wb", OFF_NO_OPTIONS,
                          ec_portable_assembly);
  if (f_pa != nullptr) {
    /* Write a dummy header to the file initially (will be overwritten at
       the end with the proper information). */
    clear_portable_assembly_header(&header);
    /* Write the header in ASCII so it's more portable.  Use a format string
       that will result in the same number of bytes being used when the
       header is later re-written (so the offsets will stay the same). */
    (void)fprintf(f_pa, PORTABLE_ASSEMBLY_HEADER_FORMAT,
                  header.magic, header.num_entries, header.table_offset);
    /* Get the string returned by import_all_types. */
    if (metadata_string_buffer == nullptr) {
      /* Allocate the buffer that will temporarily house the metadata string
         information. */
      metadata_string_buffer =
               alloc_text_buffer(METADATA_IMPORT_BUFFER_ALLOCATION_INCREMENT);
      expand_text_buffer(metadata_string_buffer, METADATA_IMPORT_BUFFER_SIZE);
    }  /* if */
    reset_text_buffer(metadata_string_buffer);
    buffer = metadata_string_buffer;
    size = buffer->allocated_size;
    import_all_types(buffer->buffer, &size);
    if (size <= buffer->allocated_size) {
      /* The buffer fits.  Mark the size that has been written. */
      buffer->size = size;
    } else {
      /* Expand the buffer */
      reset_text_buffer(buffer);
      expand_text_buffer(buffer, size);
      import_all_types(buffer->buffer, &size);
      check_assertion(size <= buffer->allocated_size);
      buffer->size = size;
    }  /* if */
    if (pa_table == nullptr) {
      /* 2780 entries are needed for mscorlib, so allocate enough so to handle
         that (most likely) case. */
      pa_table_entries = 3000;
      pa_table = (a_portable_assembly_table_entry *)alloc_resizable_buffer(
                         (sizeof_t)(pa_table_entries *
                                    sizeof(a_portable_assembly_table_entry)));
    }  /* if */
    /* The import_all_types string is pointed to by the first entry in the
       table.  Keep the offset and size information, then write the string
       to the portable assembly file. */
    pa_table[0].scope_index = 0;
    pa_table[0].token = 0;
    pa_table[0].offset = ftell(f_pa);
    pa_table[0].size = (uint32_t)buffer->size;
    (void)fwrite((a_stdio_arg)buffer->buffer, 1, buffer->size, f_pa);
    /* For each typedef in the assembly, get its definition (in case we ever
       need it) and write it to the portable assembly. */
    for (auto &import_scope : imported_scopes_) {
      an_import_interface_ptr
                          import_interface = import_scope->import_interface();
      do {
        HRESULT hr = import_interface->EnumTypeDefs(&enum_typedefs,
                                                    typedefs,
                                                    _countof(typedefs),
                                                    &count_of_typedefs);
        CHECK_API_RESULT(hr, EnumTypeDefs);
        check_assertion(count_of_typedefs <= _countof(typedefs));
        for (ULONG i = 0; i < count_of_typedefs; ++i) {
          a_boolean  is_delegate;
          check_assertion(typedefs[i] != 0);
          reset_text_buffer(metadata_string_buffer);
          buffer = metadata_string_buffer;
          size = buffer->allocated_size;
          import_class_definition(import_scope->assembly_scope_index(),
                                  typedefs[i], buffer->buffer, &size,
                                  &is_delegate);
          if (size <= buffer->allocated_size) {
            /* The buffer fits.  Mark the size that has been written. */
            buffer->size = size;
          } else {
            /* Expand the buffer */
            reset_text_buffer(buffer);
            expand_text_buffer(buffer, size);
            import_class_definition(import_scope->assembly_scope_index(),
                                    typedefs[i], buffer->buffer, &size,
                                    &is_delegate);
            check_assertion(size <= buffer->allocated_size);
            buffer->size = size;
          }  /* if */
          if (++cur_entry_no > pa_table_entries-1) {
            /* Double the size of the table if we've run out of space. */
            sizeof_t old_size = pa_table_entries *
                                sizeof(a_portable_assembly_table_entry);
            pa_table_entries *= 2;
            pa_table = (a_portable_assembly_table_entry *)realloc_buffer(
                                       (char *)pa_table, old_size, old_size*2);
          }  /* if */
          table = &pa_table[cur_entry_no];
          table->scope_index = import_scope->scope_index();
          table->token = typedefs[i];
          table->offset = ftell(f_pa);
          table->size = (uint32_t)buffer->size;
          if (is_delegate) {
            /* Add a prefix to delegates so we can produce the "is_delegate"
               flag when calling the version of import_class_definition for
               portable assemblies (see host_envir.c). */
            (void)fwrite((a_stdio_arg)PORTABLE_ASSEMBLY_DELEGATE_PREFIX, 1,
                         sizeof(PORTABLE_ASSEMBLY_DELEGATE_PREFIX), f_pa);
            table->size += sizeof(PORTABLE_ASSEMBLY_DELEGATE_PREFIX);
          }  /* if */
          (void)fwrite((a_stdio_arg)buffer->buffer, 1, buffer->size, f_pa);
        }  /* for */
      } while (count_of_typedefs > 0);
      import_interface->CloseEnum(enum_typedefs);
    }  /* for */
    /* Increment to account for zeroth entry. */
    cur_entry_no++;
    /* Initialize the header now that we know the proper information. */
    header.magic = PORTABLE_ASSEMBLY_MAGIC_NUMBER;
    header.num_entries = cur_entry_no;
    header.table_offset = ftell(f_pa);
    /* Add a newline before the table contents. */
    (void)fputc('\n', f_pa);
    /* Write out the table contents. */
    for (i = 0; i < cur_entry_no; i++) {
      (void)fprintf(f_pa, PORTABLE_ASSEMBLY_TABLE_FORMAT,
                    pa_table[i].scope_index, pa_table[i].token,
                    pa_table[i].offset, pa_table[i].size);
    }  /* for */
    /* Re-write the header with the proper information now that it is
       known. */
    (void)fseek(f_pa, 0L, SEEK_SET);
    (void)fprintf(f_pa, PORTABLE_ASSEMBLY_HEADER_FORMAT,
                  header.magic, header.num_entries, header.table_offset);
    (void)fclose(f_pa);
  }  /* if */
}  /* create_portable_assembly */

#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES */

extern
an_assembly_index import_metadata_file(
                                a_const_char              *full_assembly_path,
                                a_cpp_cli_import_flag_set import_flags,
                                a_boolean                 *is_duplicated)
/*
Import the assembly and return an unique assembly index.  *is_duplicated is
set to true if this assembly have been imported before.  If so, the previous
assembly will be returned.
*/
{
  an_assembly_index result;
  bool              is_dup= false;

  check_assertion(is_duplicated);
  check_assertion(metadata_reader->is_initialized());
  *is_duplicated = FALSE;
  /* Attempt to import this assembly. */
  result = metadata_reader->import_assembly(full_assembly_path, import_flags,
                                            &is_dup);
  *is_duplicated = is_dup ? TRUE : FALSE;
#if WRITE_CPPCLI_PORTABLE_ASSEMBLIES
  if (generate_portable_assemblies && result != 0) {
    /* Create a portable assembly. */
    an_assembly &assembly = metadata_reader->assembly_from_index(result);
    assembly.create_portable_assembly(full_assembly_path);
  }  /* if */
#endif /* WRITE_CPPCLI_PORTABLE_ASSEMBLIES */
  return result;
}  /* import_metadata_file */


extern
void import_all_types(an_assembly_index assembly_index,
                      char              *buffer,
                      size_t            *buffer_size)
/*
Import all the types defined in the specified assembly.  *buffer_size
describes the allocated size of *buffer.  If there is enough space, the
generated code is returned in *buffer and the amount of buffer used is
returned in *buffer_size.  Otherwise, *buffer is null terminated and
*buffer_size contains the required size.
*/
{
  check_assertion(metadata_reader != nullptr);
  metadata_reader->import_all_types(assembly_index, buffer, buffer_size);
}  /* import_all_types */


void an_assembly::import_all_types(char   *buffer,
                                   size_t *buffer_size)
/*
Import all the types defined in the assembly.  *buffer_size describes the
allocated size of *buffer.  If there is enough space, the generated code is
returned in *buffer and the amount of buffer used is returned in *buffer_size.
Otherwise, *buffer is null terminated and *buffer_size contains the required
size.
*/
{
  string        str;
  ostringstream os;

  import_all_types(os);
  str = os.str();
  /* '+1' for the NULL terminator. */
  if (str.size() + 1 <= *buffer_size) {
    /* The buffer fits.  Copy it. */
    strcpy_s(buffer, *buffer_size, str.c_str());
  } else if (*buffer_size > 0) {
    /* Not enough space.  Terminate the buffer. */
    *buffer = '\0';
  }  /* if */
  /* '+1' for the NULL terminator. */
  *buffer_size = str.size() + 1;
}  /* an_assembly::import_all_types */


extern
void import_class_definition(an_assembly_scope_index assembly_scope_index,
                             a_cpp_cli_token         typedef_token,
                             char                    *buffer,
                             size_t                  *buffer_size,
                             a_boolean               *is_delegate)
/*
Import the definition of the type specified by typedef_token.  The generated
code only contains the body of the class definition, including the base classes
list.  The namespace scopes and class head are omitted.
*/
{
  string str;
  ostringstream os;

  check_assertion(metadata_reader != nullptr);
  check_assertion(metadata_reader->is_initialized());
  metadata_reader->import_class_definition(os, assembly_scope_index,
                                           typedef_token,
                                           /*class_body_only=*/true,
                                           is_delegate);
  str = os.str();
  /* '+1' for the NULL terminator. */
  if (str.size() + 1 <= *buffer_size) {
    strcpy_s(buffer, *buffer_size, str.c_str());
  } else if (*buffer_size > 0) {
    *buffer = '\0';
  }  /* if */
  /* '+1' for the NULL terminator. */
  *buffer_size = str.size() + 1;
}  /* import_class_definition */


extern
void ms_metadata_trans_unit_init(a_const_char *tu_file_name)
/*
Reset the metadata reader for reading metadata for the next translation unit.
*/
{
  if (ms_metadata_init_if_needed()) {
    (void)metadata_reader->trans_unit_init(tu_file_name);
  } else {
    catastrophe(ec_ms_metadata_init_failed);
  }  /* if */
  is_cppcx_metadata = false;
}  /* ms_metadata_trans_unit_init */


extern
void ms_metadata_trans_unit_wrapup()
/*
Reset the metadata reader for the next translation unit.  This clears all
imported assemblies.
*/
{
  if (metadata_reader != nullptr) {
    metadata_reader->trans_unit_wrapup();
  }  /* if */
}  /* ms_metadata_trans_unit_wrapup */


extern void ms_metadata_cleanup()
/*
Cleanup.  Free all memory and release the interfaces.
*/
{
  metadata_reader.reset();
}  /* ms_metadata_cleanup */

/* Conditionally close the "edg" namespace. */
END_EDG_NAMESPACE

#endif /* CPPCLI_ENABLING_POSSIBLE && !defined(_lint) */

