/**
 * @file nativepath.cpp
 * File paths for the native file system.
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "de/NativePath"
#include "de/App"
#include <QDir>
#include <QFile>

/**
 * @def NATIVE_BASE_SYMBOLIC
 *
 * Symbol used in "pretty" paths to represent the base directory (the one set
 * with -basedir).
 */
#define NATIVE_BASE_SYMBOLIC    "(basedir)"

/**
 * @def NATIVE_HOME_SYMBOLIC
 *
 * Symbol used in "pretty" paths to represent the user's native home directory.
 * Note that this is not the same thing as App::nativeHomePath(), which returns
 * the native location of the Doomsday runtime "/home" path where runtime files
 * are stored.
 */

#ifdef WIN32
#  define NATIVE_HOME_SYMBOLIC  "%HOMEPATH%"
#  define DIR_SEPARATOR         QChar('\\')
#else
#  define NATIVE_HOME_SYMBOLIC  "~"
#  define DIR_SEPARATOR         QChar('/')
#  ifdef UNIX
#    include <sys/types.h>
#    include <pwd.h>
#  endif
#endif

namespace de {

static QString toNative(String const &s)
{
    // This will resolve parent references (".."), multiple separators
    // (hello//world), and self-references (".").
    return Path::normalizeString(QDir::cleanPath(s), DIR_SEPARATOR);
}

NativePath::NativePath() : Path()
{}

NativePath::NativePath(String const &str) : Path(toNative(str), DIR_SEPARATOR)
{}

NativePath::NativePath(QString const &qstr) : Path(toNative(qstr), DIR_SEPARATOR)
{}

NativePath::NativePath(char const *nullTerminatedCStr)
    : Path(toNative(nullTerminatedCStr), DIR_SEPARATOR)
{
}

NativePath::NativePath(char const *cStr, dsize length)
    : Path(toNative(String(cStr, length)), DIR_SEPARATOR)
{}

NativePath &NativePath::operator = (String const &str)
{
    set(toNative(str), DIR_SEPARATOR);
    return *this;
}

NativePath &NativePath::operator = (QString const &str)
{
    set(toNative(str), DIR_SEPARATOR);
    return *this;
}

NativePath &NativePath::operator = (char const *nullTerminatedCStr)
{
    return (*this) = String(nullTerminatedCStr);
}

NativePath NativePath::concatenatePath(NativePath const &nativePath) const
{
    if(nativePath.isAbsolute()) return nativePath;
    return toString().concatenatePath(nativePath, QChar(DIR_SEPARATOR));
}

NativePath NativePath::concatenatePath(String const &nativePath) const
{
    return concatenatePath(NativePath(nativePath));
}

NativePath NativePath::operator / (NativePath const &nativePath) const
{
    return concatenatePath(nativePath);
}

NativePath NativePath::operator / (String const &str) const
{
    return *this / NativePath(str);
}

NativePath NativePath::operator / (QString const &str) const
{
    return *this / NativePath(str);
}

NativePath NativePath::operator / (char const *nullTerminatedCStr) const
{
    return *this / NativePath(nullTerminatedCStr);
}

NativePath NativePath::fileNamePath() const
{
    return String(*this).fileNamePath(DIR_SEPARATOR);
}

bool NativePath::isAbsolute() const
{
    return QDir::isAbsolutePath(expand());
}

NativePath NativePath::expand(bool *didExpand) const
{
    if(first() == '>' || first() == '}')
    {
        if(didExpand) *didExpand = true;
        return App::app().nativeBasePath() / toString().mid(1);
    }
#ifdef UNIX
    else if(first() == '~')
    {
        if(didExpand) *didExpand = true;

        String const path = toString();
        int firstSlash = path.indexOf('/');
        if(firstSlash > 1)
        {
            // Parse the user's home directory (from passwd).
            QByteArray userName = path.mid(1, firstSlash - 1).toLatin1();
            struct passwd *pw = getpwnam(userName);
            if(!pw)
            {
                /// @throws UnknownUserError  User is not known.
                throw UnknownUserError("NativePath::expand",
                                       String("Unknown user '%1'").arg(QLatin1String(userName)));
            }

            return NativePath(pw->pw_dir) / path.mid(firstSlash + 1);
        }
        else
        {
            // Replace with the HOME path.
            return NativePath(QDir::homePath()) / path.mid(2);
        }
    }
#endif

    // No expansion done.
    if(didExpand) *didExpand = false;
    return *this;
}

String NativePath::pretty() const
{
    if(isEmpty()) return *this;

    String result = *this;

    // Replace relative directives like '}' (used in FS1 only) with a full symbol.
    if(result.length() > 1 && (result.first() == '}' || result.first() == '>'))
    {
        return String(NATIVE_BASE_SYMBOLIC) + DIR_SEPARATOR + result.mid(1);
    }

    // If within one of the known native directories, cut out the known path,
    // replacing it with a symbolic. This retains the absolute nature of the path
    // while omitting potentially redundant/verbose information.
    if(QDir::isAbsolutePath(result))
    {
        NativePath basePath = App::app().nativeBasePath();
        if(result.beginsWith(basePath))
        {
            result = NATIVE_BASE_SYMBOLIC + result.mid(basePath.length());
        }
        else
        {
#ifdef MACOSX
            NativePath contentsPath = App::app().nativeAppContentsPath();
            if(result.beginsWith(contentsPath))
            {
                return "(app)" + result.mid(contentsPath.length());
            }
#endif
            NativePath homePath = QDir::homePath(); // actual native home dir, not FS2 "/home"
            if(result.beginsWith(homePath))
            {
                result = NATIVE_HOME_SYMBOLIC + result.mid(homePath.length());
            }
        }
    }

    return result;
}

String NativePath::withSeparators(QChar sep) const
{
    return Path::withSeparators(sep);
}

bool NativePath::exists() const
{
    return QFile::exists(toString());
}

bool NativePath::isReadable() const
{
    return QFileInfo(toString()).isReadable();
}

static NativePath currentNativeWorkPath;

NativePath NativePath::workPath()
{
    if(currentNativeWorkPath.isEmpty())
    {
        currentNativeWorkPath = QDir::currentPath();
    }
    return currentNativeWorkPath;
}

bool NativePath::setWorkPath(const NativePath &cwd)
{
    if(QDir::setCurrent(cwd))
    {
        currentNativeWorkPath = cwd;
        return true;
    }
    return false;
}

QChar NativePath::separator()
{
    return DIR_SEPARATOR;
}

} // namespace de
