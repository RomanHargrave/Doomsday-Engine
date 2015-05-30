/** @file help.cpp  Runtime help text strings.
 *
 * @ingroup base
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

#include "doomsday/help.h"
#include "doomsday/console/cmd.h"

#include <de/App>
#include <de/Log>
#include <de/Reader>

#include <QMap>
#include <QStringBuilder>

using namespace de;

typedef QMap<int, String> StringsByType; // HST_* type => string
typedef QMap<String, StringsByType> HelpStrings; // id => typed strings

static HelpStrings helps;

/**
 * Parses the given file looking for help strings. The contents of the file are
 * expected to use UTF-8 encoding.
 *
 * @param file  File containing help strings.
 */
void Help_ReadStrings(File const &file)
{
    LOG_RES_VERBOSE("Reading help strings from ") << file.description();

    de::Reader reader(file);
    StringsByType *node = 0;

    while(!reader.atEnd())
    {
        String line = reader.readLine().trimmed();

        // Comments and empty lines are ignored.
        if(line.isEmpty() || line.startsWith("#"))
            continue;

        // A new node?
        if(line.startsWith("["))
        {
            int end = line.indexOf(']');
            String id = line.mid(1, end > 0? end - 1 : -1).trimmed().toLower();
            node = &helps.insert(id, StringsByType()).value();

            LOG_TRACE_DEBUGONLY("Help node '%s'", id);
        }
        else if(node && line.contains('=')) // It must be a key?
        {
            int type = HST_DESCRIPTION;
            if(line.startsWith("cv", Qt::CaseInsensitive))
            {
                type = HST_CONSOLE_VARIABLE;
            }
            else if(line.startsWith("def", Qt::CaseInsensitive))
            {
                type = HST_DEFAULT_VALUE;
            }
            else if(line.startsWith("inf", Qt::CaseInsensitive))
            {
                type = HST_INFO;
            }

            // Strip the beginning.
            line = line.mid(line.indexOf('=') + 1).trimmed();

            // The full text is collected here.
            QString text;

            // The value may be split over multiple lines.
            while(!line.isEmpty())
            {
                // Process the current line.
                bool escape = false;
                foreach(QChar ch, line)
                {
                    if(ch == QChar('\\'))
                    {
                        escape = true;
                    }
                    else if(escape)
                    {
                        if     (ch == QChar('n') ) text = text % "\n";
                        else if(ch == QChar('b') ) text = text % "\b";
                        else if(ch == QChar('\\')) text = text % "\\";
                        escape = false;
                    }
                    else
                    {
                        text = text % ch;
                    }
                }

                // This part has been processed.
                line.clear();

                if(escape)
                {
                    // Line ended with a backslash; read the next line.
                    line = reader.readLine().trimmed();
                }
            }

            node->insert(type, text);

            LOG_TRACE_DEBUGONLY("Help string (type %i): \"%s\"", type << text);
        }
    }
}

HelpId DH_Find(char const *id)
{
    // The identifiers are case insensitive.
    HelpStrings::const_iterator found = helps.constFind(String(id).lower());
    if(found != helps.constEnd())
    {
        return &found.value();
    }
    return 0;
}

char const *DH_GetString(HelpId found, int type)
{
    if(!found) return 0;
    if(type < 0 || type > NUM_HELPSTRING_TYPES) return 0;

    StringsByType const *hs = reinterpret_cast<StringsByType const *>(found);

    StringsByType::const_iterator i = hs->constFind(type);
    if(i != hs->constEnd())
    {
        return Str_Text(AutoStr_FromTextStd(i.value().toUtf8().constData()));
    }
    return 0;
}

void DD_InitHelp()
{
    LOG_AS("DD_InitHelp");
    try
    {        
        Help_ReadStrings(App::packageLoader().package("net.dengine.base")
                         .root().locate<File>("helpstrings.txt"));
    }
    catch(Error const &er)
    {
        LOG_RES_WARNING("") << er.asText();
    }
}

void DD_ShutdownHelp()
{
    helps.clear();
}

D_CMD(LoadHelp)
{
    DENG2_UNUSED3(src, argc, argv);

    DD_ShutdownHelp();
    DD_InitHelp();
    return true;
}

void DH_Register()
{
    C_CMD("loadhelp", "",   LoadHelp);
}
