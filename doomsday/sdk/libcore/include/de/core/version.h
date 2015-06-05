/**
 * @file version.h
 * Version numbering and labeling for libcore.
 *
 * @authors Copyright © 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2011-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef LIBDENG2_VERSION_H
#define LIBDENG2_VERSION_H

#ifdef __cplusplus

#include "../String"
#include "../Time"

namespace de {

/**
 * Version information about libcore. The version numbers are defined in libcore.pro.
 *
 * @note For the time being, this is separate from the project version number. libcore
 * versioning starts from 2.0.0. When the project as a whole switches to major version 2,
 * libcore version will be synced with the rest of the project. Also note that unlike in
 * the past, there is only ever three components in the version (or four, counting the
 * build number).
 *
 * @ingroup core
 */
class DENG2_PUBLIC Version
{
public:
    int major;
    int minor;
    int patch;
    int build;
    String label;           ///< Informative label, only intended for humans.
    String gitDescription;  ///< Output from "git describe".

    /**
     * Version information about this build.
     */
    Version();

    /**
     * Version information.
     *
     * @param version      Version number in the form "x.y.z".
     * @param buildNumber  Build number.
     */
    Version(String const &version, int buildNumber = 0);

    /**
     * Forms a version string in the form "x.y.z". If a release label is
     * defined, it will be included, too: "x.y.z (label)".
     */
    String base() const;

    /**
     * Forms a version string that includes the build number (unless it is
     * zero).
     */
    String asText() const;

    /**
     * Converts a textual version and updates the Version instance with the
     * values. The version has the following format: (major).(minor).(patch).
     * The release label is never part of the version string.
     *
     * @param version  Version string. Cannot include a label.
     */
    void parseVersionString(String const &version);

    bool operator < (Version const &other) const;

    bool operator == (Version const &other) const;

    bool operator != (Version const &other) const {
        return !(*this == other);
    }

    bool operator > (Version const &other) const;

    /**
     * Determines the operating system.
     */
    static String operatingSystem();

    static duint cpuBits();

    static bool isDebugBuild();
};

} // namespace de

#endif // __cplusplus

#endif // LIBDENG2_VERSION_H
