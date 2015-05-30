/** @file biastracker.h Shadow Bias illumination change tracker.
 *
 * @authors Copyright © 2005-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_RENDER_SHADOWBIAS_TRACKER_H
#define DENG_RENDER_SHADOWBIAS_TRACKER_H

#include <de/Error>
#include <de/Vector>

#include "BiasIllum"

class BiasDigest;
class BiasSource;

/**
 * Map point illumination tracker for the Shadow Bias lighting model.
 *
 * @ingroup render
 */
class BiasTracker
{
public:
    /// An unknown light contributor was referenced @ingroup errors
    DENG2_ERROR(UnknownContributorError);

    /// Maximum number of light contributors.
    static int const MAX_CONTRIBUTORS = BiasIllum::MAX_CONTRIBUTORS;

public:
    /**
     * Construct a new bias illumination tracker.
     */
    BiasTracker();

    /**
     * Remove all light contributors. Existing contributions are put into a
     * "latent" state, so that if they are added again the contribution is then
     * re-activated and no lighting changes will occur (appears seamless).
     *
     * @see addContributor()
     */
    void clearContributors();

    /**
     * Add a new light contributor. After which lighting changes at the source
     * will be tracked and routed to map point illuminations when necessary
     * (i.e., when lighting is next evaluated for the point).
     *
     * All contributors are assigned a unique index (when added) that can be
     * used to reference it (and the source) later.
     *
     * @note Contributors with intensity less than @ref BiasIllum::MIN_INTENSITY
     * will be ignored (nothing will happen).
     *
     * @note At most @ref MAX_CONTRIBUTORS can contribute lighting. Once this
     * capacity is reached adding a new contributor will result in the weakest
     * contributor (i.e., smallest intensity when added) being dropped and it's
     * index assigned to the 'new' contributor. If the weakest is the new
     * contributor then nothing will happen.
     *
     * @param source     Source of the light contribution.
     * @param intensity  Strength of the contribution from the source.
     *
     * @see contributor()
     *
     * @return  Index of the contributor otherwise @c -1 (if rejected). Note
     * that this index may be subsequently reassigned (see above notes).
     */
    int addContributor(BiasSource *source, float intensity);

    /**
     * Returns the source of a light contributor by @a index.
     *
     * @see addContributor()
     */
    BiasSource &contributor(int index) const;

    /**
     * Determine the earliest time in milliseconds that an affecting source
     * was changed/deleted.
     *
     * @see applyChanges()
     */
    uint timeOfLatestContributorUpdate() const;

    /**
     * Interpret the bias change digest and schedule illumination updates as
     * necessary (deferred, does not block).
     *
     * @param changes  Digest of all changes to apply in the tracker.
     */
    void applyChanges(BiasDigest &changes);

public: /// @todo The following API should be replaced -------------------------

    byte activeContributors() const;
    byte changedContributions() const;
    void updateAllContributors();
    void markIllumUpdateCompleted();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_RENDER_SHADOWBIAS_TRACKER_H
