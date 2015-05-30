/**
 * @file downloaddialog.h
 * Dialog that downloads a distribution package. @ingroup updater
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

#ifndef DENG_CLIENT_DOWNLOADDIALOG_H
#define DENG_CLIENT_DOWNLOADDIALOG_H

#include <de/DialogWidget>

class QNetworkReply;

/**
 * Dialog for downloading an update in the background and then starting
 * the (re)installation process.
 */
class DownloadDialog : public de::DialogWidget
{
    Q_OBJECT

public:
    DownloadDialog(de::String downloadUri, de::String fallbackUri);
    ~DownloadDialog();

    /**
     * Returns the path of the downloaded file.
     *
     * @return Path, or an empty string if the download did not finish
     * successfully.
     */
    de::String downloadedFilePath() const;

    bool isReadyToInstall() const;
    bool isFailed() const;

public:
    static bool isDownloadInProgress();
    static DownloadDialog &currentDownload();
    static void showCompletedDownload();

signals:
    void downloadProgress(int percentage);
    void downloadFailed(QString uri);

public slots:
    void replyMetaDataChanged();
    void progress(qint64 received, qint64 total);
    void finished(QNetworkReply *);
    void cancel();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_DOWNLOADDIALOG_H
