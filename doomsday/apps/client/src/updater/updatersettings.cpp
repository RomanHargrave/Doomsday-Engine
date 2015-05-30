/** @file updatersettings.cpp Persistent settings for automatic updates. 
 * @ingroup updater
 *
 * @authors Copyright © 2012-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include "updater/updatersettings.h"
#include "versioninfo.h"
#include <QDateTime>
#include <QDesktopServices>
#include <de/Record>
#include <de/App>
#include <de/Config>
#include <de/TextValue>
#include <de/NumberValue>
#include <de/TimeValue>

using namespace de;

#define VAR_FREQUENCY       "frequency"
#define VAR_CHANNEL         "channel"
#define VAR_LAST_CHECKED    "lastChecked"
#define VAR_ONLY_MANUAL     "onlyManually"
#define VAR_DELETE          "delete"
#define VAR_DOWNLOAD_PATH   "downloadPath"
#define VAR_DELETE_PATH     "deleteAtStartup"
#define VAR_AUTO_DOWNLOAD   "autoDownload"

#define SUBREC_NAME         "updater"
#define CONF(name)          SUBREC_NAME "." name

#define SYMBOL_DEFAULT_DOWNLOAD "${DEFAULT}"

UpdaterSettings::UpdaterSettings()
{}

UpdaterSettings::Frequency UpdaterSettings::frequency() const
{
    return Frequency(App::config().geti(CONF(VAR_FREQUENCY)));
}

UpdaterSettings::Channel UpdaterSettings::channel() const
{
    return Channel(App::config().geti(CONF(VAR_CHANNEL)));
}

de::Time UpdaterSettings::lastCheckTime() const
{
    // Note that the variable has only AllowTime as the mode.
    return App::config().getAs<TimeValue>(CONF(VAR_LAST_CHECKED)).time();
}

bool UpdaterSettings::onlyCheckManually() const
{
    return App::config().getb(CONF(VAR_ONLY_MANUAL));
}

bool UpdaterSettings::autoDownload() const
{
    return App::config().getb(CONF(VAR_AUTO_DOWNLOAD));
}

bool UpdaterSettings::deleteAfterUpdate() const
{
    return App::config().getb(CONF(VAR_DELETE));
}

de::NativePath UpdaterSettings::pathToDeleteAtStartup() const
{
    de::NativePath p = App::config().gets(CONF(VAR_DELETE_PATH));
    de::String ext = p.toString().fileNameExtension();
    if(p.fileName().startsWith("doomsday") && (ext == ".exe" || ext == ".deb" || ext == ".dmg"))
    {
        return p;
    }
    // Doesn't look valid.
    return "";
}

bool UpdaterSettings::isDefaultDownloadPath() const
{
    return downloadPath() == defaultDownloadPath();
}

de::NativePath UpdaterSettings::downloadPath() const
{
    de::NativePath dir = App::config().gets(CONF(VAR_DOWNLOAD_PATH));
    if(dir.toString() == SYMBOL_DEFAULT_DOWNLOAD)
    {
        dir = defaultDownloadPath();
    }
    return dir;
}

void UpdaterSettings::setDownloadPath(de::NativePath downloadPath)
{
    if(downloadPath == defaultDownloadPath())
    {
        downloadPath = SYMBOL_DEFAULT_DOWNLOAD;
    }
    App::config().set(CONF(VAR_DOWNLOAD_PATH), downloadPath.toString());
}

void UpdaterSettings::setFrequency(UpdaterSettings::Frequency freq)
{
    App::config().set(CONF(VAR_FREQUENCY), dint(freq));
}

void UpdaterSettings::setChannel(UpdaterSettings::Channel channel)
{
    App::config().set(CONF(VAR_CHANNEL), dint(channel));
}

void UpdaterSettings::setLastCheckTime(de::Time const &time)
{
    App::config()[CONF(VAR_LAST_CHECKED)] = new TimeValue(time);
}

void UpdaterSettings::setOnlyCheckManually(bool onlyManually)
{
    App::config().set(CONF(VAR_ONLY_MANUAL), onlyManually);
}

void UpdaterSettings::setAutoDownload(bool autoDl)
{
    App::config().set(CONF(VAR_AUTO_DOWNLOAD), autoDl);
}

void UpdaterSettings::setDeleteAfterUpdate(bool deleteAfter)
{
    App::config().set(CONF(VAR_DELETE), deleteAfter);
}

void UpdaterSettings::useDefaultDownloadPath()
{
    setDownloadPath(defaultDownloadPath());
}

void UpdaterSettings::setPathToDeleteAtStartup(de::NativePath deletePath)
{
    App::config().set(CONF(VAR_DELETE_PATH), deletePath.toString());
}

de::NativePath UpdaterSettings::defaultDownloadPath()
{
#ifdef DENG2_QT_5_0_OR_NEWER
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#else
    return QDesktopServices::storageLocation(QDesktopServices::CacheLocation);
#endif
}

de::String UpdaterSettings::lastCheckAgo() const
{
    de::Time when = lastCheckTime();
    if(!when.isValid()) return ""; // Never checked.

    de::TimeDelta delta = when.since();
    if(delta < 0.0) return "";

    int t;
    if(delta < 60.0)
    {
        t = delta.asMilliSeconds() / 1000;
        return de::String(QObject::tr("%1 %2 ago")).arg(t).
                          arg(t != 1? QObject::tr("seconds") : QObject::tr("second"));
    }

    t = delta.asMinutes();
    if(t <= 60)
    {
        return de::String(QObject::tr("%1 %2 ago")).arg(t).
                arg(t != 1? QObject::tr("minutes") : QObject::tr("minute"));
    }

    t = delta.asHours();
    if(t <= 24)
    {
        return de::String(QObject::tr("%1 %2 ago")).arg(t).
                arg(t != 1? QObject::tr("hours") : QObject::tr("hour"));
    }

    t = delta.asDays();
    if(t <= 7)
    {
        return de::String(QObject::tr("%1 %2 ago")).arg(t).
                arg(t != 1? QObject::tr("days") : QObject::tr("day"));
    }

    return de::String("on " + when.asText(de::Time::FriendlyFormat));
}
