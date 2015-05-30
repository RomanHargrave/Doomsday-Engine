/** @file filelogsink.h  Log sink that uses a File for output.
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

#ifndef LIBDENG2_FILELOGSINK_H
#define LIBDENG2_FILELOGSINK_H

#include "../LogSink"
#include "../File"
#include "../MonospaceLogSinkFormatter"

namespace de {

/**
 * Log sink that uses a File for output.
 * @ingroup core
 */
class FileLogSink : public LogSink
{
public:
    FileLogSink(File &outputFile);

    LogSink &operator << (String const &plainText);

    void flush();

private:
    File &_file;
    MonospaceLogSinkFormatter _format;
};

} // namespace de

#endif // LIBDENG2_FILELOGSINK_H
