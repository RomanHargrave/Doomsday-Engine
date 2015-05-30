/** @file logsink.h  Sink where log entries are flushed from the LogBuffer.
 *
 * @authors Copyright © 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_LOGSINK_H
#define LIBDENG2_LOGSINK_H

#include "../String"
#include "../Log"
#include <QList>

namespace de {

/**
 * Sink where log entries are flushed from the LogBuffer.
 * @ingroup core
 */
class DENG2_PUBLIC LogSink
{
public:
    enum Mode {
        Disabled,
        Enabled,
        OnlyNormalEntries,  ///< Info or lower.
        OnlyWarningEntries  ///< Warning or higher.
    };

public:
    /**
     * Formatters are responsible for converting LogEntry instances to a
     * human-presentable, print-ready format suitable for the sink. It may,
     * for instance, apply indenting and omit repeating parts.
     */
    class DENG2_PUBLIC IFormatter
    {
    public:
        typedef QList<String> Lines;

    public:
        virtual ~IFormatter() {}

        virtual Lines logEntryToTextLines(LogEntry const &entry) = 0;
    };

public:
    LogSink();

    /**
     * Construct a sink.
     *
     * @param formatter  Formatter for the flushed entries.
     */
    LogSink(IFormatter &formatter);

    virtual ~LogSink();

    void setMode(Mode mode);

    Mode mode() const;

    bool willAccept(LogEntry const &entry) const;

    IFormatter *formatter();

    /**
     * Output a log entry to the sink. The caller must first verify with
     * isAccepted() whether this is an acceptable entry according to the mode
     * of the sink.
     *
     * The default implementation uses the formatter to convert the entry
     * to one or more lines of text.
     *
     * @param entry  Log entry to output.
     *
     * @return Reference to this sink.
     */
    virtual LogSink &operator << (LogEntry const &entry);

    /**
     * Output a plain text string to the sink. This will be called as a
     * fallback if the formatting of a LogEntry causes an exception.
     *
     * @param plainText  Message.
     *
     * @return Reference to this sink.
     */
    virtual LogSink &operator << (String const &plainText) = 0;

    /**
     * Flushes buffered output.
     */
    virtual void flush() = 0;

private:
    IFormatter *_formatter;
    Mode _mode;
};

} // namespace de

#endif // LIBDENG2_LOGSINK_H
