/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2009-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "de/CommandLine"
#include "de/String"
#include "de/NativePath"
#include "de/Log"

#include <QFile>
#include <QDir>
#include <QProcess>
#include <QDebug>

#include <fstream>
#include <sstream>
#include <cctype>
#include <string.h>

using namespace de;

static char *duplicateStringAsUtf8(QString const &s)
{
    QByteArray utf = s.toUtf8();
    char *copy = (char *) malloc(utf.size() + 1);
    memcpy(copy, utf.constData(), utf.size());
    copy[utf.size()] = 0;
    return copy;
}

DENG2_PIMPL(CommandLine)
{
    QDir initialDir;

    typedef QList<QString> Arguments;
    Arguments arguments;

    typedef std::vector<char *> ArgumentPointers; // UTF-8 representation
    ArgumentPointers pointers;

    typedef std::vector<String> ArgumentStrings;
    typedef std::map<std::string, ArgumentStrings> Aliases;
    Aliases aliases;

    Instance(Public &i) : Base(i)
    {
        initialDir = QDir::current();
    }

    ~Instance()
    {
        clear();
    }

    void clear()
    {
        arguments.clear();
        DENG2_FOR_EACH(ArgumentPointers, i, pointers) free(*i);
        pointers.clear();
        pointers.push_back(0);
    }

    void appendArg(QString const &arg)
    {
        arguments.append(arg);

        if(pointers.empty())
        {
            pointers.push_back(duplicateStringAsUtf8(arg));
            pointers.push_back(0); // Keep null-terminated.
        }
        else
        {
            // Insert before the NULL.
            pointers.insert(pointers.end() - 1, duplicateStringAsUtf8(arg));
        }
        DENG2_ASSERT(pointers.back() == 0);
    }

    void insert(duint pos, String const &arg)
    {
        if(pos > (duint) arguments.size())
        {
            /// @throw OutOfRangeError @a pos is out of range.
            throw OutOfRangeError("CommandLine::insert", "Index out of range");
        }

        arguments.insert(pos, arg);

        pointers.insert(pointers.begin() + pos, duplicateStringAsUtf8(arg));
        DENG2_ASSERT(pointers.back() == 0);
    }

    void remove(duint pos)
    {
        if(pos >= (duint) arguments.size())
        {
            /// @throw OutOfRangeError @a pos is out of range.
            throw OutOfRangeError("CommandLine::remove", "Index out of range");
        }

        arguments.removeAt(pos);

        free(pointers[pos]);
        pointers.erase(pointers.begin() + pos);
        DENG2_ASSERT(pointers.back() == 0);
    }
};

CommandLine::CommandLine() : d(new Instance(*this))
{}

CommandLine::CommandLine(QStringList const &args) : d(new Instance(*this))
{
    for(int i = 0; i < args.size(); ++i)
    {
        if(args.at(i)[0] == '@')
        {
            // This is a response file or something else that requires parsing.
            parseResponseFile(args.at(i).mid(1));
        }
        else
        {
            d->appendArg(args.at(i));
        }
    }
}

CommandLine::CommandLine(CommandLine const &other) : d(new Instance(*this))
{
    DENG2_FOR_EACH_CONST(Instance::Arguments, i, other.d->arguments)
    {
        d->appendArg(*i);
    }
}

NativePath CommandLine::startupPath()
{
    return d->initialDir.path();
}

dint CommandLine::count() const
{
    return d->arguments.size();
}

void CommandLine::clear()
{
    d->clear();
}

void CommandLine::append(String const &arg)
{
    d->appendArg(arg);
}

void CommandLine::insert(duint pos, String const &arg)
{
    d->insert(pos, arg);
}

void CommandLine::remove(duint pos)
{
    d->remove(pos);
}

CommandLine::ArgWithParams CommandLine::check(String const &arg, dint numParams) const
{
    // Do a search for arg.
    Instance::Arguments::const_iterator i = d->arguments.begin();
    for(; i != d->arguments.end() && !matches(arg, *i); ++i) {}
    
    if(i == d->arguments.end())
    {
        // Not found.
        return ArgWithParams();
    }

    // It was found, check for the number of non-option parameters.
    ArgWithParams found;
    found.arg = arg;
    Instance::Arguments::const_iterator k = i;
    while(numParams-- > 0)
    {
        if(++k == d->arguments.end() || isOption(*k))
        {
            // Ran out of arguments, or encountered an option.
            return ArgWithParams();
        }
        found.params.append(*k);
    }

    found.pos = i - d->arguments.begin();
    return found;
}

bool CommandLine::getParameter(String const &arg, String &param) const
{
    dint pos = check(arg, 1);
    if(pos > 0)
    {
        param = at(pos + 1);
        return true;
    }
    return false;
}

dint CommandLine::has(String const &arg) const
{
    dint howMany = 0;
    
    DENG2_FOR_EACH_CONST(Instance::Arguments, i, d->arguments)
    {
        if(matches(arg, *i))
        {
            howMany++;
        }
    }
    return howMany;
}

bool CommandLine::isOption(duint pos) const
{
    if(pos >= (duint) d->arguments.size())
    {
        /// @throw OutOfRangeError @a pos is out of range.
        throw OutOfRangeError("CommandLine::isOption", "Index out of range");
    }
    DENG2_ASSERT(!d->arguments[pos].isEmpty());
    return isOption(d->arguments[pos]);
}

bool CommandLine::isOption(String const &arg)
{
    return !(arg.empty() || arg[0] != '-');
}

String CommandLine::at(duint pos) const
{
    return d->arguments.at(pos);
}

char const *const *CommandLine::argv() const
{
    DENG2_ASSERT(*d->pointers.rbegin() == 0); // the list itself must be null-terminated
    return &d->pointers[0];
}

void CommandLine::makeAbsolutePath(duint pos)
{
    if(pos >= (duint) d->arguments.size())
    {
        /// @throw OutOfRangeError @a pos is out of range.
        throw OutOfRangeError("CommandLine::makeAbsolutePath", "Index out of range");
    }

    QString arg = d->arguments[pos];

    if(!isOption(pos) && !arg.startsWith("}"))
    {
        bool converted = false;
        QDir dir(NativePath(arg).expand()); // note: strips trailing slash

        if(!QDir::isAbsolutePath(arg))
        {
            dir.setPath(d->initialDir.filePath(dir.path()));
            converted = true;
        }

        // Update the argument string.
        d->arguments[pos] = NativePath(dir.path());

        QFileInfo info(dir.path());
        if(info.isDir())
        {
            // Append a slash so FS1 will treat it as a directory.
            d->arguments[pos] += '/';
        }

        // Replace the pointer string.
        free(d->pointers[pos]);
        d->pointers[pos] = duplicateStringAsUtf8(d->arguments[pos]);

        if(converted)
        {
            LOG_DEBUG("Argument %i converted to absolute path: \"%s\"") << pos << d->pointers[pos];
        }
    }
}

void CommandLine::parseResponseFile(NativePath const &nativePath)
{
    QFile response(nativePath.expand());
    if(response.open(QFile::ReadOnly | QFile::Text))
    {
        parse(QString::fromUtf8(response.readAll().constData()));
    }
    else
    {
        qWarning() << "Failed to open response file:" << nativePath;
    }
}

void CommandLine::parse(String const &cmdLine)
{
    String::const_iterator i = cmdLine.begin();

    // This is unset if we encounter a terminator token.
    bool isDone = false;

    // Are we currently inside quotes?
    bool quote = false;

    while(i != cmdLine.end() && !isDone)
    {
        // Skip initial whitespace.
        String::skipSpace(i, cmdLine.end());
        
        // Check for response files.
        bool isResponse = false;
        if(*i == '@')
        {
            isResponse = true;
            String::skipSpace(++i, cmdLine.end());
        }

        String word;

        while(i != cmdLine.end() && (quote || !(*i).isSpace()))
        {
            bool copyChar = true;
            if(!quote)
            {
                // We're not inside quotes.
                if(*i == '\"') // Quote begins.
                {
                    quote = true;
                    copyChar = false;
                }
            }
            else
            {
                // We're inside quotes.
                if(*i == '\"') // Quote ends.
                {
                    if(i + 1 != cmdLine.end() && *(i + 1) == '\"') // Doubles?
                    {
                        // Normal processing, but output only one quote.
                        i++;
                    }
                    else
                    {
                        quote = false;
                        copyChar = false;
                    }
                }
            }

            if(copyChar)
            {
                word.push_back(*i);
            }
            
            i++;
        }

        // Word has been extracted, examine it.
        if(isResponse) // Response file?
        {
            parseResponseFile(word);
        }
        else if(word == "--") // End of arguments.
        {
            isDone = true;
        }
        else if(!word.empty()) // Make sure there *is *a word.
        {
            d->appendArg(word);
        }
    }
}

void CommandLine::alias(String const &full, String const &alias)
{
    d->aliases[full.toStdString()].push_back(alias);
}

bool CommandLine::matches(String const &full, String const &fullOrAlias) const
{
    if(!full.compareWithoutCase(fullOrAlias))
    {
        // They are, in fact, the same.
        return true;
    }
    
    Instance::Aliases::const_iterator found = d->aliases.find(full.toStdString());
    if(found != d->aliases.end())
    {
        DENG2_FOR_EACH_CONST(Instance::ArgumentStrings, i, found->second)
        {
            if(!i->compareWithoutCase(fullOrAlias))
            {
                // Found it among the aliases.
                return true;
            }
        }
    }    
    return false;
}

bool CommandLine::execute() const
{
    LOG_AS("CommandLine");

    if(count() < 1) return false;

    QStringList args;
    for(int i = 1; i < count(); ++i) args << at(i);

    qint64 pid = 0;
    if(!QProcess::startDetached(at(0), args, d->initialDir.path(), &pid))
    {
        LOG_ERROR("Failed to start \"%s\"") << at(0);
        return false;
    }

    LOG_DEBUG("Started detached process %i using \"%s\"") << pid << at(0);
    return true;
}

bool CommandLine::executeAndWait(String *output) const
{
    LOG_AS("CommandLine");

    if(count() < 1) return false;

    QStringList args;
    for(int i = 1; i < count(); ++i) args << at(i);

    LOG_DEBUG("Starting process \"%s\"") << at(0);

    if(output) output->clear();

    QProcess proc;
    proc.start(at(0), args);
    if(!proc.waitForStarted()) return false;
    bool result = proc.waitForFinished();

    if(output)
    {
        *output = String::fromUtf8(Block(proc.readAll()));
    }

    return result;
}
