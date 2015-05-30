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

#ifndef LIBDENG2_DATE_H
#define LIBDENG2_DATE_H

#include "../Time"
#include "../Log"

#include <QTextStream>

namespace de {

/**
 * Information about a date.
 *
 * @ingroup types
 */
class DENG2_PUBLIC Date : public Time, public LogEntry::Arg::Base
{
public:
    /**
     * Constructs a new Date out of the current time.
     */
    Date();

    Date(Time const &time);

    int year() const { return asDateTime().date().year(); }
    int month() const { return asDateTime().date().month(); }
    int dayOfMonth() const { return asDateTime().date().day(); }
    int hours() const { return asDateTime().time().hour(); }
    int minutes() const { return asDateTime().time().minute(); }
    int seconds() const { return asDateTime().time().second(); }
    int daysTo(Date const &other) const { return int(asDateTime().date().daysTo(other.asDateTime().date())); }

    /**
     * Forms a textual representation of the date.
     */
    String asText() const;

    /**
     * Converts the date back to a Time.
     *
     * @return  The corresponding Time.
     */
    Time asTime() const;

    // Implements LogEntry::Arg::Base.
    LogEntry::Arg::Type logEntryArgType() const {
        return LogEntry::Arg::StringArgument;
    }
};

DENG2_PUBLIC QTextStream &operator << (QTextStream &os, Date const &date);

}

#endif /* LIBDENG2_DATE_H */
