/*=============================================================================
    Wave: A Standard compliant C++ preprocessor library

    Re2C based C++ lexer
    
    Copyright (c) 2001-2004 Hartmut Kaiser
    http://spirit.sourceforge.net/

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(CPP_RE2C_LEXER_HPP_B81A2629_D5B1_4944_A97D_60254182B9A8_INCLUDED)
#define CPP_RE2C_LEXER_HPP_B81A2629_D5B1_4944_A97D_60254182B9A8_INCLUDED

#include <string>
#include <cstdio>
#include <cstdarg>
#if defined(BOOST_SPIRIT_DEBUG)
#include <iostream>
#endif // defined(BOOST_SPIRIT_DEBUG)

#include <boost/concept_check.hpp>
#include <boost/assert.hpp>
#include <boost/spirit/core.hpp>

#include <boost/wave/wave_config.hpp>
#include <boost/wave/language_support.hpp>
#include <boost/wave/token_ids.hpp>
#include <boost/wave/util/file_position.hpp>
#include <boost/wave/cpplexer/validate_universal_char.hpp>
#include <boost/wave/cpplexer/cpplexer_exceptions.hpp>
#include <boost/wave/cpplexer/token_cache.hpp>

#include <boost/wave/cpplexer/cpp_lex_token.hpp>
#include <boost/wave/cpplexer/cpp_lex_interface.hpp>
#include <boost/wave/cpplexer/re2clex/scanner.hpp>
#include <boost/wave/cpplexer/re2clex/cpp.re.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace boost {
namespace wave {
namespace cpplexer {
namespace re2clex {

///////////////////////////////////////////////////////////////////////////////
// 
//  encapsulation of the re2c based cpp lexer
//
///////////////////////////////////////////////////////////////////////////////

template <typename IteratorT, typename PositionT = boost::wave::util::file_position_t>
class lexer 
{
public:

    typedef char                        char_t;
    typedef Scanner                     base_t;
    typedef lex_token<PositionT>        token_t;
    typedef typename token_t::string_t  string_t;
    
    lexer(IteratorT const &first, IteratorT const &last, 
        PositionT const &pos, boost::wave::language_support language);
    ~lexer();

    lex_token<PositionT> get();
    void set_position(PositionT const &pos)
    {
        filename = pos.get_file();
        scanner.line = pos.get_line();
        scanner.file_name = filename.c_str();
    }

// error reporting from the re2c generated lexer
    static int report_error(Scanner *s, char *, ...);

private:
    static char const *tok_names[];
    
    Scanner scanner;
    string_t filename;
    string_t value;
    bool at_eof;
    
    token_cache<string_t> token_cache;
};

///////////////////////////////////////////////////////////////////////////////
// initialize cpp lexer 
template <typename IteratorT, typename PositionT>
inline
lexer<IteratorT, PositionT>::lexer(IteratorT const &first, 
        IteratorT const &last, PositionT const &pos, 
        boost::wave::language_support language) 
:   filename(pos.get_file()), at_eof(false)
{
    memset(&scanner, '\0', sizeof(Scanner));
    scanner.fd = -1;
    scanner.eol_offsets = aq_create();
    scanner.first = scanner.act = (uchar *)&(*first);
    scanner.last = (uchar *)&(*last);
    scanner.line = 1;                   // start with line_no 1
    scanner.error_proc = report_error;
    scanner.file_name = filename.c_str();
    
#if BOOST_WAVE_SUPPORT_MS_EXTENSIONS != 0
    scanner.enable_ms_extensions = 1;
#else
    scanner.enable_ms_extensions = 0;
#endif

#if BOOST_WAVE_SUPPORT_VARIADICS_PLACEMARKERS != 0
    scanner.act_in_c99_mode = boost::wave::need_c99(language);
#if BOOST_WAVE_ENABLE_CPP0X_EXTENSIONS != 0
    scanner.act_in_cpp0x_mode = boost::wave::need_cpp0x(language);
#endif
#endif

    boost::ignore_unused_variable_warning(language);
}

template <typename IteratorT, typename PositionT>
inline
lexer<IteratorT, PositionT>::~lexer() 
{
    aq_terminate(scanner.eol_offsets);
    free(scanner.bot);
}

///////////////////////////////////////////////////////////////////////////////
//  get the next token from the input stream
template <typename IteratorT, typename PositionT>
inline lex_token<PositionT> 
lexer<IteratorT, PositionT>::get()
{
    if (at_eof) 
        return lex_token<PositionT>();  // return T_EOI

    token_id id = token_id(scan(&scanner));
    
    switch (id) {
    case T_IDENTIFIER:
    // test identifier characters for validity (throws if invalid chars found)
        value = string_t((char const *)scanner.tok, scanner.cur-scanner.tok);
        impl::validate_identifier_name(value, scanner.line, -1, filename); 
        break;
    
    case T_STRINGLIT:
    case T_CHARLIT:
    // test literal characters for validity (throws if invalid chars found)
        value = string_t((char const *)scanner.tok, scanner.cur-scanner.tok);
        impl::validate_literal(value, scanner.line, -1, filename); 
        break;

#if BOOST_WAVE_SUPPORT_INCLUDE_NEXT != 0
    case T_PP_HHEADER:
    case T_PP_QHEADER:
    case T_PP_INCLUDE:
    // convert to the corresponding ..._next token, if appropriate
        value = string_t((char const *)scanner.tok, scanner.cur-scanner.tok);
        if (string_t::npos != value.find("include_"))
            id = token_id(id | AltTokenType);
        break;
#endif

    case T_OCTALINT:
    case T_DECIMALINT:
    case T_HEXAINT:
    case T_INTLIT:
    case T_FLOATLIT:
    case T_FIXEDPOINTLIT:
    case T_CCOMMENT:
    case T_CPPCOMMENT:
    case T_SPACE:
    case T_SPACE2:
    case T_ANY:
        value = string_t((char const *)scanner.tok, scanner.cur-scanner.tok);
        break;
        
    case T_EOF:
    // T_EOF is returned as a valid token, the next call will return T_EOI,
    // i.e. the actual end of input
        at_eof = true;
        break;
        
    default:
        if (CATEGORY_FROM_TOKEN(id) != EXTCATEGORY_FROM_TOKEN(id) ||
            IS_CATEGORY(id, UnknownTokenType))
        {
            value = string_t((char const *)scanner.tok, scanner.cur-scanner.tok);
        }
        else {
            value = token_cache.get_token_value(id);
        }
        break;
    }
    
    return lex_token<PositionT>(id, value, 
        PositionT(filename, scanner.line, -1));
}

template <typename IteratorT, typename PositionT>
inline int 
lexer<IteratorT, PositionT>::report_error(Scanner *s, char *msg, ...)
{
    BOOST_ASSERT(0 != s);
    BOOST_ASSERT(0 != msg);

    using namespace std;    // some system have vsprintf in namespace std
    
    char buffer[200];           // should be large enough
    va_list params;
    va_start(params, msg);
    vsprintf(buffer, msg, params);
    va_end(params);
    
    BOOST_WAVE_LEXER_THROW(lexing_exception, generic_lexing_error, buffer, s->line, -1, 
        s->file_name);
    return 0;   // unreachable code;
}

///////////////////////////////////////////////////////////////////////////////
//   
//  lex_functor
//   
///////////////////////////////////////////////////////////////////////////////
     
template <typename IteratorT, typename PositionT = boost::wave::util::file_position_t>
class lex_functor 
:   public lex_input_interface<typename lexer<IteratorT, PositionT>::token_t>
{    
public:

    typedef typename lexer<IteratorT, PositionT>::token_t   token_t;
    
    lex_functor(IteratorT const &first, IteratorT const &last, 
            PositionT const &pos, boost::wave::language_support language)
    :   lexer(first, last, pos, language)
    {}
    virtual ~lex_functor() {}
    
// get the next token from the input stream
    token_t get() { return lexer.get(); }
    void set_position(PositionT const &pos) 
    { lexer.set_position(pos); }

private:
    lexer<IteratorT, PositionT> lexer;
};

}   // namespace re2clex

///////////////////////////////////////////////////////////////////////////////
//  
//  The new_lexer_gen<>::new_lexer function (declared in cpp_slex_token.hpp)
//  should be defined inline, if the lex_functor shouldn't be instantiated 
//  separately from the lex_iterator.
//
//  Separate (explicit) instantiation helps to reduce compilation time.
//
///////////////////////////////////////////////////////////////////////////////

#if BOOST_WAVE_SEPARATE_LEXER_INSTANTIATION != 0
#define BOOST_WAVE_RE2C_NEW_LEXER_INLINE
#else
#define BOOST_WAVE_RE2C_NEW_LEXER_INLINE inline
#endif 

///////////////////////////////////////////////////////////////////////////////
//
//  The 'new_lexer' function allows the opaque generation of a new lexer object.
//  It is coupled to the iterator type to allow to decouple the lexer/iterator 
//  configurations at compile time.
//
//  This function is declared inside the cpp_slex_token.hpp file, which is 
//  referenced by the source file calling the lexer and the source file, which
//  instantiates the lex_functor. But is is defined here, so it will be 
//  instantiated only while compiling the source file, which instantiates the 
//  lex_functor. While the cpp_re2c_token.hpp file may be included everywhere,
//  this file (cpp_re2c_lexer.hpp) should be included only once. This allows
//  to decouple the lexer interface from the lexer implementation and reduces 
//  compilation time.
//
///////////////////////////////////////////////////////////////////////////////

template <typename IteratorT, typename PositionT>
BOOST_WAVE_RE2C_NEW_LEXER_INLINE
lex_input_interface<lex_token<PositionT> > *
new_lexer_gen<IteratorT, PositionT>::new_lexer(IteratorT const &first,
    IteratorT const &last, PositionT const &pos, 
    boost::wave::language_support language)
{
    return new re2clex::lex_functor<IteratorT, PositionT>(first, last, pos,
        language);
}

#undef BOOST_WAVE_RE2C_NEW_LEXER_INLINE

///////////////////////////////////////////////////////////////////////////////
}   // namespace cpplexer
}   // namespace wave
}   // namespace boost 
     
#endif // !defined(CPP_RE2C_LEXER_HPP_B81A2629_D5B1_4944_A97D_60254182B9A8_INCLUDED)
