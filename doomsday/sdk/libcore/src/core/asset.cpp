/** @file asset.cpp  Information about the state of an asset (e.g., resource).
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

#include "de/Asset"

namespace de {

DENG2_PIMPL_NOREF(Asset)
{
    State state;

    Instance(State s) : state(s) {}
    Instance(Instance const &other) : de::IPrivate(), state(other.state) {}

    DENG2_PIMPL_AUDIENCE(StateChange)
    DENG2_PIMPL_AUDIENCE(Deletion)
};

DENG2_AUDIENCE_METHOD(Asset, StateChange)
DENG2_AUDIENCE_METHOD(Asset, Deletion)

Asset::Asset(State initialState) : d(new Instance(initialState))
{}

Asset::Asset(Asset const &other) : d(new Instance(*other.d))
{}

Asset::~Asset()
{
    DENG2_FOR_AUDIENCE2(Deletion, i) i->assetBeingDeleted(*this);
}

void Asset::setState(State s)
{
    State old = d->state;
    d->state = s;
    if(old != d->state)
    {
        DENG2_FOR_AUDIENCE2(StateChange, i) i->assetStateChanged(*this);
    }
}

void Asset::setState(bool assetReady)
{
    setState(assetReady? Ready : NotReady);
}

Asset::State Asset::state() const
{
    return d->state;
}

bool Asset::isReady() const
{
    return d->state == Ready;
}

//----------------------------------------------------------------------------

DENG2_PIMPL_NOREF(AssetGroup)
{
    Members deps;

    /**
     * Determines if all the assets in the group are ready.
     */
    bool allReady() const
    {
        DENG2_FOR_EACH_CONST(Members, i, deps)
        {
            switch(i->second)
            {
            case Required:
                if(!i->first->isReady()) return false;
                break;

            default:
                break;
            }
        }
        // Yay.
        return true;
    }

    void update(AssetGroup &self)
    {
        self.setState(allReady()? Ready : NotReady);
    }
};

AssetGroup::AssetGroup() : d(new Instance)
{
    // An empty set of members means the group is Ready.
    setState(Ready);
}

AssetGroup::~AssetGroup()
{
    // We are about to be deleted.
    audienceForStateChange().clear();

    clear();
}

dint AssetGroup::size() const
{
    return dint(d->deps.size());
}

void AssetGroup::clear()
{
    DENG2_FOR_EACH(Members, i, d->deps)
    {
        i->first->audienceForDeletion() -= this;
        i->first->audienceForStateChange() -= this;
    }

    d->deps.clear();
    d->update(*this);
}

void AssetGroup::insert(Asset const &asset, Policy policy)
{
    d->deps[&asset] = policy;
    asset.audienceForDeletion() += this;
    asset.audienceForStateChange() += this;
    d->update(*this);
}

void AssetGroup::remove(Asset const &asset)
{
    asset.audienceForDeletion() -= this;
    asset.audienceForStateChange() -= this;
    d->deps.erase(&asset);
    d->update(*this);
}

bool AssetGroup::has(Asset const &asset) const
{
    return d->deps.find(&asset) != d->deps.end();
}

void AssetGroup::setPolicy(Asset const &asset, Policy policy)
{
    DENG2_ASSERT(d->deps.find(&asset) != d->deps.end());

    d->deps[&asset] = policy;
    d->update(*this);
}

AssetGroup::Members const &AssetGroup::all() const
{
    return d->deps;
}

void AssetGroup::assetBeingDeleted(Asset &asset)
{
    if(has(asset))
    {
        remove(asset);
    }
}

void AssetGroup::assetStateChanged(Asset &)
{
    d->update(*this);
}

} // namespace de
