/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_TIME_H
#define LIBDENG2_TIME_H

#include "../libcore.h"
#include "../math.h"
#include "../ISerializable"

#include <QDateTime>
#include <QTextStream>

namespace de {

class Date;
class String;

/**
 * The Time class represents a single time measurement. It represents
 * one absolute point in time (since the epoch).  Instances of Time should be
 * used wherever time needs to be measured, calculated or stored.
 *
 * It is important to note that if the time values are used in a performance
 * sensitive manner (e.g., animations), one should use the values returned by
 * Time::currentHighPerformanceTime(), which deals with simple deltas using
 * seconds as the unit since the start of the process. The normal constructors
 * of Time construct a complete Date/Time pair, meaning it is aware of all the
 * complicated details of time zones, DST, leap years, etc. Consequently,
 * operations of the latter kind of Time instances can be significantly slower
 * when performing often-repeated calculations.
 *
 * @ingroup types
 */
class DENG2_PUBLIC Time : public ISerializable
{
public:
    /**
     * Difference between two points in time. @ingroup types
     */
    class DENG2_PUBLIC Delta : public ISerializable
    {
    public:
        /**
         * Constructs a time delta.
         *
         * @param seconds  Length of the delta.
         */
        Delta(ddouble seconds = 0) : _seconds(seconds) {}

        /**
         * Conversion to the numeric type (floating-point seconds).
         */
        operator ddouble() const { return _seconds; }

        bool operator < (ddouble const &d) const {
            return _seconds < d;
        }

        bool operator > (ddouble const &d) const {
            return _seconds > d;
        }

        bool operator == (ddouble const &d) const {
            return fequal(_seconds, d);
        }

        Delta operator + (ddouble const &d) const;

        Delta &operator += (ddouble const &d);

        Delta operator - (ddouble const &d) const;

        Delta &operator -= (ddouble const &d) {
            _seconds -= d;
            return *this;
        }

        /**
         * Convert the delta to milliseconds.
         *
         * @return  Milliseconds.
         */
        duint64 asMilliSeconds() const;

        ddouble asMinutes() const;

        ddouble asHours() const;

        ddouble asDays() const;

        static Delta fromMilliSeconds(duint64 milliseconds) {
            return Delta(milliseconds/1000.0);
        }

        /**
         * Determines the amount of time passed since the beginning of the native
         * process (i.e., since creation of the high performance timer).
         */
        static Delta sinceStartOfProcess();

        /**
         * Blocks the thread.
         */
        void sleep() const;

        // Implements ISerializable.
        void operator >> (Writer &to) const;
        void operator << (Reader &from);

    private:
        ddouble _seconds;
    };

    enum Format {
        ISOFormat,
        BuildNumberAndTime,
        BuildNumberAndSecondsSinceStart,
        FriendlyFormat,
        ISODateOnly,
        CompilerDateTime // Oct  7 2013 03:18:36 (__DATE__ __TIME__)
    };

public:
    /**
     * Constructor initializes the time to the current time.
     */
    Time();

    Time(Time const &other);

    Time(QDateTime const &t);

    /**
     * Construct a time relative to the shared high performance timer.
     *
     * @param highPerformanceDelta Elapsed time since the high performance timer was started.
     */
    Time(Delta const &highPerformanceDelta);

    static Time invalidTime();

    Time &operator = (Time const &other);

    bool isValid() const;

    bool operator < (Time const &t) const;

    inline bool operator > (Time const &t) const { return t < *this; }

    inline bool operator <= (Time const &t) const { return !(*this > t); }

    inline bool operator >= (Time const &t) const { return !(*this < t); }

    bool operator == (Time const &t) const;

    inline bool operator != (Time const &t) const { return !(*this == t); }

    /**
     * Add a delta to the point of time.
     *
     * @param delta  Delta to add.
     *
     * @return  Modified time.
     */
    Time operator + (Delta const &delta) const;

    /**
     * Subtract a delta from the point of time.
     *
     * @param delta  Delta to subtract.
     *
     * @return  Modified time.
     */
    inline Time operator - (Delta const &delta) const { return *this + (-delta); }

    /**
     * Modify point of time.
     *
     * @param delta  Delta to add.
     *
     * @return  Reference to this Time.
     */
    Time &operator += (Delta const &delta);

    /**
     * Modify point of time.
     *
     * @param delta  Delta to subtract.
     *
     * @return  Reference to this Time.
     */
    inline Time &operator -= (Delta const &delta) { return *this += -delta; }

    /**
     * Difference between two times.
     *
     * @param earlierTime  Time at some point before this time.
     */
    Delta operator - (Time const &earlierTime) const;

    /**
     * Difference between this time and the current point of time.
     * Returns positive deltas if current time is past this time.
     *
     * @return  Delta.
     */
    inline Delta since() const { return deltaTo(Time()); }

    /**
     * Difference between current time and this time.
     * Returns positive deltas if current time is before this time.
     *
     * @return  Delta.
     */
    inline Delta until() const { return Time().deltaTo(*this); }

    /**
     * Difference to a later point in time.
     *
     * @param laterTime  Time.
     *
     * @return  Delta.
     */
    inline Delta deltaTo(Time const &laterTime) const { return laterTime - *this; }

    /**
     * Makes a text representation of the time (default is ISO format, e.g.,
     * 2012-12-02 13:08:21.851).
     */
    String asText(Format format = ISOFormat) const;

    /**
     * Parses a text string into a Time.
     *
     * @param text    Text that contains a date and/or time.
     * @param format  Format of the text string.
     *
     * @return Time that corresponds @a text.
     */
    static Time fromText(String const &text, Format format = ISOFormat);

    /**
     * Converts the time to a QDateTime.
     */
    QDateTime &asDateTime();

    /**
     * Converts the time to a QDateTime.
     */
    QDateTime const &asDateTime() const;

    /**
     * Converts the time into a Date.
     */
    Date asDate() const;

    /**
     * Converts the time to a build number.
     */
    dint asBuildNumber() const;

    // Implements ISerializable.
    void operator >> (Writer &to) const;
    void operator << (Reader &from);

public:
    /**
     * Returns a Time that represents the current elapsed time from the shared
     * high performance timer.
     *
     * @return
     */
    static Time currentHighPerformanceTime();

private:
    DENG2_PRIVATE(d)
};

DENG2_PUBLIC QTextStream &operator << (QTextStream &os, Time const &t);

typedef Time::Delta TimeDelta;

} // namespace de

#endif // LIBDENG2_TIME_H
