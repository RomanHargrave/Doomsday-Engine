/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef LIBCORE_H
#define LIBCORE_H

/**
 * @file libcore.h  Common definitions for Doomsday 2.
 */

/**
 * @defgroup core  Core
 *
 * @defgroup data  Data Types and Structures
 * Classes related to accessing, storing, and processing of data.
 *
 * @defgroup input  Input Subsystem
 *
 * @defgroup net  Network
 * Classes responsible for network communications.
 *
 * @defgroup resource  Resources
 *
 * @defgroup render  Renderer
 *
 * @defgroup GL  Graphics Library
 *
 * @defgroup math  Math Utilities
 *
 * @defgroup types  Basic Types
 * Basic data types.
 */

/**
 * @mainpage Doomsday SDK
 *
 * This documentation covers all the functions and data that Doomsday 2 makes
 * available for games and other plugins.
 *
 * @section Overview
 * The documentation has been organized into <a href="modules.html">modules</a>.
 * The primary ones are listed below:
 * - @ref core
 * - @ref data
 * - @ref input
 * - @ref net
 * - @ref resource
 * - @ref render
 */

#ifdef __cplusplus
#  define DENG2_EXTERN_C extern "C"
#else
#  define DENG2_EXTERN_C extern
#endif

#if defined(__cplusplus) && !defined(DENG2_C_API_ONLY)
#  define DENG2_USE_QT
#  include <typeinfo>
#  include <memory>  // unique_ptr, shared_ptr
#  include <cstring> // memset
#endif

#if defined(__x86_64__) || defined(__x86_64) || defined(_LP64)
#  define DENG2_64BIT
#endif

#ifdef DENG2_USE_QT
#  include <QtCore/qglobal.h>
#  include <QScopedPointer>
#  include <QDebug>

// Qt versioning helper. Qt 4.8 is the oldest we support.
#  if (QT_VERSION < QT_VERSION_CHECK(4, 8, 0))
#    error "Unsupported version of Qt"
#  endif
#  define DENG2_QT_4_6_OR_NEWER
#  define DENG2_QT_4_7_OR_NEWER
#  define DENG2_QT_4_8_OR_NEWER
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#    define DENG2_QT_5_0_OR_NEWER
#  endif
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 1, 0))
#    define DENG2_QT_5_1_OR_NEWER
#  endif
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
#    define DENG2_QT_5_2_OR_NEWER
#  endif
#  if (QT_VERSION >= QT_VERSION_CHECK(5, 3, 0))
#    define DENG2_QT_5_3_OR_NEWER
#  endif
#endif

#ifndef _MSC_VER
#  include <stdint.h> // Not MSVC so use C99 standard header
#endif

#include <assert.h>

/*
 * When using the C API, the Qt string functions are not available, so we
 * must use the platform-specific functions.
 */
#if defined(UNIX) && defined(DENG2_C_API_ONLY)
#  include <strings.h> // strcasecmp etc. 
#endif

/*
 * The DENG2_PUBLIC macro is used for declaring exported symbols. It must be
 * applied in all exported classes and functions. DEF files are not used for
 * exporting symbols out of libcore.
 */
#if defined(_WIN32) && defined(_MSC_VER)
#  ifdef __DENG2__
// This is defined when compiling the library.
#    define DENG2_PUBLIC __declspec(dllexport)
#  else
#    define DENG2_PUBLIC __declspec(dllimport)
#  endif
#else
// No need to use any special declarators.
#  define DENG2_PUBLIC
#endif

#ifndef NDEBUG
#  define DENG2_DEBUG
   DENG2_EXTERN_C DENG2_PUBLIC void LogBuffer_Flush(void);
#  ifdef DENG2_USE_QT
#    define DENG2_ASSERT(x) {if(!(x)) {LogBuffer_Flush(); Q_ASSERT(x);}}
#  else
#    define DENG2_ASSERT(x) {if(!(x)) {LogBuffer_Flush(); assert(x);}}
#  endif
#  define DENG2_DEBUG_ONLY(x) x
#else
#  define DENG2_NO_DEBUG
#  define DENG2_ASSERT(x)
#  define DENG2_DEBUG_ONLY(x)
#endif

#ifdef DENG2_USE_QT
#  ifdef UNIX
#    include <execinfo.h>
/**
 * @macro DENG2_PRINT_BACKTRACE
 * Debug utility for dumping the current backtrace using qDebug.
 */
#    define DENG2_PRINT_BACKTRACE() { \
        void *callstack[128]; \
        int i, frames = backtrace(callstack, 128); \
        char** strs = backtrace_symbols(callstack, frames); \
        for(i = 0; i < frames; ++i) qDebug("%s", strs[i]); \
        free(strs); }
#    define DENG2_BACKTRACE(n, out) { \
        void *callstack[n]; \
        int i, frames = backtrace(callstack, n); \
        char** strs = backtrace_symbols(callstack, frames); \
        out = ""; \
        for(i = 0; i < frames; ++i) { out.append(strs[i]); out.append('\n'); } \
        free(strs); }
#  endif
#endif

/**
 * Macro for determining the name of a type (using RTTI).
 */
#define DENG2_TYPE_NAME(t)  (typeid(t).name())
    
/**
 * Macro for hiding the warning about an unused parameter.
 */
#define DENG2_UNUSED(a)         (void)a

/**
 * Macro for hiding the warning about two unused parameters.
 */
#define DENG2_UNUSED2(a, b)     (void)a, (void)b

/**
 * Macro for hiding the warning about three unused parameters.
 */
#define DENG2_UNUSED3(a, b, c)  (void)a, (void)b, (void)c

/**
 * Macro for hiding the warning about four unused parameters.
 */
#define DENG2_UNUSED4(a, b, c, d) (void)a, (void)b, (void)c, (void)d

#define DENG2_PLURAL_S(Count) ((Count) != 1? "s" : "")

#define DENG2_BOOL_YESNO(Yes) ((Yes)? "yes" : "no" )

/**
 * Forms an escape sequence string literal. Escape sequences begin
 * with an ASCII Escape character.
 */
#define DENG2_ESC(StringLiteral) "\x1b" StringLiteral
#define _E(Code) DENG2_ESC(#Code)

/**
 * Macro for defining an opaque type in the C wrapper API.
 */
#define DENG2_OPAQUE(Name) \
    struct Name ## _s; \
    typedef struct Name ## _s Name;

/**
 * Macro for converting an opaque wrapper type to a de::type.
 * Asserts that the object really exists (not null).
 */
#define DENG2_SELF(Type, Var) \
    DENG2_ASSERT(Var != 0); \
    de::Type *self = reinterpret_cast<de::Type *>(Var);

/**
 * Macro for iterating through an STL container. Note that @a ContainerRef should be a
 * variable and not a temporary value returned from some function call -- otherwise the
 * begin() and end() calls may not refer to the same container's beginning and end.
 *
 * @param IterClass     Class being iterated.
 * @param Iter          Name/declaration of the iterator variable.
 * @param ContainerRef  Container.
 */
#define DENG2_FOR_EACH(IterClass, Iter, ContainerRef) \
    for(IterClass::iterator Iter = (ContainerRef).begin(); Iter != (ContainerRef).end(); ++Iter)

/// @copydoc DENG2_FOR_EACH
#define DENG2_FOR_EACH_CONST(IterClass, Iter, ContainerRef) \
    for(IterClass::const_iterator Iter = (ContainerRef).begin(); Iter != (ContainerRef).end(); ++Iter)

/**
 * Macro for iterating through an STL container in reverse. Note that @a ContainerRef
 * should be a variable and not a temporary value returned from some function call --
 * otherwise the begin() and end() calls may not refer to the same container's beginning
 * and end.
 *
 * @param IterClass     Class being iterated.
 * @param Var           Name/declaration of the iterator variable.
 * @param ContainerRef  Container.
 */
#define DENG2_FOR_EACH_REVERSE(IterClass, Var, ContainerRef) \
    for(IterClass::reverse_iterator Var = (ContainerRef).rbegin(); Var != (ContainerRef).rend(); ++Var)

/// @copydoc DENG2_FOR_EACH_REVERSE
#define DENG2_FOR_EACH_CONST_REVERSE(IterClass, Var, ContainerRef) \
    for(IterClass::const_reverse_iterator Var = (ContainerRef).rbegin(); Var != (ContainerRef).rend(); ++Var)

#define DENG2_NO_ASSIGN(ClassName) \
    private: ClassName &operator = (ClassName const &);

#define DENG2_NO_COPY(ClassName) \
    private: ClassName(ClassName const &);

#define DENG2_AS_IS_METHODS() \
    template <typename T_> \
    bool is() const { return dynamic_cast<T_ const *>(this) != 0; } \
    template <typename T_> \
    T_ &as() { \
        DENG2_ASSERT(is<T_>()); \
        return *static_cast<T_ *>(this); \
    } \
    template <typename T_> \
    T_ const &as() const { \
        DENG2_ASSERT(is<T_>()); \
        return *static_cast<T_ const *>(this); \
    } \
    template <typename T_> \
    T_ *maybeAs() { return dynamic_cast<T_ *>(this); } \
    template <typename T_> \
    T_ const *maybeAs() const { return dynamic_cast<T_ const *>(this); } \

/**
 * Macro for starting the definition of a private implementation struct. The
 * struct holds a reference to the public instance, which must be specified in
 * the call to the base class constructor. @see de::Private
 *
 * Example:
 * <pre>
 *    DENG2_PIMPL(MyClass)
 *    {
 *        Instance(Public &inst) : Base(inst) {
 *            // constructor
 *        }
 *        // private data and methods
 *    };
 * </pre>
 */
#define DENG2_PIMPL(ClassName) \
    typedef ClassName Public; \
    struct ClassName::Instance : public de::Private<ClassName>

/**
 * Macro for starting the definition of a private implementation struct without
 * a reference to the public instance. This is useful for simpler classes where
 * the private implementation mostly holds member variables.
 */
#define DENG2_PIMPL_NOREF(ClassName) \
    struct ClassName::Instance : public de::IPrivate

/**
 * Macro for publicly declaring a pointer to the private implementation.
 * de::PrivateAutoPtr owns the private instance and will automatically delete
 * it when the PrivateAutoPtr is destroyed.
 */
#define DENG2_PRIVATE(Var) \
    struct Instance; \
    de::PrivateAutoPtr<Instance> Var;

#if defined(__cplusplus) && !defined(DENG2_C_API_ONLY)
namespace de {

/**
 * Interface for all private instance implementation structs.
 * In a debug build, also contains a verification code that can be used
 * to assert whether the pointed object really is derived from IPrivate.
 */
struct IPrivate {
    virtual ~IPrivate() {}
#ifdef DENG2_DEBUG
    unsigned int _privateInstVerification;
#define DENG2_IPRIVATE_VERIFICATION 0xdeadbeef
    IPrivate() : _privateInstVerification(DENG2_IPRIVATE_VERIFICATION) {}
    unsigned int privateInstVerification() const { return _privateInstVerification; }
#endif
};

/**
 * Pointer to the private implementation. Behaves like std::auto_ptr, but with
 * the additional requirement that the pointed/owned instance must be derived
 * from de::IPrivate.
 */
template <typename InstType>
class PrivateAutoPtr
{
    DENG2_NO_COPY  (PrivateAutoPtr)
    DENG2_NO_ASSIGN(PrivateAutoPtr)

public:
    PrivateAutoPtr(InstType *p = 0) : ptr(p) {}
    ~PrivateAutoPtr() { reset(); }

    InstType &operator * () const { return *ptr; }
    InstType *operator -> () const { return ptr; }
    void reset(InstType *p = 0) {
        IPrivate *ip = reinterpret_cast<IPrivate *>(ptr);
        if(ip)
        {
            DENG2_ASSERT(ip->privateInstVerification() == DENG2_IPRIVATE_VERIFICATION);
            delete ip;
        }
        ptr = p;
    }
    InstType *get() const {
        return ptr;
    }
    InstType const *getConst() const {
        return ptr;
    }
    operator InstType *() const {
        return ptr;
    }
    InstType *release() {
        InstType *p = ptr;
        ptr = 0;
        return p;
    }
    void swap(PrivateAutoPtr &other) {
        std::swap(ptr, other.ptr);
    }
    bool isNull() const {
        return !ptr;
    }
#ifdef DENG2_DEBUG
    bool isValid() const {
        return ptr && reinterpret_cast<IPrivate *>(ptr)->privateInstVerification() == DENG2_IPRIVATE_VERIFICATION;
    }
#endif

private:
    InstType *ptr;
};

/**
 * Utility template for defining private implementation data (pimpl idiom). Use
 * this in source files, not in headers.
 */
template <typename PublicType>
struct Private : public IPrivate {
    PublicType &self;
#define thisPublic (&self)      // To be used inside private implementations.

    typedef Private<PublicType> Base;

    Private(PublicType &i) : self(i) {}
    Private(PublicType *i) : self(*i) {}
};

template <typename ToType, typename FromType>
inline ToType function_cast(FromType ptr)
{
    /**
     * @note Casting to a pointer-to-function type: see
     * http://www.trilithium.com/johan/2004/12/problem-with-dlsym/
     */
    // This is not 100% portable to all possible memory architectures; thus:
    DENG2_ASSERT(sizeof(void *) == sizeof(ToType));

    union { FromType original; ToType target; } forcedCast;
    forcedCast.original = ptr;
    return forcedCast.target;
}

/**
 * Clears a region of memory. Size of the region is the size of Type.
 * @param t  Reference to the memory.
 */
template <typename Type>
inline void zap(Type &t) {
    std::memset(&t, 0, sizeof(Type));
}

/**
 * Clears a region of memory. Size of the region is the size of Type.
 * @param t  Pointer to the start of the region of memory.
 *
 * @note An overloaded zap(Type *) would not work as the size of array
 * types could not be correctly determined at compile time; thus this
 * function is not an overload.
 */
template <typename Type>
inline void zapPtr(Type *t) {
    std::memset(t, 0, sizeof(Type));
}

} // namespace de
#endif // __cplusplus

#if defined(__cplusplus) && !defined(DENG2_C_API_ONLY)
namespace de {

/**
 * Operation performed on a flag set (e.g., QFlags).
 */
enum FlagOp {
    UnsetFlags   = 0,   ///< Specified flags are unset, leaving others unmodified.
    SetFlags     = 1,   ///< Specified flags are set, leaving others unmodified.
    ReplaceFlags = 2    ///< Specified flags become the new set of flags, replacing all previous flags.
};

template <typename FlagsType>
void applyFlagOperation(FlagsType &flags, FlagsType const &newFlags, FlagOp operation) {
    switch(operation) {
    case SetFlags:     flags |= newFlags;  break;
    case UnsetFlags:   flags &= ~newFlags; break;
    case ReplaceFlags: flags = newFlags;   break;
    }
}

/**
 * Logical clock-wise direction identifiers.
 */
enum ClockDirection {
    Anticlockwise = 0,
    Clockwise     = 1
};

/**
 * Status to return from abortable iteration loops that use callbacks per iteration.
 *
 * The "for" pattern:
 * <code>
 * LoopResult forExampleObjects(std::function<LoopResult (ExampleObject &)> func);
 *
 * example.forExampleObjects([] (ExampleObject &ex) {
 *     // do stuff...
 *     return LoopContinue;
 * });
 * </code>
 */
enum GenericLoopResult {
    LoopContinue = 0,
    LoopAbort    = 1
};

/// Use as return type of iteration loop callbacks (a "for*" method).
struct LoopResult
{
    int value;

    LoopResult(int val = LoopContinue) : value(val) {}
    operator bool () const { return value != LoopContinue; }
    operator int () const { return value; }
    operator GenericLoopResult () const { return GenericLoopResult(value); }
};
    
/**
 * All serialization in all contexts use a common protocol version number.
 * Whenever anything changes in serialization, the protocol version needs to be
 * incremented. Therefore, deserialization routines shouldn't check for
 * specific versions, but instead always use < and >.
 *
 * Do not reserve version numbers in advance; any build may need to increment
 * the version, necessitating changing the subsequent numbers.
 */
enum ProtocolVersion {
    DENG2_PROTOCOL_1_9_10 = 0,

    DENG2_PROTOCOL_1_10_0 = 0,

    DENG2_PROTOCOL_1_11_0_Time_high_performance = 1,
    DENG2_PROTOCOL_1_11_0 = 1,

    DENG2_PROTOCOL_1_12_0 = 1,

    DENG2_PROTOCOL_1_13_0 = 1,

    DENG2_PROTOCOL_1_14_0_LogEntry_metadata = 2,
    DENG2_PROTOCOL_1_14_0 = 2,

    DENG2_PROTOCOL_1_15_0_NameExpression_with_scope_identifier = 3,
    DENG2_PROTOCOL_1_15_0 = 3,

    DENG2_PROTOCOL_LATEST = DENG2_PROTOCOL_1_15_0
};

//@{
/// @ingroup types
typedef qint8   dchar;      ///< 8-bit signed integer.
typedef quint8  dbyte;      ///< 8-bit unsigned integer.
typedef quint8  duchar;     ///< 8-bit unsigned integer.
typedef dchar   dint8;      ///< 8-bit signed integer.
typedef dbyte   duint8;     ///< 8-bit unsigned integer.
typedef qint16  dint16;     ///< 16-bit signed integer.
typedef quint16 duint16;    ///< 16-bit unsigned integer.
typedef dint16  dshort;     ///< 16-bit signed integer.
typedef duint16 dushort;    ///< 16-bit unsigned integer.
typedef qint32  dint32;     ///< 32-bit signed integer.
typedef quint32 duint32;    ///< 32-bit unsigned integer.
typedef dint32  dint;       ///< 32-bit signed integer.
typedef duint32 duint;      ///< 32-bit unsigned integer.
typedef qint64  dint64;     ///< 64-bit signed integer.
typedef quint64 duint64;    ///< 64-bit unsigned integer.
typedef float   dfloat;     ///< 32-bit floating point number.
typedef double  ddouble;    ///< 64-bit floating point number.
typedef quint64 dsize;

// Pointer-integer conversion (used for legacy code).
#ifdef DENG2_64BIT
typedef duint64 dintptr;
#else
typedef duint32 dintptr;
#endif
//@}

} // namespace de

#include "Error"

#else // !__cplusplus

/*
 * Data types for C APIs.
 */
#ifdef _MSC_VER
typedef short           dint16;
typedef unsigned short  duint16;
typedef int             dint32;
typedef unsigned int    duint32;
typedef long long       dint64;
typedef unsigned long long duint64;
#else
#  include <stdint.h>
typedef int16_t         dint16;
typedef uint16_t        duint16;
typedef int32_t         dint32;
typedef uint32_t        duint32;
typedef int64_t         dint64;
typedef uint64_t        duint64;
#endif
typedef unsigned char   dbyte;
typedef unsigned int    duint;  // 32-bit
typedef float           dfloat;
typedef double          ddouble;

#ifdef DENG2_64BIT
typedef uint64_t        dsize;  // 64-bit size
#else
typedef unsigned int    dsize;  // 32-bit size
#endif

#endif // !__cplusplus

#endif // LIBCORE_H
