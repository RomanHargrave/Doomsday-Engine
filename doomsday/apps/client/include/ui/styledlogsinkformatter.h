/** @file styledlogsinkformatter.h
 *
 * @authors Copyright (c) 2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#ifndef DENG_CLIENT_STYLEDLOGSINKFORMATTER_H
#define DENG_CLIENT_STYLEDLOGSINKFORMATTER_H

#include <de/LogSink>
#include <de/String>

/**
 * Formats log entries for styled output.
 */
class StyledLogSinkFormatter : public de::LogSink::IFormatter
{
public:
    StyledLogSinkFormatter();
    StyledLogSinkFormatter(de::LogEntry::Flags const &formatFlags);

    Lines logEntryToTextLines(de::LogEntry const &entry);

    /**
     * Omits the log entry section information if the entry is marked as
     * a non-developer entry. The default is @c true.
     *
     * @param omit  Omit section.
     */
    void setOmitSectionIfNonDev(bool omit);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_STYLEDLOGSINKFORMATTER_H
