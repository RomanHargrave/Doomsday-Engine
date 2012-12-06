/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2004-2012 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "de/Log"
#include "de/Time"
#include "de/Date"
#include "de/LogBuffer"
#include "de/Guard"
#include "logtextstyle.h"

#include <QMap>
#include <QTextStream>
#include <QThread>
#include <QStringList>
#include <QDebug>

namespace de {

char const *MAIN_SECTION = "";

/// If the section is longer than this, it will be alone on one line while
/// the rest of the entry continues after a break.
static int const LINE_BREAKING_SECTION_LENGTH = 35;

namespace internal {

/**
 * @internal
 * The logs table is lockable so that multiple threads can access their logs at
 * the same time.
 */
class Logs : public QMap<QThread *, Log *>, public Lockable
{
public:
    Logs() {}
    ~Logs() {
        DENG2_GUARD(this);
        // The logs are owned by the logs table.
        foreach(Log *log, values()) {
            delete log;
        }
    }
};

} // namespace internal

/// The logs table contains the log of each thread that uses logging.
static std::auto_ptr<internal::Logs> logsPtr;

LogEntry::LogEntry() : _level(TRACE), _sectionDepth(0), _disabled(true)
{}

LogEntry::LogEntry(Level level, String const &section, int sectionDepth, String const &format, Args args)
    : _level(level), _section(section), _sectionDepth(sectionDepth), _format(format), _disabled(false), _args(args)
{
    if(!LogBuffer::appBuffer().isEnabled(level))
    {
        _disabled = true;
    }
}

LogEntry::~LogEntry()
{
    // The entry has ownership of its args.
    for(Args::iterator i = _args.begin(); i != _args.end(); ++i) 
    {
        delete *i;
    }    
}

String LogEntry::asText(Flags const &formattingFlags, int shortenSection) const
{
    /// @todo This functionality belongs in an entry formatter class.

    Flags flags = formattingFlags;
    QString result;
    QTextStream output(&result);

    if(_defaultFlags & Simple)
    {
        flags |= Simple;
    }
    
    // In simple mode, skip the metadata.
    if(!flags.testFlag(Simple))
    {    
        // Begin with the timestamp.
        if(flags & Styled) output << TEXT_STYLE_LOG_TIME;
    
        output << _when.asText(Date::BuildNumberAndTime) << " ";

        if(!flags.testFlag(Styled))
        {
            char const *levelNames[LogEntry::MAX_LOG_LEVELS] = {
                "(...)",
                "(deb)",
                "(vrb)",
                "",
                "(inf)",
                "(WRN)",
                "(ERR)",
                "(!!!)"        
            };
            output << qSetPadChar(' ') << qSetFieldWidth(5) << levelNames[_level] <<
                      qSetFieldWidth(0) << " ";
        }
        else
        {
            char const *levelNames[LogEntry::MAX_LOG_LEVELS] = {
                "Trace",
                "Debug",
                "Verbose",
                "",
                "Info",
                "Warning",
                "ERROR",
                "FATAL!"        
            };
            output << "\t" 
                << (_level >= LogEntry::WARNING? TEXT_STYLE_LOG_BAD_LEVEL : TEXT_STYLE_LOG_LEVEL)
                << levelNames[_level] << "\t\r";
        }
    }

    // Section name.
    if(!flags.testFlag(OmitSection) && !_section.empty())
    {
        if(flags.testFlag(Styled))
        {
            output << TEXT_STYLE_SECTION;
        }

        // Process the section: shortening and possible abbreviation.
        QString sect;
        if(flags.testFlag(AbbreviateSection))
        {
            /*
             * We'll split the section into parts, and then abbreviate some of
             * the parts, trying not to lose too much information.
             * @a shortenSection controls how much of the section can be
             * abbreviated (num of chars from beginning).
             */
            QStringList parts = _section.split(" > ");
            int len = 0;
            while(!parts.isEmpty())
            {
                if(!sect.isEmpty())
                {
                    len += 3;
                    sect += " > ";
                }

                if(len + parts.first().size() >= shortenSection) break;

                len += parts.first().size();
                if(sect.isEmpty())
                {
                    // Never abbreviate the first part.
                    sect += parts.first();
                }
                else
                {
                    sect += "..";
                }
                parts.removeFirst();
            }
            // Append the remainer as-is.
            sect += _section.mid(len);
        }
        else
        {
            if(shortenSection < _section.size())
            {
                sect = _section.right(_section.size() - shortenSection);
            }
        }

        if(flags.testFlag(SectionSameAsBefore))
        {
            if(!shortenSection || sect.isEmpty())
            {
                output << "^ : ";
            }
            else
            {
                output << "^" << sect << ": ";
            }
        }
        else
        {
            // If the section is very long, it's clearer to break the line here.
            char const *separator = (sect.size() > LINE_BREAKING_SECTION_LENGTH? ":\n    " : ": ");
            output << sect << separator;
        }
    }

    if(flags.testFlag(Styled))
    {
        output << TEXT_STYLE_MESSAGE;
    }
    
    // Message text with the arguments formatted.
    if(_args.empty())
    {
        output << _format; // Verbatim.
    }
    else
    {
        Args::const_iterator arg = _args.begin();

        DENG2_FOR_EACH_CONST(String, i, _format)
        {
            if(*i == '%')
            {
                if(arg == _args.end())
                {
                    // Out of args.
                    throw IllegalFormatError("LogEntry::asText", "Ran out of arguments");
                }
                                
                output << String::patternFormat(i, _format.end(), **arg);
                ++arg;
            }
            else
            {
                output << *i;
            }
        }
        
        // Just append the rest of the arguments without special instructions.
        for(; arg != _args.end(); ++arg)
        {
            output << **arg;
        }        
    }
    
    if(flags & Styled)
    {
        output << TEXT_STYLE_MESSAGE;
    }
    
    return result;
}

QTextStream &operator << (QTextStream &stream, LogEntry::Arg const &arg)
{
    switch(arg.type())
    {
    case LogEntry::Arg::INTEGER:
        stream << arg.intValue();
        break;
        
    case LogEntry::Arg::FLOATING_POINT:
        stream << arg.floatValue();
        break;
        
    case LogEntry::Arg::STRING:
        stream << arg.stringValue();
        break;
    }
    return stream;
}

Log::Section::Section(char const *name) : _log(threadLog()), _name(name)
{
    _log.beginSection(_name);
}

Log::Section::~Section()
{
    _log.endSection(_name);
}

Log::Log() : _throwawayEntry(0)
{
    _throwawayEntry = new LogEntry; // disabled LogEntry, so doesn't accept arguments
    _sectionStack.push_back(MAIN_SECTION);
}

Log::~Log()
{
    delete _throwawayEntry;
}

void Log::beginSection(char const *name)
{
    _sectionStack.append(name);
}

void Log::endSection(char const *DENG2_DEBUG_ONLY(name))
{
    DENG2_ASSERT(_sectionStack.back() == name);
    _sectionStack.takeLast();
}

LogEntry &Log::enter(String const &format, LogEntry::Args arguments)
{
    return enter(LogEntry::MESSAGE, format, arguments);
}

LogEntry &Log::enter(LogEntry::Level level, String const &format, LogEntry::Args arguments)
{
    if(!LogBuffer::appBuffer().isEnabled(level))
    {
        DENG2_ASSERT(arguments.isEmpty());

        // If the level is disabled, no messages are entered into it.
        return *_throwawayEntry;
    }
    
    // Collect the sections.
    String context;
    String latest;
    int depth = 0;
    foreach(char const *i, _sectionStack)
    {
        if(i == latest)
        {
            // Don't repeat if it has the exact same name (due to recursive calls).
            continue;
        }
        if(context.size())
        {
            context += " > ";
        }
        latest = i;
        context += i;
        ++depth;
    }

    // Make a new entry.
    LogEntry *entry = new LogEntry(level, context, depth, format, arguments);
    
    // Add it to the application's buffer. The buffer gets ownership.
    LogBuffer::appBuffer().add(entry);
    
    return *entry;
}

Log &Log::threadLog()
{
    if(!logsPtr.get()) logsPtr.reset(new internal::Logs);
    internal::Logs& logs = *logsPtr;
    DENG2_GUARD(logs);

    // Each thread has its own log.
    QThread *thread = QThread::currentThread();
    internal::Logs::const_iterator found = logs.constFind(thread);
    if(found == logs.constEnd())
    {
        // Create a new log.
        Log* theLog = new Log;
        qDebug() << "Log" << theLog << "created for thread" << thread;
        logs.insert(thread, theLog);
        return *theLog;
    }
    else
    {
        return *found.value();
    }
}

void Log::disposeThreadLog()
{
    if(!logsPtr.get()) logsPtr.reset(new internal::Logs);
    internal::Logs& logs = *logsPtr;
    DENG2_GUARD(logs);

    QThread *thread = QThread::currentThread();
    internal::Logs::iterator found = logs.find(thread);
    if(found != logs.end())
    {
        delete found.value();
        logs.remove(found.key());
    }
}

LogEntryStager::LogEntryStager(LogEntry::Level level, String const &format) : _level(level)
{
    _disabled = !LogBuffer::appBuffer().isEnabled(level);
    if(!_disabled)
    {
        _format = format;
    }
}

} // namespace de
