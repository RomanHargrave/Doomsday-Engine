/** @file precompiled.h  Precompiled headers for libappfw.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifdef __cplusplus

// C++ standard library:
#include <algorithm>
#include <cmath>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <typeinfo>
#include <vector>

// Qt:
#include <QtCore/qglobal.h>
#include <QCoreApplication>
#include <QDataStream>
#include <QDebug>
#include <QFlags>
#include <QList>
#include <QMap>
#include <QMutex>
#include <QScopedPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QVarLengthArray>
#include <QVector>

// libcore:
#include <de/App>
#include <de/Error>
#include <de/Log>
#include <de/NativePath>
#include <de/Observers>
#include <de/PathTree>
#include <de/Reader>
#include <de/Rectangle>
#include <de/String>
#include <de/Vector>
#include <de/Writer>

#endif
