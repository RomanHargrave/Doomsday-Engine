/** @file map.cpp  World map.
 *
 * @todo This file has grown far too large. It should be split up through the
 * introduction of new abstractions / collections.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2014 Daniel Swanson <danij@dengine.net>
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

#include "de_base.h"
#include "world/map.h"

#include "de_console.h" // Con_GetInteger
#include "de_defs.h"
#ifdef __CLIENT__
#  include "clientapp.h"
#endif
#include "m_nodepile.h"

#include "Face"

#include "BspLeaf"
#include "BspNode"
#include "ConvexSubspace"
#include "Line"
#include "Polyobj"
#include "Sector"
#include "SectorCluster"
#include "Surface"
#include "Vertex"

#include "world/bsp/partitioner.h"
#include "world/worldsystem.h" // ddMapSetup, validCount
#include "world/blockmap.h"
#include "world/lineblockmap.h"
#include "world/entitydatabase.h"
#include "world/lineowner.h"
#include "world/p_object.h"
#include "world/polyobjdata.h"
#include "world/sky.h"
#include "world/thinkers.h"
#ifdef __CLIENT__
#  include "Contact"
#  include "ContactSpreader"
#  include "client/cl_mobj.h"
#endif

#ifdef __CLIENT__
#  include "api_sound.h"
#endif

#ifdef __CLIENT__
#  include "BiasDigest"
#  include "LightDecoration"
#  include "Lumobj"
#  include "SurfaceDecorator"
#  include "WallEdge"
#  include "render/viewports.h"
#  include "render/rend_main.h"
#  include "render/rend_particle.h"
#  include "render/skydrawable.h"
#endif

#include <de/Rectangle>
#include <de/aabox.h>
#include <de/vector1.h>
#include <de/timer.h>
#include <doomsday/defs/mapinfo.h>
#include <doomsday/defs/sky.h>
#include <QBitArray>
#include <QMultiMap>
#include <QVarLengthArray>

static int bspSplitFactor = 7; // cvar

#ifdef __CLIENT__
static int lgMXSample  = 1; ///< 5 samples per block. Cvar.

/// Milliseconds it takes for Unpredictable and Hidden mobjs to be
/// removed from the hash. Under normal circumstances, the special
/// status should be removed fairly quickly.
#define CLMOBJ_TIMEOUT  4000
#endif

namespace internal
{
    static inline de::WorldSystem &worldSys()
    {
        return App_WorldSystem();
    }

}  // namespace internal
using namespace internal;

namespace de {

struct EditableElements
{
    typedef QList<Line *> Lines;
    Lines lines;

    typedef QList<Sector *> Sectors;
    Sectors sectors;

    typedef QList<Polyobj *> Polyobjs;
    Polyobjs polyobjs;

    EditableElements()
    {}

    ~EditableElements()
    {
        clearAll();
    }

    void clearAll();
};

DENG2_PIMPL(Map)
, DENG2_OBSERVES(bsp::Partitioner, UnclosedSectorFound)
#ifdef __CLIENT__
, DENG2_OBSERVES(ThinkerData, Deletion)
#endif
{
    bool editingEnabled = true;
    EditableElements editable;

    MapDef *def;    ///< Definition for the map (not owned, may be @c NULL).
    AABoxd bounds;  ///< Boundary points which encompass the entire map

    Mesh mesh;      ///< All map geometries.

    typedef QList<Sector *> Sectors;
    Sectors sectors;

    typedef QList<Line *> Lines;
    Lines lines;

    typedef QList<Polyobj *> Polyobjs;
    Polyobjs polyobjs;

    struct Bsp
    {
        BspTree *tree; ///< Owns the BspElements.

        Bsp() : tree(0) {}
        ~Bsp() { clear(); }

        void clear()
        {
            if(!tree) return;
            tree->traversePostOrder(clearUserDataWorker);
            delete tree; tree = 0;
        }

    private:
        static int clearUserDataWorker(BspTree &subtree, void *)
        {
            delete subtree.userData();
            return 0;
        }
    } bsp;

    typedef QList<ConvexSubspace *> Subspaces;
    Subspaces subspaces;

    typedef QMultiMap<Sector *, SectorCluster *> SectorClusters;
    SectorClusters clusters;

    /// Map entities and element properties (things, line specials, etc...).
    QScopedPointer<Thinkers> thinkers;
    Sky sky;

    EntityDatabase entityDatabase;

    QScopedPointer<Blockmap> mobjBlockmap;
    QScopedPointer<Blockmap> polyobjBlockmap;
    QScopedPointer<LineBlockmap> lineBlockmap;
    QScopedPointer<Blockmap> subspaceBlockmap;

#ifdef __CLIENT__
    struct ContactBlockmap : public Blockmap
    {
        QBitArray spreadBlocks; ///< Used to prevent repeat processing.

        /**
         * Construct a new contact blockmap.
         *
         * @param bounds    Map space boundary.
         * @param cellSize  Width and height of a cell in map space units.
         */
        ContactBlockmap(AABoxd const &bounds, uint cellSize = 128)
            : Blockmap(bounds, cellSize),
              spreadBlocks(width() * height())
        {}

        void clear()
        {
            spreadBlocks.fill(false);
            unlinkAll();
        }

        /**
         * @param contact  Contact to be linked. Note that if the object's origin
         *                 lies outside the blockmap it will not be linked!
         */
        void link(Contact &contact)
        {
            bool outside;
            BlockmapCell cell = toCell(contact.objectOrigin(), &outside);
            if(!outside)
            {
                Blockmap::link(cell, &contact);
            }
        }

        void spread(AABoxd const &region)
        {
            spreadContacts(*this, region, &spreadBlocks);
        }
    };
    QScopedPointer<ContactBlockmap> mobjContactBlockmap; /// @todo Redundant?
    QScopedPointer<ContactBlockmap> lumobjContactBlockmap;
#endif

    nodepile_t mobjNodes;
    nodepile_t lineNodes;
    nodeindex_t *lineLinks = nullptr; ///< Indices to roots.

#ifdef __CLIENT__
    PlaneSet trackedPlanes;
    SurfaceSet scrollingSurfaces;

    /**
     * All (particle) generators.
     */
    struct Generators
    {
        struct ListNode
        {
            ListNode *next;
            Generator *gen;
        };

        Generator *activeGens[MAX_GENERATORS];

        // We can link 64 generators each into four lists each before running out of links.
        static int const LINKSTORE_SIZE = 4 * MAX_GENERATORS;
        ListNode *linkStore;
        uint linkStoreCursor;

        uint listsSize;
        // Array of list heads containing links from linkStore to generators in activeGens.
        ListNode **lists;

        Generators()
            : linkStore(0)
            , linkStoreCursor(0)
            , listsSize(0)
            , lists(0)
        {}

        ~Generators()
        {
            Z_Free(lists);
            Z_Free(linkStore);
        }

        /**
         * Resize the collection.
         *
         * @param listCount  Number of lists the collection must support.
         */
        void resize(uint listCount)
        {
            if(!linkStore)
            {
                linkStore = (ListNode *) Z_Malloc(sizeof(*linkStore) * LINKSTORE_SIZE, PU_MAP, 0);
                linkStoreCursor = 0;
                zap(activeGens);
            }

            listsSize = listCount;
            lists = (ListNode **) Z_Realloc(lists, sizeof(ListNode*) * listsSize, PU_MAP);
        }

        /**
         * Returns an unused link from the linkStore.
         */
        ListNode *newLink()
        {
            if(linkStoreCursor < (unsigned)LINKSTORE_SIZE)
            {
                return &linkStore[linkStoreCursor++];
            }
            LOG_MAP_WARNING("Exhausted generator link storage");
            return 0;
        }
    };
    QScopedPointer<Generators> generators;

    QScopedPointer<LightGrid> lightGrid;

    /// Shadow Bias data.
    struct Bias
    {
        uint currentTime = 0;        ///< The "current" frame in milliseconds.
        uint lastChangeOnFrame = 0;

        typedef QList<BiasSource *> BiasSources;
        BiasSources sources;         ///< All bias light sources (owned).
    } bias;

    typedef QList<Lumobj *> Lumobjs;
    Lumobjs lumobjs;  ///< All lumobjs (owned).

    QScopedPointer<SurfaceDecorator> decorator;

    coord_t skyFloorHeight   = DDMAXFLOAT;
    coord_t skyCeilingHeight = DDMINFLOAT;

    ClMobjHash clMobjHash;
#endif

    Instance(Public *i) : Base(i)
    {
        sky.setMap(thisPublic);
        sky.setIndexInMap(0);
    }

    ~Instance()
    {
        DENG2_FOR_PUBLIC_AUDIENCE(Deletion, i) i->mapBeingDeleted(self);

#ifdef __CLIENT__
        self.removeAllLumobjs();
        self.removeAllBiasSources();
#endif

        // Delete thinkers before the map elements, because thinkers may reference them
        // in their private data destructors.
        thinkers.reset();

        qDeleteAll(clusters);
        qDeleteAll(subspaces);
        qDeleteAll(sectors);

        for(Polyobj *polyobj : polyobjs)
        {
            polyobj->~Polyobj();
            M_Free(polyobj);
        }
        qDeleteAll(lines);

#ifdef __CLIENT__        
        // Stop observing client mobjs.
        for(mobj_t *mo : clMobjHash)
        {
            THINKER_DATA(mo->thinker, ThinkerData).audienceForDeletion() -= this;
        }
#endif

        /// @todo fixme: Free all memory we have ownership of.
        // mobjNodes/lineNodes/lineLinks
    }

    /**
     * @pre Axis-aligned bounding boxes of all Sectors must be initialized.
     */
    void updateBounds()
    {
        bool haveGeometry = false;
        for(Line *line : lines)
        {
            // Polyobj lines don't count.
            if(line->definesPolyobj()) continue;

            if(haveGeometry)
            {
                // Expand the bounding box.
                V2d_UniteBox(bounds.arvec2, line->aaBox().arvec2);
            }
            else
            {
                // The first line's bounds are used as is.
                V2d_CopyBox(bounds.arvec2, line->aaBox().arvec2);
                haveGeometry = true;
            }
        }
    }

    // Observes bsp::Partitioner UnclosedSectorFound.
    void unclosedSectorFound(Sector &sector, Vector2d const &nearPoint)
    {
        // Notify interested parties that an unclosed sector was found.
        DENG2_FOR_PUBLIC_AUDIENCE(UnclosedSectorFound, i) i->unclosedSectorFound(sector, nearPoint);
    }

    /**
     * Notify interested parties of a "one-way window" in the map.
     *
     * @param line              The window line.
     * @param backFacingSector  Sector that the back of the line is facing.
     */
    void notifyOneWayWindowFound(Line &line, Sector &backFacingSector)
    {
        DENG2_FOR_PUBLIC_AUDIENCE(OneWayWindowFound, i) i->oneWayWindowFound(line, backFacingSector);
    }

    struct testForWindowEffectParams
    {
        double frontDist    = 0;
        double backDist     = 0;
        Sector *frontOpen   = nullptr;
        Sector *backOpen    = nullptr;
        Line *frontLine     = nullptr;
        Line *backLine      = nullptr;
        Line *testLine      = nullptr;
        bool castHorizontal = false;
        Vector2d testLineCenter;
    };

    static void testForWindowEffect2(Line &line, testForWindowEffectParams &p)
    {
        if(&line == p.testLine) return;
        if(line.isSelfReferencing()) return;
        if(line.hasZeroLength()) return;

        double dist = 0;
        Sector *hitSector = 0;
        bool isFront = false;
        if(p.castHorizontal)
        {
            if(de::abs(line.direction().y) < bsp::DIST_EPSILON)
                return;

            if((line.aaBox().maxY < p.testLineCenter.y - bsp::DIST_EPSILON) ||
               (line.aaBox().minY > p.testLineCenter.y + bsp::DIST_EPSILON))
                return;

            dist = (line.fromOrigin().x +
                    (p.testLineCenter.y - line.fromOrigin().y) * line.direction().x / line.direction().y)
                   - p.testLineCenter.x;

            isFront = ((p.testLine->direction().y > 0) != (dist > 0));
            dist = de::abs(dist);

            // Too close? (overlapping lines?)
            if(dist < bsp::DIST_EPSILON)
                return;

            bool dir = (p.testLine->direction().y > 0) ^ (line.direction().y > 0);
            hitSector = line.side(dir ^ !isFront).sectorPtr();
        }
        else // Cast vertically.
        {
            if(de::abs(line.direction().x) < bsp::DIST_EPSILON)
                return;

            if((line.aaBox().maxX < p.testLineCenter.x - bsp::DIST_EPSILON) ||
               (line.aaBox().minX > p.testLineCenter.x + bsp::DIST_EPSILON))
                return;

            dist = (line.fromOrigin().y +
                    (p.testLineCenter.x - line.fromOrigin().x) * line.direction().y / line.direction().x)
                   - p.testLineCenter.y;

            isFront = ((p.testLine->direction().x > 0) == (dist > 0));
            dist = de::abs(dist);

            bool dir = (p.testLine->direction().x > 0) ^ (line.direction().x > 0);
            hitSector = line.side(dir ^ !isFront).sectorPtr();
        }

        // Too close? (overlapping lines?)
        if(dist < bsp::DIST_EPSILON)
            return;

        if(isFront)
        {
            if(dist < p.frontDist)
            {
                p.frontDist = dist;
                p.frontOpen = hitSector;
                p.frontLine = &line;
            }
        }
        else
        {
            if(dist < p.backDist)
            {
                p.backDist = dist;
                p.backOpen = hitSector;
                p.backLine = &line;
            }
        }
    }

    bool lineMightHaveWindowEffect(Line const &line)
    {
        if(line.definesPolyobj()) return false;
        if(line.hasFrontSector() && line.hasBackSector()) return false;
        if(!line.hasFrontSector()) return false;
        if(line.hasZeroLength()) return false;

        // Look for window effects by checking for an odd number of one-sided
        // line owners for a single vertex. Idea courtesy of Graham Jackson.
        if((line.from()._onesOwnerCount % 2) == 1 &&
           (line.from()._onesOwnerCount + line.from()._twosOwnerCount) > 1)
            return true;

        if((line.to()._onesOwnerCount % 2) == 1 &&
           (line.to()._onesOwnerCount + line.to()._twosOwnerCount) > 1)
            return true;

        return false;
    }

    void findOneWayWindows()
    {
        for(Vertex *vertex : mesh.vertexs())
        {
            // Count the total number of one and two-sided line owners for each
            // vertex. (Used in the process of locating window effect lines.)
            vertex->countLineOwners();
        }

        // Search for "one-way window" effects.
        for(Line *line : lines)
        {
            if(!lineMightHaveWindowEffect(*line))
                continue;

            testForWindowEffectParams p;
            p.frontDist      = p.backDist = DDMAXFLOAT;
            p.testLine       = line;
            p.testLineCenter = line->center();
            p.castHorizontal = (de::abs(line->direction().x) < de::abs(line->direction().y)? true : false);

            AABoxd scanRegion = bounds;
            if(p.castHorizontal)
            {
                scanRegion.minY = line->aaBox().minY - bsp::DIST_EPSILON;
                scanRegion.maxY = line->aaBox().maxY + bsp::DIST_EPSILON;
            }
            else
            {
                scanRegion.minX = line->aaBox().minX - bsp::DIST_EPSILON;
                scanRegion.maxX = line->aaBox().maxX + bsp::DIST_EPSILON;
            }

            validCount++;
            self.forAllLinesInBox(scanRegion, LIF_SECTOR, [&p] (Line &line)
            {
                testForWindowEffect2(line, p);
                return LoopContinue;
            });

            if(p.backOpen && p.frontOpen && line->frontSectorPtr() == p.backOpen)
            {
                notifyOneWayWindowFound(*line, *p.frontOpen);

                line->_bspWindowSector = p.frontOpen; /// @todo Refactor away.
            }
        }
    }

    /**
     * Build a new BSP tree.
     *
     * @pre Map line bounds have been determined and a line blockmap constructed.
     */
    bool buildBspTree()
    {
        DENG2_ASSERT(bsp.tree == nullptr);
        DENG2_ASSERT(subspaces.isEmpty());

        // It begins...
        Time begunAt;

        LOGDEV_MAP_XVERBOSE("Building BSP for \"%s\" with split cost factor %d...")
                << (def? def->composeUri() : "(unknown map)") << bspSplitFactor;

        // First we'll scan for so-called "one-way window" constructs and mark
        // them so that the space partitioner can treat them specially.
        findOneWayWindows();

        // Remember the current next vertex ordinal as we'll need to index any
        // new vertexes produced during the build process.
        int nextVertexOrd = mesh.vertexCount();

        // Determine the set of lines for which we will build a BSP.
        QSet<Line *> linesToBuildFor = QSet<Line *>::fromList(lines);

        // Polyobj lines should be excluded.
        for(Polyobj *po : polyobjs)
        for(Line *line : po->lines())
        {
            linesToBuildFor.remove(line);
        }

        try
        {
            // Configure a space partitioner.
            bsp::Partitioner partitioner(bspSplitFactor);
            partitioner.audienceForUnclosedSectorFound += this;

            // Build a new BSP tree.
            bsp.tree = partitioner.makeBspTree(linesToBuildFor, mesh);
            DENG2_ASSERT(bsp.tree);

            LOG_MAP_VERBOSE("BSP built: %s. With %d Segments and %d Vertexes.")
                    << bsp.tree->summary()
                    << partitioner.segmentCount()
                    << partitioner.vertexCount();

            // Attribute an index to any new vertexes.
            for(int i = nextVertexOrd; i < mesh.vertexCount(); ++i)
            {
                Vertex *vtx = mesh.vertexs().at(i);
                vtx->setMap(thisPublic);
                vtx->setIndexInMap(i);
            }

#ifdef DENG2_QT_4_7_OR_NEWER
            /// @todo Determine the actual number of subspaces needed.
            subspaces.reserve(bsp.tree->leafCount());
#endif

            // Iterative pre-order traversal of the map element tree.
            BspTree const *cur  = bsp.tree;
            BspTree const *prev = nullptr;
            while(cur)
            {
                while(cur)
                {
                    if(cur->userData())
                    {
                        if(cur->isLeaf())
                        {
                            BspLeaf &leaf = cur->userData()->as<BspLeaf>();
                            if(!leaf.sectorPtr())
                            {
                                LOG_MAP_WARNING("BSP leaf %p has degenerate geometry (%d half-edges).")
                                        << &leaf << (leaf.hasSubspace()? leaf.subspace().poly().hedgeCount() : 0);
                            }

                            if(leaf.hasSubspace())
                            {
                                // Add this subspace to the LUT.
                                ConvexSubspace &subspace = leaf.subspace();
                                subspace.setIndexInMap(subspaces.count());
                                subspaces.append(&subspace);

#ifdef DENG_DEBUG // See if we received a partial geometry...
                                int discontinuities = 0;
                                HEdge *hedge = subspace.poly().hedge();
                                do
                                {
                                    if(hedge->next().origin() != hedge->twin().origin())
                                    {
                                        discontinuities++;
                                    }
                                } while((hedge = &hedge->next()) != subspace.poly().hedge());

                                if(discontinuities)
                                {
                                    LOG_MAP_WARNING("Face geometry for BSP leaf [%p] at %s in sector %i "
                                                    "is not contiguous (%i gaps/overlaps).\n%s")
                                            << &leaf << subspace.poly().center().asText()
                                            << (leaf.sectorPtr()? leaf.sectorPtr()->indexInArchive() : -1)
                                            << discontinuities
                                            << subspace.poly().description();
                                }
#endif
                            }
                        }
                    }

                    if(prev == cur->parentPtr())
                    {
                        // Descending - right first, then left.
                        prev = cur;
                        if(cur->hasRight()) cur = cur->rightPtr();
                        else                cur = cur->leftPtr();
                    }
                    else if(prev == cur->rightPtr())
                    {
                        // Last moved up the right branch - descend the left.
                        prev = cur;
                        cur = cur->leftPtr();
                    }
                    else if(prev == cur->leftPtr())
                    {
                        // Last moved up the left branch - continue upward.
                        prev = cur;
                        cur = cur->parentPtr();
                    }
                }

                if(prev)
                {
                    // No left child - back up.
                    cur = prev->parentPtr();
                }
            }
        }
        catch(Error const &er)
        {
            LOG_MAP_WARNING("%s.") << er.asText();
        }

        // How much time did we spend?
        LOGDEV_MAP_VERBOSE("BSP built in %.2f seconds") << begunAt.since();

        return bsp.tree != nullptr;
    }

    /**
     * (Re)Build subspace clusters for the sector.
     */
    void buildClusters(Sector &sector)
    {
        for(auto it = clusters.find(&sector); it != clusters.end() && it.key() == &sector; )
        {
            delete *it;
        }
        clusters.remove(&sector);

        typedef QList<ConvexSubspace *> Subspaces;
        typedef QList<Subspaces> SubspaceSets;
        SubspaceSets subspaceSets;

        /*
         * Separate the subspaces into edge-adjacency clusters. We'll do this by
         * starting with a set per subspace and then keep merging these sets until
         * no more shared edges are found.
         */
        for(ConvexSubspace *subspace : subspaces)
        {
            if(subspace->bspLeaf().sectorPtr() == &sector)
            {
                subspaceSets.append(Subspaces());
                subspaceSets.last().append(subspace);
            }
        }

        if(subspaceSets.isEmpty()) return;

        // Merge sets whose subspaces share a common edge.
        while(subspaceSets.count() > 1)
        {
            bool didMerge = false;
            for(int i = 0; i < subspaceSets.count(); ++i)
            for(int k = 0; k < subspaceSets.count(); ++k)
            {
                if(i == k) continue;

                for(ConvexSubspace *subspace : subspaceSets[i])
                {
                    HEdge *baseHEdge = subspace->poly().hedge();
                    HEdge *hedge = baseHEdge;
                    do
                    {
                        if(hedge->twin().hasFace() &&
                           hedge->twin().face().hasMapElement())
                        {
                            ConvexSubspace &otherSubspace = hedge->twin().face().mapElementAs<ConvexSubspace>();
                            if(otherSubspace.bspLeaf().sectorPtr() == &sector &&
                               subspaceSets[k].contains(&otherSubspace))
                            {
                                // Merge k into i.
                                subspaceSets[i].append(subspaceSets[k]);
                                subspaceSets.removeAt(k);

                                // Compare the next pair.
                                if(i >= k) i -= 1;
                                k -= 1;

                                // We'll need to repeat in any case.
                                didMerge = true;
                                break;
                            }
                        }
                    } while((hedge = &hedge->next()) != baseHEdge);

                    if(didMerge) break;
                }
            }

            if(!didMerge) break;
        }
        // Clustering complete.

        // Build clusters.
        for(Subspaces const &subspaceSet : subspaceSets)
        {
            // Subspace ownership is not given to the cluster.
            clusters.insert(&sector, new SectorCluster(subspaceSet));
        }
    }

    /// @return  @c true= @a mobj was unlinked successfully.
    bool unlinkMobjFromSectors(mobj_t &mobj)
    {
        if(Mobj_IsSectorLinked(&mobj))
        {
            Mobj_Sector(&mobj)->unlink(&mobj);
            return true;
        }
        return false;
    }

    /**
     * Construct an initial (empty) line blockmap.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initLineBlockmap(ddouble margin = 8)
    {
        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        lineBlockmap.reset(
            new LineBlockmap(AABoxd(bounds.minX - margin, bounds.minY - margin,
                                    bounds.maxX + margin, bounds.maxY + margin)));

        LOG_MAP_VERBOSE("Line blockmap dimensions:")
            << lineBlockmap->dimensions().asText();

        // Populate the blockmap.
        lineBlockmap->link(lines);
    }

    /**
     * Construct an initial (empty) mobj blockmap.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initMobjBlockmap(ddouble margin = 8)
    {
        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        mobjBlockmap.reset(
            new Blockmap(AABoxd(bounds.minX - margin, bounds.minY - margin,
                                bounds.maxX + margin, bounds.maxY + margin)));

        LOG_MAP_VERBOSE("Mobj blockmap dimensions:")
            << mobjBlockmap->dimensions().asText();
    }

    /**
     * Unlinks the mobj from all the lines it's been linked to. Can be called
     * without checking that the list does indeed contain lines.
     */
    bool unlinkMobjFromLines(mobj_t &mo)
    {
        // Try unlinking from lines.
        if(!mo.lineRoot)
            return false; // A zero index means it's not linked.

        // Unlink from each line.
        linknode_t *tn = mobjNodes.nodes;
        for(nodeindex_t nix = tn[mo.lineRoot].next; nix != mo.lineRoot;
            nix = tn[nix].next)
        {
            // Data is the linenode index that corresponds this mobj.
            NP_Unlink((&lineNodes), tn[nix].data);
            // We don't need these nodes any more, mark them as unused.
            // Dismissing is a macro.
            NP_Dismiss((&lineNodes), tn[nix].data);
            NP_Dismiss((&mobjNodes), nix);
        }

        // The mobj no longer has a line ring.
        NP_Dismiss((&mobjNodes), mo.lineRoot);
        mo.lineRoot = 0;

        return true;
    }

    /**
     * @note Caller must ensure a mobj is linked only once to any given line.
     *
     * @param mo    Mobj to be linked.
     * @param line  Line to link the mobj to.
     */
    void linkMobjToLine(mobj_t *mo, Line *line)
    {
        if(!mo || !line) return;

        // Lines with only one sector will not be linked to because a mobj can't
        // legally cross one.
        if(!line->hasFrontSector()) return;
        if(!line->hasBackSector()) return;

        // Add a node to the mobj's ring.
        nodeindex_t nodeIndex = NP_New(&mobjNodes, line);
        NP_Link(&mobjNodes, nodeIndex, mo->lineRoot);

        // Add a node to the line's ring. Also store the linenode's index
        // into the mobjring's node, so unlinking is easy.
        nodeIndex = mobjNodes.nodes[nodeIndex].data = NP_New(&lineNodes, mo);
        NP_Link(&lineNodes, nodeIndex, lineLinks[line->indexInMap()]);
    }

    /**
     * @note Caller must ensure that the mobj is @em not linked.
     */
    void linkMobjToLines(mobj_t &mo)
    {
        AABoxd const box = Mobj_AABox(mo);

        // Get a new root node.
        mo.lineRoot = NP_New(&mobjNodes, NP_ROOT_NODE);

        validCount++;
        self.forAllLinesInBox(box, [this, &mo, &box] (Line &line)
        {
            // Do the bounding boxes intercept?
            if(!(box.minX >= line.aaBox().maxX ||
                 box.minY >= line.aaBox().maxY ||
                 box.maxX <= line.aaBox().minX ||
                 box.maxY <= line.aaBox().minY))
            {
                // Line crosses the mobj's bounding box?
                if(!line.boxOnSide(box))
                {
                    this->linkMobjToLine(&mo, &line);
                }
            }
            return LoopContinue;
        });
    }

    /**
     * Construct an initial (empty) polyobj blockmap.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initPolyobjBlockmap(ddouble margin = 8)
    {
        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        polyobjBlockmap.reset(
            new Blockmap(AABoxd(bounds.minX - margin, bounds.minY - margin,
                                bounds.maxX + margin, bounds.maxY + margin)));

        LOG_MAP_VERBOSE("Polyobj blockmap dimensions:")
            << polyobjBlockmap->dimensions().asText();
    }

    /**
     * Construct an initial (empty) convex subspace blockmap.
     *
     * @pre Coordinate space bounds have already been determined.
     */
    void initSubspaceBlockmap(ddouble margin = 8)
    {
        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        subspaceBlockmap.reset(
            new Blockmap(AABoxd(bounds.minX - margin, bounds.minY - margin,
                                bounds.maxX + margin, bounds.maxY + margin)));

        LOG_MAP_VERBOSE("Convex subspace blockmap dimensions:")
            << subspaceBlockmap->dimensions().asText();

        // Populate the blockmap.
        foreach(ConvexSubspace *subspace, subspaces)
        {
            subspaceBlockmap->link(subspace->poly().aaBox(), subspace);
        }
    }

#ifdef __CLIENT__
    void initContactBlockmaps(ddouble margin = 8)
    {
        // Setup the blockmap area to enclose the whole map, plus a margin
        // (margin is needed for a map that fits entirely inside one blockmap cell).
        AABoxd expandedBounds(bounds.minX - margin, bounds.minY - margin,
                              bounds.maxX + margin, bounds.maxY + margin);

        mobjContactBlockmap.reset(new ContactBlockmap(expandedBounds));
        lumobjContactBlockmap.reset(new ContactBlockmap(expandedBounds));
    }

    /**
     * Returns the appropriate contact blockmap for the specified contact @a type.
     */
    ContactBlockmap &contactBlockmap(ContactType type)
    {
        switch(type)
        {
        case ContactMobj:   return *mobjContactBlockmap;
        case ContactLumobj: return *lumobjContactBlockmap;

        default:
            throw Error("Map::contactBlockmap", "Invalid type");
        }
    }

    static int linkContactWorker(Contact &contact, void *context)
    {
        static_cast<Instance *>(context)->contactBlockmap(contact.type()).link(contact);
        return false; // Continue iteration.
    }

    /**
     * To be called to link all contacts into the contact blockmaps.
     *
     * @todo Why don't we link contacts immediately? -ds
     */
    void linkAllContacts()
    {
        R_ContactIterator(linkContactWorker, this);
    }

    // Clear the "contact" blockmaps (BSP leaf => object).
    void removeAllContacts()
    {
        mobjContactBlockmap->clear();
        lumobjContactBlockmap->clear();

        R_ClearContactLists(*thisPublic);
    }
#endif

    /**
     * Locate a polyobj by sound emitter.
     *
     * @param soundEmitter  SoundEmitter to search for.
     *
     * @return  Pointer to the referenced Polyobj instance; otherwise @c 0.
     */
    Polyobj *polyobjBySoundEmitter(SoundEmitter const &soundEmitter) const
    {
        for(Polyobj *polyobj : polyobjs)
        {
            if(&soundEmitter == &polyobj->soundEmitter())
                return polyobj;
        }
        return nullptr; // Not found.
    }

    /**
     * Locate a sector by sound emitter.
     *
     * @param soundEmitter  SoundEmitter to search for.
     *
     * @return  Pointer to the referenced Sector instance; otherwise @c 0.
     */
    Sector *sectorBySoundEmitter(SoundEmitter const &soundEmitter) const
    {
        for(Sector *sector : sectors)
        {
            if(&soundEmitter == &sector->soundEmitter())
                return sector;
        }
        return nullptr; // Not found.
    }

    /**
     * Locate a sector plane by sound emitter.
     *
     * @param soundEmitter  SoundEmitter to search for.
     *
     * @return  Pointer to the referenced Plane instance; otherwise @c 0.
     */
    Plane *planeBySoundEmitter(SoundEmitter const &soundEmitter) const
    {
        Plane *found = nullptr;  // Not found.
        for(Sector *sector : sectors)
        {
            LoopResult located = sector->forAllPlanes([&soundEmitter, &found] (Plane &plane)
            {
                if(&soundEmitter == &plane.soundEmitter())
                {
                    found = &plane;
                    return LoopAbort;
                }
                return LoopContinue;
            });
            if(located) break;
        }
        return found;
    }

    /**
     * Locate a surface by sound emitter.
     *
     * @param soundEmitter  SoundEmitter to search for.
     *
     * @return  Pointer to the referenced Surface instance; otherwise @c 0.
     */
    Surface *surfaceBySoundEmitter(SoundEmitter const &soundEmitter) const
    {
        // Perhaps a wall surface?
        for(Line *line : lines)
        for(int i = 0; i < 2; ++i)
        {
            LineSide &side = line->side(i);
            if(!side.hasSections()) continue;

            if(&soundEmitter == &side.middleSoundEmitter())
            {
                return &side.middle();
            }
            if(&soundEmitter == &side.bottomSoundEmitter())
            {
                return &side.bottom();
            }
            if(&soundEmitter == &side.topSoundEmitter())
            {
                return &side.top();
            }
        }

        return nullptr; // Not found.
    }

#ifdef __CLIENT__
    SurfaceDecorator &surfaceDecorator()
    {
        if(decorator.isNull())
        {
            decorator.reset(new SurfaceDecorator);
        }
        return *decorator;
    }

    /**
     * Interpolate the smoothed height of planes.
     */
    void lerpTrackedPlanes(bool resetNextViewer)
    {
        if(resetNextViewer)
        {
            // Reset the plane height trackers.
            for(Plane *plane : trackedPlanes)
            {
                plane->resetSmoothedHeight();
            }

            // Tracked movement is now all done.
            trackedPlanes.clear();
        }
        // While the game is paused there is no need to smooth.
        else //if(!clientPaused)
        {
            QMutableSetIterator<Plane *> iter(trackedPlanes);
            while(iter.hasNext())
            {
                Plane *plane = iter.next();

                plane->lerpSmoothedHeight();

                // Has this plane reached its destination?
                if(de::fequal(plane->heightSmoothed(), plane->height()))
                {
                    iter.remove();
                }
            }
        }
    }

    /**
     * Interpolate the smoothed material origin of surfaces.
     */
    void lerpScrollingSurfaces(bool resetNextViewer)
    {
        if(resetNextViewer)
        {
            // Reset the surface material origin trackers.
            for(Surface *surface : scrollingSurfaces)
            {
                surface->resetSmoothedMaterialOrigin();
            }

            // Tracked movement is now all done.
            scrollingSurfaces.clear();
        }
        // While the game is paused there is no need to smooth.
        else //if(!clientPaused)
        {
            QMutableSetIterator<Surface *> iter(scrollingSurfaces);
            while(iter.hasNext())
            {
                Surface *surface = iter.next();

                surface->lerpSmoothedMaterialOrigin();

                // Has this material reached its destination?
                if(surface->materialOriginSmoothed() == surface->materialOrigin())
                {
                    iter.remove();
                }
            }
        }
    }

    /**
     * Perform preprocessing which must be done before rendering a frame.
     */
    void biasBeginFrame()
    {
        if(!useBias) return;

        // The time that applies on this frame.
        bias.currentTime = Timer_RealMilliseconds();

        // Check which sources have changed and update the trackers for any
        // affected surfaces.
        BiasDigest allChanges;
        bool needUpdateSurfaces = false;

        for(int i = 0; i < bias.sources.count(); ++i)
        {
            BiasSource *bsrc = bias.sources.at(i);

            if(bsrc->trackChanges(allChanges, i, bias.currentTime))
            {
                // We'll need to redetermine source => surface affection.
                needUpdateSurfaces = true;
            }
        }

        if(!needUpdateSurfaces) return;

        /*
         * Apply changes to all surfaces:
         */
        bias.lastChangeOnFrame = R_FrameCount();
        for(SectorCluster *cluster : clusters)
        {
            cluster->applyBiasDigest(allChanges);
        }
    }

    /**
     * Create new mobj => BSP leaf contacts.
     */
    void generateMobjContacts()
    {
        for(Sector *sector : sectors)
        for(mobj_t *iter = sector->firstMobj(); iter; iter = iter->sNext)
        {
            R_AddContact(*iter);
        }
    }

    void generateLumobjs(Surface const &surface)
    {
        surface.forAllDecorations([this] (Decoration &decor)
        {
            if(LightDecoration const *decorLight = decor.maybeAs<LightDecoration>())
            {
                std::unique_ptr<Lumobj> lum(decorLight->generateLumobj());
                if(lum)
                {
                    self.addLumobj(*lum); // a copy is made.
                }
            }
            return LoopContinue;
        });
    }

    /**
     * Perform lazy initialization of the generator collection.
     */
    Generators &getGenerators()
    {
        // Time to initialize a new collection?
        if(generators.isNull())
        {
            generators.reset(new Generators);
            generators->resize(sectors.count());
        }
        return *generators;
    }

    /**
     * Lookup the next available generator id.
     *
     * @todo Optimize: Cache this result.
     * @todo Optimize: We could maintain an age-sorted list of generators.
     *
     * @return  The next available id else @c 0 iff there are no unused ids.
     */
    Generator::Id findIdForNewGenerator()
    {
        Generators &gens = getGenerators();

        // Prefer allocating a new generator if we've a spare id.
        Generator::Id id = 0;
        for(; id < MAX_GENERATORS; ++id)
        {
            if(!gens.activeGens[id]) break;
        }
        if(id < MAX_GENERATORS) return id + 1;

        // See if there is an active, non-static generator we can supplant.
        Generator *oldest = 0;
        for(Generator::Id i = 0; i < MAX_GENERATORS; ++i)
        {
            Generator *gen = gens.activeGens[i];
            if(!gen || gen->isStatic()) continue;

            if(!oldest || gen->age() > oldest->age())
            {
                oldest = gen;
            }
        }

        return (oldest? oldest->id() : 0);
    }

    void spawnMapParticleGens()
    {
        if(!def) return;

        for(int i = 0; i < defs.ptcGens.size(); ++i)
        {
            ded_ptcgen_t *genDef = &defs.ptcGens[i];

            if(!genDef->map) continue;

            if(*genDef->map != def->composeUri())
                continue;

            // Are we still spawning using this generator?
            if(genDef->spawnAge > 0 && worldSys().time() > genDef->spawnAge)
                continue;

            Generator *gen = self.newGenerator();
            if(!gen) return; // No more generators.

            // Initialize the particle generator.
            gen->count = genDef->particles;
            gen->spawnRateMultiplier = 1;

            gen->configureFromDef(genDef);
            gen->setUntriggered();

            // Is there a need to pre-simulate?
            gen->presimulate(genDef->preSim);
        }
    }

    /**
     * Spawns all type-triggered particle generators, regardless of whether
     * the type of mobj exists in the map or not (mobjs might be dynamically
     * created).
     */
    void spawnTypeParticleGens()
    {
        for(int i = 0; i < defs.ptcGens.size(); ++i)
        {
            ded_ptcgen_t *def = &defs.ptcGens[i];

            if(def->typeNum != DED_PTCGEN_ANY_MOBJ_TYPE && def->typeNum < 0)
                continue;

            Generator *gen = self.newGenerator();
            if(!gen) return; // No more generators.

            // Initialize the particle generator.
            gen->count = def->particles;
            gen->spawnRateMultiplier = 1;

            gen->configureFromDef(def);
            gen->type = def->typeNum;
            gen->type2 = def->type2Num;

            // Is there a need to pre-simulate?
            gen->presimulate(def->preSim);
        }
    }

    int findDefForGenerator(Generator *gen)
    {
        DENG2_ASSERT(gen != 0);

        // Search for a suitable definition.
        for(int i = 0; i < defs.ptcGens.size(); ++i)
        {
            ded_ptcgen_t *def = &defs.ptcGens[i];

            // A type generator?
            if(def->typeNum == DED_PTCGEN_ANY_MOBJ_TYPE && gen->type == DED_PTCGEN_ANY_MOBJ_TYPE)
            {
                return i+1; // Stop iteration.
            }
            if(def->typeNum >= 0 &&
               (gen->type == def->typeNum || gen->type2 == def->type2Num))
            {
                return i+1; // Stop iteration.
            }

            // A damage generator?
            if(gen->source && gen->source->type == def->damageNum)
            {
                return i+1; // Stop iteration.
            }

            // A flat generator?
            if(gen->plane && def->material)
            {
                try
                {
                    Material *defMat = &App_ResourceSystem().material(*def->material);

                    Material *mat = gen->plane->surface().materialPtr();
                    if(def->flags & Generator::SpawnFloor)
                        mat = gen->plane->sector().floorSurface().materialPtr();
                    if(def->flags & Generator::SpawnCeiling)
                        mat = gen->plane->sector().ceilingSurface().materialPtr();

                    // Is this suitable?
                    if(mat == defMat)
                    {
                        return i + 1; // 1-based index.
                    }

#if 0 /// @todo $revise-texture-animation
                    if(def->flags & PGF_GROUP)
                    {
                        // Generator triggered by all materials in the animation.
                        if(Material_IsGroupAnimated(defMat) && Material_IsGroupAnimated(mat) &&
                           &Material_AnimGroup(defMat) == &Material_AnimGroup(mat))
                        {
                            // Both are in this animation! This def will do.
                            return i + 1; // 1-based index.
                        }
                    }
#endif
                }
                catch(MaterialManifest::MissingMaterialError const &)
                {} // Ignore this error.
                catch(ResourceSystem::MissingManifestError const &)
                {} // Ignore this error.
            }

            // A state generator?
            if(gen->source && def->state[0] &&
               runtimeDefs.states.indexOf(gen->source->state) == Def_GetStateNum(def->state))
            {
                return i + 1; // 1-based index.
            }
        }

        return 0; // Not found.
    }

    /**
     * Update existing generators in the map following an engine reset.
     */
    void updateParticleGens()
    {
        Generators &gens = getGenerators();
        for(Generator::Id i = 0; i < MAX_GENERATORS; ++i)
        {
            // Only consider active generators.
            Generator *gen = gens.activeGens[i];
            if(!gen) continue;

            // Map generators cannot be updated (we have no means to reliably
            // identify them), so destroy them.
            if(gen->isUntriggered())
            {
                Generator_Delete(gen);
                continue; // Continue iteration.
            }

            if(int defIndex = findDefForGenerator(gen))
            {
                // Update the generator using the new definition.
                ded_ptcgen_t *def = &defs.ptcGens[defIndex - 1];
                gen->def = def;
            }
            else
            {
                // Nothing else we can do, destroy it.
                Generator_Delete(gen);
            }
        }

        // Re-spawn map generators.
        spawnMapParticleGens();
    }

    /**
     * Link all generated particles into the map so that they will be drawn.
     *
     * @todo Overkill?
     */
    void linkAllParticles()
    {
        //LOG_AS("Map::linkAllParticles");

        Generators &gens = getGenerators();

        // Empty all generator lists.
        std::memset(gens.lists, 0, sizeof(*gens.lists) * gens.listsSize);
        gens.linkStoreCursor = 0;

        if(useParticles)
        {
            for(Generator::Id id = 0; id < MAX_GENERATORS; ++id)
            {
                // Only consider active generators.
                Generator *gen = gens.activeGens[id];
                if(!gen) continue;

                ParticleInfo const *pInfo = gen->particleInfo();
                for(int i = 0; i < gen->count; ++i, pInfo++)
                {
                    if(pInfo->stage < 0 || !pInfo->bspLeaf)
                        continue;

                    int listIndex = pInfo->bspLeaf->sectorPtr()->indexInMap();
                    DENG2_ASSERT((unsigned)listIndex < gens.listsSize);

                    // Must check that it isn't already there...
                    bool found = false;
                    for(Generators::ListNode *it = gens.lists[listIndex]; it; it = it->next)
                    {
                        if(it->gen == gen)
                        {
                            // Warning message disabled as these are occuring so thick and fast
                            // that logging is pointless (and negatively affecting performance).
                            //LOGDEV_MAP_VERBOSE("Attempted repeat link of generator %p to list %u.")
                            //        << gen << listIndex;
                            found = true; // No, no...
                        }
                    }

                    if(found) continue;

                    // We need a new link.
                    if(Generators::ListNode *link = gens.newLink())
                    {
                        link->gen  = gen;
                        link->next = gens.lists[listIndex];
                        gens.lists[listIndex] = link;
                    }
                }
            }
        }
    }

    void thinkerBeingDeleted(thinker_s &th)
    {
        clMobjHash.remove(th.id);
    }
#endif // __CLIENT__
};

Map::Map(MapDef *mapDefinition) : d(new Instance(this))
{
    setDef(mapDefinition);
}

MapDef *Map::def() const
{
    return d->def;
}

void Map::setDef(MapDef *newMapDefinition)
{
    d->def = newMapDefinition;
}

Record const &Map::mapInfo() const
{
    return worldSys().mapInfoForMapUri(def()? def()->composeUri() : de::Uri("Maps:", RC_NULL));
}

Mesh const &Map::mesh() const
{
    return d->mesh;
}

bool Map::hasBspTree() const
{
    return d->bsp.tree != nullptr;
}

Map::BspTree const &Map::bspTree() const
{
    if(d->bsp.tree)
    {
        return *d->bsp.tree;
    }
    /// @throw MissingBspTreeError  Attempted with no BSP tree available.
    throw MissingBspTreeError("Map::bspTree", "No BSP tree is available");
}

#ifdef __CLIENT__

bool Map::hasLightGrid()
{
    return !d->lightGrid.isNull();
}

LightGrid &Map::lightGrid()
{
    if(!d->lightGrid.isNull())
    {
        return *d->lightGrid;
    }
    /// @throw MissingLightGrid Attempted with no LightGrid initialized.
    throw MissingLightGridError("Map::lightGrid", "No light grid is initialized");
}

void Map::initLightGrid()
{
    // Disabled?
    if(!Con_GetInteger("rend-bias-grid"))
        return;

    // Diagonal in maze arrangement of natural numbers.
    // Up to 65 samples per-block(!)
    static int const MSFACTORS = 7;
    static int multisample[] = {1, 5, 9, 17, 25, 37, 49, 65};

    // Time to initialize the LightGrid?
    if(!d->lightGrid.isNull())
    {
        d->lightGrid->updateIfNeeded();
        return;
    }

    Time begunAt;
    d->lightGrid.reset(new LightGrid(origin(), dimensions()));

    LightGrid &lg = *d->lightGrid;

    // Determine how many sector cluster samples we'll make per block and
    // allocate the tempoary storage.
    int const numSamples = multisample[de::clamp(0, lgMXSample, MSFACTORS)];
    QVector<Vector2d> samplePoints(numSamples);
    QVector<int>      sampleHits(numSamples);

    /**
     * It would be possible to only allocate memory for the unique
     * sample results. And then select the appropriate sample in the loop
     * for initializing the grid instead of copying the previous results in
     * the loop for acquiring the sample points.
     *
     * Calculate with the equation (number of unique sample points):
     *
     * ((1 + lgBlockHeight * lgMXSample) * (1 + lgBlockWidth * lgMXSample)) +
     *     (size % 2 == 0? numBlocks : 0)
     * OR
     *
     * We don't actually need to store the ENTIRE ssample array. It would be
     * sufficent to only store the results from the start of the previous row
     * to current col index. This would save a bit of memory.
     *
     * However until lightgrid init is finalized it would be rather silly to
     * optimize this much further.
     */

    // Allocate memory for all the sample results.
    QVector<SectorCluster *> ssamples((lg.dimensions().x * lg.dimensions().y) * numSamples);

    // Determine the size^2 of the samplePoint array plus its center.
    int size = 0, center = 0;
    if(numSamples > 1)
    {
        float f = sqrt(float(numSamples));

        if(std::ceil(f) != std::floor(f))
        {
            size = sqrt(float(numSamples - 1));
            center = 0;
        }
        else
        {
            size = (int) f;
            center = size+1;
        }
    }

    // Construct the sample point offset array.
    // This way we can use addition only during calculation of:
    // (dimensions.y * dimensions.x) * numSamples
    if(center == 0)
    {
        // Zero is the center so do that first.
        samplePoints[0] = Vector2d(lg.blockSize() / 2, lg.blockSize() / 2);
    }

    if(numSamples > 1)
    {
        double bSize = double(lg.blockSize()) / (size - 1);

        // Is there an offset?
        int idx = (center == 0? 1 : 0);

        for(int y = 0; y < size; ++y)
        for(int x = 0; x < size; ++x, ++idx)
        {
            samplePoints[idx] = Vector2d(de::round<double>(x * bSize),
                                         de::round<double>(y * bSize));
        }
    }

    // Acquire the sector clusters at ALL the sample points.
    for(int y = 0; y < lg.dimensions().y; ++y)
    for(int x = 0; x < lg.dimensions().x; ++x)
    {
        LightGrid::Index const blk = lg.toIndex(x, y);
        Vector2d const off(x * lg.blockSize(), y * lg.blockSize());

        int sampleOffset = 0;
        if(center == 0)
        {
            // Center point is not considered with the term 'size'.
            // Sample this point and place at index 0 (at the start of the
            // samples for this block).
            ssamples[blk * numSamples] = clusterAt(lg.origin() + off + samplePoints[0]);
            sampleOffset++;
        }

        int count = blk * size;
        for(int b = 0; b < size; ++b)
        {
            int i = (b + count) * size;

            for(int a = 0; a < size; ++a, ++sampleOffset)
            {
                int idx = a + i + (center == 0? blk + 1 : 0);

                if(numSamples > 1 && ((x > 0 && a == 0) || (y > 0 && b == 0)))
                {
                    // We have already sampled this point.
                    // Get the previous result.
                    LightGrid::Ref prev(x, y);
                    LightGrid::Ref prevB(a, b);
                    int prevIdx;

                    if(x > 0 && a == 0)
                    {
                        prevB.x = size -1;
                        prev.x--;
                    }
                    if(y > 0 && b == 0)
                    {
                        prevB.y = size -1;
                        prev.y--;
                    }

                    prevIdx = prevB.x + (prevB.y + lg.toIndex(prev) * size) * size;
                    if(center == 0)
                        prevIdx += lg.toIndex(prev) + 1;

                    ssamples[idx] = ssamples[prevIdx];
                }
                else
                {
                    // We haven't sampled this point yet.
                    ssamples[idx] = clusterAt(lg.origin() + off + samplePoints[sampleOffset]);
                }
            }
        }
    }

    // Allocate memory used for the collection of the sample results.
    QVector<SectorCluster *> blkSampleClusters(numSamples);

    for(int y = 0; y < lg.dimensions().y; ++y)
    for(int x = 0; x < lg.dimensions().x; ++x)
    {
        /**
         * Pick the sector cluster at each of the sample points.
         *
         * @todo We don't actually need the blkSampleClusters array anymore.
         * Now that ssamples stores the results consecutively a simple index
         * into ssamples would suffice. However if the optimization to save
         * memory is implemented as described in the comments above we WOULD
         * still require it.
         *
         * For now we'll make use of it to clarify the code.
         */
        int const sampleOffset = lg.toIndex(x, y) * numSamples;
        for(int i = 0; i < numSamples; ++i)
        {
            blkSampleClusters[i] = ssamples[i + sampleOffset];
        }

        SectorCluster *cluster = 0;
        if(numSamples == 1)
        {
            cluster = blkSampleClusters[center];
        }
        else
        {
            // Pick the sector which had the most hits.
            int best = -1;
            sampleHits.fill(0);

            for(int i = 0; i < numSamples; ++i)
            {
                if(!blkSampleClusters[i]) continue;

                for(int k = 0; k < numSamples; ++k)
                {
                    if(blkSampleClusters[k] == blkSampleClusters[i] && blkSampleClusters[k])
                    {
                        sampleHits[k]++;
                        if(sampleHits[k] > best)
                        {
                            best = i;
                        }
                    }
                }
            }

            if(best != -1)
            {
                // Favour the center sample if its a draw.
                if(sampleHits[best] == sampleHits[center] &&
                   blkSampleClusters[center])
                {
                    cluster = blkSampleClusters[center];
                }
                else
                {
                    cluster = blkSampleClusters[best];
                }
            }
        }

        if(cluster)
        {
            lg.setPrimarySource(lg.toIndex(x, y), cluster);
        }
    }

    LOGDEV_GL_MSG("%i light blocks (%u bytes)")
            << lg.numBlocks() << lg.blockStorageSize();

    // How much time did we spend?
    LOGDEV_GL_MSG("LightGrid init completed in %.2f seconds") << begunAt.since();
}

void Map::initBias()
{
    Time begunAt;

    LOG_AS("Map::initBias");

    // Start with no sources whatsoever.
    d->bias.sources.clear();

    if(d->def)
    {
        String const oldUniqueId = d->def->composeUniqueId(App_CurrentGame());

        // Load light sources from Light definitions.
        for(int i = 0; i < defs.lights.size(); ++i)
        {
            ded_light_t *lightDef = &defs.lights[i];

            if(lightDef->state[0]) continue;
            if(oldUniqueId.compareWithoutCase(lightDef->uniqueMapID)) continue;

            // Already at maximum capacity?
            if(biasSourceCount() == MAX_BIAS_SOURCES)
                break;

            addBiasSource(BiasSource::fromDef(*lightDef));
        }
    }

    LOGDEV_MAP_VERBOSE("Completed in %.2f seconds") << begunAt.since();
}

void Map::unlinkInMaterialLists(Surface *surface)
{
    if(!surface) return;
    if(d->decorator.isNull()) return;

    d->surfaceDecorator().remove(surface);
}

void Map::linkInMaterialLists(Surface *surface)
{
    if(!surface) return;

    // Only surfaces with a material will be linked.
    if(!surface->hasMaterial()) return;

    // Ignore surfaces not currently attributed to the map.
    if(&surface->map() != this)
    {
        qDebug() << "Ignoring alien surface" << de::dintptr(surface) << "in Map::unlinkInMaterialLists";
        return;
    }

    d->surfaceDecorator().add(surface);
}

void Map::buildMaterialLists()
{
    d->surfaceDecorator().reset();

    for(ConvexSubspace const *subspace : d->subspaces)
    {
        HEdge *base  = subspace->poly().hedge();
        HEdge *hedge = base;
        do
        {
            if(hedge->hasMapElement())
            {
                LineSide &side = hedge->mapElementAs<LineSideSegment>().lineSide();
                if(side.hasSections())
                {
                    linkInMaterialLists(&side.middle());
                    linkInMaterialLists(&side.top());
                    linkInMaterialLists(&side.bottom());
                }
                if(side.back().hasSections())
                {
                    linkInMaterialLists(&side.back().middle());
                    linkInMaterialLists(&side.back().top());
                    linkInMaterialLists(&side.back().bottom());
                }
            }
        } while((hedge = &hedge->next()) != base);

        subspace->forAllExtraMeshes([this] (Mesh &mesh)
        {
            for(HEdge *hedge : mesh.hedges())
            {
                // Is this on the back of a one-sided line?
                if(!hedge->hasMapElement()) continue;

                LineSide &side = hedge->mapElementAs<LineSideSegment>().lineSide();
                if(side.hasSections())
                {
                    linkInMaterialLists(&side.middle());
                    linkInMaterialLists(&side.top());
                    linkInMaterialLists(&side.bottom());
                }
                if(side.back().hasSections())
                {
                    linkInMaterialLists(&side.back().middle());
                    linkInMaterialLists(&side.back().top());
                    linkInMaterialLists(&side.back().bottom());
                }
            }
            return LoopContinue;
        });

        subspace->sector().forAllPlanes([this] (Plane &plane)
        {
            linkInMaterialLists(&plane.surface());
            return LoopContinue;
        });
    }
}

void Map::initContactBlockmaps()
{
    d->initContactBlockmaps();
}

void Map::spreadAllContacts(AABoxd const &region)
{
    // Expand the region according by the maxium radius of each contact type.
    d->mobjContactBlockmap->
        spread(AABoxd(region.minX - DDMOBJ_RADIUS_MAX, region.minY - DDMOBJ_RADIUS_MAX,
                      region.maxX + DDMOBJ_RADIUS_MAX, region.maxY + DDMOBJ_RADIUS_MAX));

    d->lumobjContactBlockmap->
        spread(AABoxd(region.minX - Lumobj::radiusMax(), region.minY - Lumobj::radiusMax(),
                      region.maxX + Lumobj::radiusMax(), region.maxY + Lumobj::radiusMax()));
}

void Map::initGenerators()
{
    LOG_AS("Map::initGenerators");
    Time begunAt;
    d->spawnTypeParticleGens();
    d->spawnMapParticleGens();
    LOGDEV_MAP_VERBOSE("Completed in %.2f seconds") << begunAt.since();
}

void Map::spawnPlaneParticleGens()
{
    //if(!useParticles) return;

    foreach(Sector *sector, d->sectors)
    {
        Plane &floor = sector->floor();
        floor.spawnParticleGen(Def_GetGenerator(floor.surface().composeMaterialUri()));

        Plane &ceiling = sector->ceiling();
        ceiling.spawnParticleGen(Def_GetGenerator(ceiling.surface().composeMaterialUri()));
    }
}

void Map::clearClMobjs()
{
    d->clMobjHash.clear();
}

mobj_t *Map::clMobjFor(thid_t id, bool canCreate) const
{
    LOG_AS("Map::clMobjFor");

    ClMobjHash::const_iterator found = d->clMobjHash.constFind(id);
    if(found != d->clMobjHash.constEnd())
    {
        return found.value();
    }

    if(!canCreate) return 0;

    // Create a new client mobj. This is a regular mobj that has network state
    // associated with it.

    MobjThinker mo(Thinker::AllocateMemoryZone);
    mo.id = id;
    mo.function = reinterpret_cast<thinkfunc_t>(gx.MobjThinker);

    ClientMobjThinkerData *data = new ClientMobjThinkerData;
    data->remoteSync().flags = DDMF_REMOTE;
    mo.setData(data);

    d->clMobjHash.insert(id, mo);
    data->audienceForDeletion() += d; // for removing from the hash

    d->thinkers->setMobjId(id); // Mark this ID as used.

    // Client mobjs are full-fludged game mobjs as well.
    d->thinkers->add(*(thinker_t *)mo);

    return mo.take();
}

int Map::clMobjIterator(int (*callback)(mobj_t *, void *), void *context)
{
    ClMobjHash::const_iterator next;
    for(ClMobjHash::const_iterator i = d->clMobjHash.constBegin();
        i != d->clMobjHash.constEnd(); i = next)
    {
        next = i;
        next++;

        DENG2_ASSERT(THINKER_DATA(i.value()->thinker, ClientMobjThinkerData).hasRemoteSync());

        // Callback returns zero to continue.
        if(int result = callback(i.value(), context))
            return result;
    }
    return 0;
}

Map::ClMobjHash const &Map::clMobjHash() const
{
    return d->clMobjHash;
}

#endif // __CLIENT__

AABoxd const &Map::bounds() const
{
    return d->bounds;
}

coord_t Map::gravity() const
{
    return _effectiveGravity;
}

void Map::setGravity(coord_t newGravity)
{
    if(!de::fequal(_effectiveGravity, newGravity))
    {
        _effectiveGravity = newGravity;
        LOG_MAP_VERBOSE("Effective gravity for %s now %.1f")
                << (d->def? d->def->gets("id") : "(unknown map)") << _effectiveGravity;
    }
}

Thinkers &Map::thinkers() const
{
    if(!d->thinkers.isNull())
    {
        return *d->thinkers;
    }
    /// @throw MissingThinkersError  The thinker lists are not yet initialized.
    throw MissingThinkersError("Map::thinkers", "Thinkers not initialized");
}

Sky &Map::sky() const
{
    return d->sky;
}

int Map::vertexCount() const
{
    return d->mesh.vertexCount();
}

Vertex &Map::vertex(int index) const
{
    if(Vertex *vtx = vertexPtr(index)) return *vtx;
    /// @throw MissingElementError  Invalid Vertex reference specified.
    throw MissingElementError("Map::vertex", "Unknown Vertex index:" + String::number(index));
}

Vertex *Map::vertexPtr(int index) const
{
    if(index >= 0 && index < d->mesh.vertexCount())
    {
        return d->mesh.vertexs().at(index);
    }
    return nullptr;
}

LoopResult Map::forAllVertexs(std::function<LoopResult (Vertex &)> func) const
{
    for(Vertex *vtx : d->mesh.vertexs())
    {
        if(auto result = func(*vtx)) return result;
    }
    return LoopContinue;
}

int Map::lineCount() const
{
    return d->lines.count();
}

Line &Map::line(int index) const
{
    if(Line *li = linePtr(index)) return *li;
    /// @throw MissingElementError  Invalid Line reference specified.
    throw MissingElementError("Map::line", "Unknown Line index:" + String::number(index));
}

Line *Map::linePtr(int index) const
{
    if(index >= 0 && index < d->lines.count())
    {
        return d->lines.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllLines(std::function<LoopResult (Line &)> func) const
{
    for(Line *li : d->lines)
    {
        if(auto result = func(*li)) return result;
    }
    return LoopContinue;
}

int Map::sectorCount() const
{
    return d->sectors.count();
}

Sector &Map::sector(int index) const
{
    if(Sector *sec = sectorPtr(index)) return *sec;
    /// @throw MissingElementError  Invalid Sector reference specified.
    throw MissingElementError("Map::sector", "Unknown Sector index:" + String::number(index));
}

Sector *Map::sectorPtr(int index) const
{
    if(index >= 0 && index < d->sectors.count())
    {
        return d->sectors.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllSectors(std::function<LoopResult (Sector &)> func) const
{
    for(Sector *sec : d->sectors)
    {
        if(auto result = func(*sec)) return result;
    }
    return LoopContinue;
}

int Map::subspaceCount() const
{
    return d->subspaces.count();
}

ConvexSubspace &Map::subspace(int index) const
{
    if(ConvexSubspace *sub = subspacePtr(index)) return *sub;
    /// @throw MissingElementError  Invalid ConvexSubspace reference specified.
    throw MissingElementError("Map::subspace", "Unknown subspace index:" + String::number(index));
}

ConvexSubspace *Map::subspacePtr(int index) const
{
    if(index >= 0 && index < d->subspaces.count())
    {
        return d->subspaces.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllSubspaces(std::function<LoopResult (ConvexSubspace &)> func) const
{
    for(ConvexSubspace *sub : d->subspaces)
    {
        if(auto result = func(*sub)) return result;
    }
    return LoopContinue;
}

int Map::clusterCount() const
{
    return d->clusters.count();
}

LoopResult Map::forAllClusters(Sector *sector, std::function<LoopResult (SectorCluster &)> func)
{
    if(sector)
    {
        for(auto it = d->clusters.constFind(sector); it != d->clusters.end() && it.key() == sector; ++it)
        {
            if(auto result = func(**it)) return result;
        }
        return LoopContinue;
    }

    for(SectorCluster *cluster : d->clusters)
    {
        if(auto result = func(*cluster)) return result;
    }
    return LoopContinue;
}

int Map::polyobjCount() const
{
    return d->polyobjs.count();
}

Polyobj &Map::polyobj(int index) const
{
    if(Polyobj *pob = polyobjPtr(index)) return *pob;
    /// @throw MissingObjectError  Invalid ConvexSubspace reference specified.
    throw MissingObjectError("Map::subspace", "Unknown Polyobj index:" + String::number(index));
}

Polyobj *Map::polyobjPtr(int index) const
{
    if(index >= 0 && index < d->polyobjs.count())
    {
        return d->polyobjs.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllPolyobjs(std::function<LoopResult (Polyobj &)> func) const
{
    for(Polyobj *pob : d->polyobjs)
    {
        if(auto result = func(*pob)) return result;
    }
    return LoopContinue;
}

void Map::initPolyobjs()
{
    LOG_AS("Map::initPolyobjs");

    for(Polyobj *po : d->polyobjs)
    {
        /// @todo Is this still necessary? -ds
        /// (This data is updated automatically when moving/rotating).
        po->updateAABox();
        po->updateSurfaceTangents();

        po->unlink();
        po->link();
    }
}

int Map::ambientLightLevel() const
{
    return _ambientLightLevel;
}

LineSide &Map::side(int index) const
{
    if(LineSide *side = sidePtr(index)) return *side;
    /// @throw MissingElementError  Invalid LineSide reference specified.
    throw MissingElementError("Map::side", "Unknown LineSide index:" + String::number(index));
}

LineSide *Map::sidePtr(int index) const
{
    if(index < 0) return nullptr;
    return &d->lines.at(index / 2)->side(index % 2);
}

int Map::toSideIndex(int lineIndex, int backSide) // static
{
    DENG_ASSERT(lineIndex >= 0);
    return lineIndex * 2 + (backSide? 1 : 0);
}

bool Map::identifySoundEmitter(SoundEmitter const &emitter, Sector **sector,
    Polyobj **poly, Plane **plane, Surface **surface) const
{
    *sector  = 0;
    *poly    = 0;
    *plane   = 0;
    *surface = 0;

    /// @todo Optimize: All sound emitters in a sector are linked together forming
    /// a chain. Make use of the chains instead.

    *poly = d->polyobjBySoundEmitter(emitter);
    if(!*poly)
    {
        // Not a polyobj. Try the sectors next.
        *sector = d->sectorBySoundEmitter(emitter);
        if(!*sector)
        {
            // Not a sector. Try the planes next.
            *plane = d->planeBySoundEmitter(emitter);
            if(!*plane)
            {
                // Not a plane. Try the surfaces next.
                *surface = d->surfaceBySoundEmitter(emitter);
            }
        }
    }

    return (*sector != 0 || *poly != 0|| *plane != 0|| *surface != 0);
}

EntityDatabase &Map::entityDatabase() const
{
    return d->entityDatabase;
}

void Map::initNodePiles()
{
    LOG_AS("Map");

    Time begunAt;

    // Initialize node piles and line rings.
    NP_Init(&d->mobjNodes, 256);  // Allocate a small pile.
    NP_Init(&d->lineNodes, lineCount() + 1000);

    // Allocate the rings.
    DENG_ASSERT(d->lineLinks == 0);
    d->lineLinks = (nodeindex_t *) Z_Malloc(sizeof(*d->lineLinks) * lineCount(), PU_MAPSTATIC, 0);

    for(int i = 0; i < lineCount(); ++i)
    {
        d->lineLinks[i] = NP_New(&d->lineNodes, NP_ROOT_NODE);
    }

    // How much time did we spend?
    LOGDEV_MAP_MSG("Initialized node piles in %.2f seconds") << begunAt.since();
}

Blockmap const &Map::mobjBlockmap() const
{
    if(!d->mobjBlockmap.isNull())
    {
        return *d->mobjBlockmap;
    }
    /// @throw MissingBlockmapError  The mobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::mobjBlockmap", "Mobj blockmap is not initialized");
}

Blockmap const &Map::polyobjBlockmap() const
{
    if(!d->polyobjBlockmap.isNull())
    {
        return *d->polyobjBlockmap;
    }
    /// @throw MissingBlockmapError  The polyobj blockmap is not yet initialized.
    throw MissingBlockmapError("Map::polyobjBlockmap", "Polyobj blockmap is not initialized");
}

LineBlockmap const &Map::lineBlockmap() const
{
    if(!d->lineBlockmap.isNull())
    {
        return *d->lineBlockmap;
    }
    /// @throw MissingBlockmapError  The line blockmap is not yet initialized.
    throw MissingBlockmapError("Map::lineBlockmap", "Line blockmap is not initialized");
}

Blockmap const &Map::subspaceBlockmap() const
{
    if(!d->subspaceBlockmap.isNull())
    {
        return *d->subspaceBlockmap;
    }
    /// @throw MissingBlockmapError  The subspace blockmap is not yet initialized.
    throw MissingBlockmapError("Map::subspaceBlockmap", "Convex subspace blockmap is not initialized");
}

LoopResult Map::forAllLinesTouchingMobj(mobj_t &mob, std::function<LoopResult (Line &)> func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if(&Mobj_Map(mob) == this && Mobj_IsLinked(mob) && mob.lineRoot)
    {
        QVarLengthArray<Line *, 16> linkStore;

        linknode_t *tn = d->mobjNodes.nodes;
        for(nodeindex_t nix = tn[mob.lineRoot].next; nix != mob.lineRoot; nix = tn[nix].next)
        {
            linkStore.append((Line *)(tn[nix].ptr));
        }

        for(int i = 0; i < linkStore.count(); ++i)
        {
            if(auto result = func(*linkStore[i]))
                return result;
        }
    }
    return LoopContinue;
}

LoopResult Map::forAllSectorsTouchingMobj(mobj_t &mob, std::function<LoopResult (Sector &)> func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if(&Mobj_Map(mob) == this && Mobj_IsLinked(mob))
    {
        QVarLengthArray<Sector *, 16> linkStore;

        // Always process the mobj's own sector first.
        Sector &ownSec = *Mobj_BspLeafAtOrigin(mob).sectorPtr();
        ownSec.setValidCount(validCount);
        linkStore.append(&ownSec);

        // Any good lines around here?
        if(mob.lineRoot)
        {
            linknode_t *tn = d->mobjNodes.nodes;
            for(nodeindex_t nix = tn[mob.lineRoot].next; nix != mob.lineRoot; nix = tn[nix].next)
            {
                Line *ld = (Line *)(tn[nix].ptr);

                // All these lines have sectors on both sides.
                // First, try the front.
                Sector &frontSec = ld->frontSector();
                if(frontSec.validCount() != validCount)
                {
                    frontSec.setValidCount(validCount);
                    linkStore.append(&frontSec);
                }

                // And then the back.
                /// @todo Above comment suggest always twosided, which is it? -ds
                if(ld->hasBackSector())
                {
                    Sector &backSec = ld->backSector();
                    if(backSec.validCount() != validCount)
                    {
                        backSec.setValidCount(validCount);
                        linkStore.append(&backSec);
                    }
                }
            }
        }

        for(int i = 0; i < linkStore.count(); ++i)
        {
            if(auto result = func(*linkStore[i]))
                return result;
        }
    }

    return LoopContinue;
}

LoopResult Map::forAllMobjsTouchingLine(Line &line, std::function<LoopResult (mobj_t &)> func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if(&line.map() == this)
    {
        QVarLengthArray<mobj_t *, 256> linkStore;

        // Collate mobjs touching the given line in case these relationships change.
        linknode_t *ln   = d->lineNodes.nodes;
        nodeindex_t root = d->lineLinks[line.indexInMap()];
        for(nodeindex_t nix = ln[root].next; nix != root; nix = ln[nix].next)
        {
            linkStore.append((mobj_t *)(ln[nix].ptr));
        }

        for(int i = 0; i < linkStore.count(); ++i)
        {
            if(auto result = func(*linkStore[i]))
                return result;
        }
    }
    return LoopContinue;
}

LoopResult Map::forAllMobjsTouchingSector(Sector &sector, std::function<LoopResult (mobj_t &)> func) const
{
    /// @todo Optimize: It should not be necessary to collate the objects first in
    /// in order to perform the iteration. This kind of "belt and braces" safety
    /// measure would not be necessary at this level if the caller(s) instead took
    /// responsibility for managing relationship changes during the iteration. -ds

    if(&sector.map() == this)
    {
        QVarLengthArray<mobj_t *, 256> linkStore;

        // Collate mobjs that obviously are in the sector.
        for(mobj_t *mob = sector.firstMobj(); mob; mob = mob->sNext)
        {
            if(mob->validCount != validCount)
            {
                mob->validCount = validCount;
                linkStore.append(mob);
            }
        }

        // Collate mobjs linked to the sector's lines.
        linknode_t const *ln = d->lineNodes.nodes;
        sector.forAllSides([this, &linkStore, &ln] (LineSide &side)
        {
            nodeindex_t root = d->lineLinks[side.line().indexInMap()];
            for(nodeindex_t nix = ln[root].next; nix != root; nix = ln[nix].next)
            {
                mobj_t *mob = (mobj_t *)(ln[nix].ptr);
                if(mob->validCount != validCount)
                {
                    mob->validCount = validCount;
                    linkStore.append(mob);
                }
            }
            return LoopContinue;
        });

        // Process all collected mobjs.
        for(int i = 0; i < linkStore.count(); ++i)
        {
            if(auto result = func(*linkStore[i]))
                return result;
        }
    }
    return LoopContinue;
}

int Map::unlink(mobj_t &mo)
{
    int links = 0;

    if(d->unlinkMobjFromSectors(mo))
        links |= MLF_SECTOR;

    BlockmapCell cell = d->mobjBlockmap->toCell(Mobj_Origin(mo));
    if(d->mobjBlockmap->unlink(cell, &mo))
        links |= MLF_BLOCKMAP;

    if(!d->unlinkMobjFromLines(mo))
        links |= MLF_NOLINE;

    return links;
}

void Map::link(mobj_t &mo, int flags)
{
    BspLeaf &bspLeafAtOrigin = bspLeafAt_FixedPrecision(Mobj_Origin(mo));

    // Link into the sector?
    if(flags & MLF_SECTOR)
    {
        d->unlinkMobjFromSectors(mo);
        bspLeafAtOrigin.sectorPtr()->link(&mo);
    }
    mo._bspLeaf = &bspLeafAtOrigin;

    // Link into blockmap?
    if(flags & MLF_BLOCKMAP)
    {
        BlockmapCell cell = d->mobjBlockmap->toCell(Mobj_Origin(mo));
        d->mobjBlockmap->link(cell, &mo);
    }

    // Link into lines?
    if(!(flags & MLF_NOLINE))
    {
        d->unlinkMobjFromLines(mo);
        d->linkMobjToLines(mo);
    }

    // If this is a player - perform additional tests to see if they have
    // entered or exited the void.
    if(mo.dPlayer && mo.dPlayer->mo)
    {
        mo.dPlayer->inVoid = true;

        if(SectorCluster *cluster = Mobj_ClusterPtr(mo))
        {
            if(Mobj_BspLeafAtOrigin(mo).subspace().contains(Mobj_Origin(mo)))
            {
#ifdef __CLIENT__
                if(mo.origin[VZ] <  cluster->visCeiling().heightSmoothed() + 4 &&
                   mo.origin[VZ] >= cluster->visFloor().heightSmoothed())
#else
                if(mo.origin[VZ] <  cluster->ceiling().height() + 4 &&
                   mo.origin[VZ] >= cluster->floor().height())
#endif
                {
                    mo.dPlayer->inVoid = false;
                }
            }
        }
    }
}

void Map::unlink(Polyobj &polyobj)
{
    d->polyobjBlockmap->unlink(polyobj.aaBox, &polyobj);
}

void Map::link(Polyobj &polyobj)
{
    d->polyobjBlockmap->link(polyobj.aaBox, &polyobj);
}

LoopResult Map::forAllLinesInBox(AABoxd const &box, int flags, std::function<LoopResult (Line &line)> func) const
{
    LoopResult result = LoopContinue;

    // Process polyobj lines?
    if((flags & LIF_POLYOBJ) && polyobjCount())
    {
        int const localValidCount = validCount;
        result = polyobjBlockmap().forAllInBox(box, [&func, &localValidCount] (void *object)
        {
            Polyobj &pob = *(Polyobj *)object;
            if(pob.validCount != localValidCount) // not yet processed
            {
                pob.validCount = localValidCount;
                for(Line *line : pob.lines())
                {
                    if(line->validCount() != localValidCount) // not yet processed
                    {
                        line->setValidCount(localValidCount);
                        if(auto result = func(*line))
                            return result;
                    }
                }
            }
            return LoopResult(); // continue
        });
    }

    // Process sector lines?
    if(!result && (flags & LIF_SECTOR))
    {
        int const localValidCount = validCount;
        result = lineBlockmap().forAllInBox(box, [&func, &localValidCount] (void *object)
        {
            Line &line = *(Line *)object;
            if(line.validCount() != localValidCount) // not yet processed
            {
                line.setValidCount(localValidCount);
                return func(line);
            }
            return LoopResult(); // continue
        });
    }

    return result;
}

BspLeaf &Map::bspLeafAt(Vector2d const &point) const
{
    if(!d->bsp.tree)
        /// @throw MissingBspTreeError  No BSP data is available.
        throw MissingBspTreeError("Map::bspLeafAt", "No BSP data available");

    BspTree const *bspTree = d->bsp.tree;
    while(!bspTree->isLeaf())
    {
        BspNode &bspNode = bspTree->userData()->as<BspNode>();

        int side = bspNode.partition().pointOnSide(point) < 0;

        // Descend to the child subspace on "this" side.
        bspTree = bspTree->childPtr(BspTree::ChildId(side));
    }

    // We've arrived at a leaf.
    return bspTree->userData()->as<BspLeaf>();
}

BspLeaf &Map::bspLeafAt_FixedPrecision(Vector2d const &point) const
{
    if(!d->bsp.tree)
        /// @throw MissingBspTreeError  No BSP data is available.
        throw MissingBspTreeError("Map::bspLeafAt_FixedPrecision", "No BSP data available");

    fixed_t pointX[2] = { DBL2FIX(point.x), DBL2FIX(point.y) };

    BspTree const *bspTree = d->bsp.tree;
    while(!bspTree->isLeaf())
    {
        BspNode &bspNode = bspTree->userData()->as<BspNode>();
        Partition const &partition = bspNode.partition();

        fixed_t lineOriginX[2]    = { DBL2FIX(partition.origin.x),    DBL2FIX(partition.origin.y) };
        fixed_t lineDirectionX[2] = { DBL2FIX(partition.direction.x), DBL2FIX(partition.direction.y) };
        int side = V2x_PointOnLineSide(pointX, lineOriginX, lineDirectionX);

        // Decend to the child subspace on "this" side.
        bspTree = bspTree->childPtr(BspTree::ChildId(side));
    }

    // We've arrived at a leaf.
    return bspTree->userData()->as<BspLeaf>();
}

SectorCluster *Map::clusterAt(Vector2d const &point) const
{
    BspLeaf &bspLeaf = bspLeafAt(point);
    if(bspLeaf.hasSubspace() && bspLeaf.subspace().contains(point))
    {
        return bspLeaf.subspace().clusterPtr();
    }
    return 0;
}

#ifdef __CLIENT__

void Map::updateScrollingSurfaces()
{
    foreach(Surface *surface, d->scrollingSurfaces)
    {
        surface->updateMaterialOriginTracking();
    }
}

Map::SurfaceSet &Map::scrollingSurfaces()
{
    return d->scrollingSurfaces;
}

void Map::updateTrackedPlanes()
{
    foreach(Plane *plane, d->trackedPlanes)
    {
        plane->updateHeightTracking();
    }
}

Map::PlaneSet &Map::trackedPlanes()
{
    return d->trackedPlanes;
}

void Map::initSkyFix()
{
    Time begunAt;

    LOG_AS("Map::initSkyFix");

    d->skyFloorHeight   = DDMAXFLOAT;
    d->skyCeilingHeight = DDMINFLOAT;

    // Update for sector plane heights and mobjs which intersect the ceiling.
    /// @todo Can't we defer this?
    for(Sector *sector : d->sectors)
    {
        if(!sector->sideCount()) continue;

        bool const skyFloor = sector->floorSurface().hasSkyMaskedMaterial();
        bool const skyCeil  = sector->ceilingSurface().hasSkyMaskedMaterial();

        if(!skyFloor && !skyCeil) continue;

        if(skyCeil)
        {
            // Adjust for the plane height.
            if(sector->ceiling().heightSmoothed() > d->skyCeilingHeight)
            {
                // Must raise the skyfix ceiling.
                d->skyCeilingHeight = sector->ceiling().heightSmoothed();
            }

            // Check that all the mobjs in the sector fit in.
            for(mobj_t *mo = sector->firstMobj(); mo; mo = mo->sNext)
            {
                coord_t extent = mo->origin[VZ] + mo->height;

                if(extent > d->skyCeilingHeight)
                {
                    // Must raise the skyfix ceiling.
                    d->skyCeilingHeight = extent;
                }
            }
        }

        if(skyFloor)
        {
            // Adjust for the plane height.
            if(sector->floor().heightSmoothed() < d->skyFloorHeight)
            {
                // Must lower the skyfix floor.
                d->skyFloorHeight = sector->floor().heightSmoothed();
            }
        }

        // Update for middle materials on lines which intersect the
        // floor and/or ceiling on the front (i.e., sector) side.
        sector->forAllSides([this, &skyCeil, &skyFloor] (LineSide &side)
        {
            if(!side.hasSections()) return LoopContinue;
            if(!side.middle().hasMaterial()) return LoopContinue;

            // There must be a sector on both sides.
            if(!side.hasSector() || !side.back().hasSector())
                return LoopContinue;

            // Possibility of degenerate BSP leaf.
            if(!side.leftHEdge()) return LoopContinue;

            WallEdge edge(WallSpec::fromMapSide(side, LineSide::Middle),
                          *side.leftHEdge(), Line::From);

            if(edge.isValid() && edge.top().z() > edge.bottom().z())
            {
                if(skyCeil && edge.top().z() + edge.materialOrigin().y > d->skyCeilingHeight)
                {
                    // Must raise the skyfix ceiling.
                    d->skyCeilingHeight = edge.top().z() + edge.materialOrigin().y;
                }

                if(skyFloor && edge.bottom().z() + edge.materialOrigin().y < d->skyFloorHeight)
                {
                    // Must lower the skyfix floor.
                    d->skyFloorHeight = edge.bottom().z() + edge.materialOrigin().y;
                }
            }
            return LoopContinue;
        });
    }

    LOGDEV_MAP_VERBOSE("Completed in %.2f seconds") << begunAt.since();
}

coord_t Map::skyFix(bool ceiling) const
{
    return ceiling? d->skyCeilingHeight : d->skyFloorHeight;
}

void Map::setSkyFix(bool ceiling, coord_t newHeight)
{
    if(ceiling) d->skyCeilingHeight = newHeight;
    else        d->skyFloorHeight   = newHeight;
}

Generator *Map::newGenerator()
{
    Generator::Id id = d->findIdForNewGenerator(); // 1-based
    if(!id) return 0; // Failed; too many generators?

    Instance::Generators &gens = d->getGenerators();

    // If there is already a generator with that id - remove it.
    if(id > 0 && id <= MAX_GENERATORS)
    {
        Generator_Delete(gens.activeGens[id - 1]);
    }

    /// @todo Linear allocation when in-game is not good...
    Generator *gen = (Generator *) Z_Calloc(sizeof(Generator), PU_MAP, 0);

    gen->setId(id);

    // Link the thinker to the list of (private) thinkers.
    gen->thinker.function = (thinkfunc_t) Generator_Thinker;
    d->thinkers->add(gen->thinker, false /*not public*/);

    // Link the generator into the collection.
    gens.activeGens[id - 1] = gen;

    return gen;
}

int Map::generatorCount() const
{
    if(d->generators.isNull()) return 0;
    int count = 0;
    Instance::Generators &gens = d->getGenerators();
    for(Generator::Id i = 0; i < MAX_GENERATORS; ++i)
    {
        if(gens.activeGens[i])
        {
            count += 1;
        }
    }
    return count;
}

void Map::unlink(Generator &generator)
{
    Instance::Generators &gens = d->getGenerators();
    for(Generator::Id i = 0; i < MAX_GENERATORS; ++i)
    {
        if(gens.activeGens[i] == &generator)
        {
            gens.activeGens[i] = 0;
            break;
        }
    }
}

int Map::generatorIterator(int (*callback) (Generator *, void *), void *context)
{
    Instance::Generators &gens = d->getGenerators();
    for(Generator::Id i = 0; i < MAX_GENERATORS; ++i)
    {
        // Only consider active generators.
        if(!gens.activeGens[i]) continue;

        if(int result = callback(gens.activeGens[i], context))
            return result;
    }
    return 0; // Continue iteration.
}

int Map::generatorListIterator(uint listIndex, int (*callback) (Generator *, void *),
    void *context)
{
    Instance::Generators &gens = d->getGenerators();
    for(Instance::Generators::ListNode *it = gens.lists[listIndex]; it; it = it->next)
    {
        if(int result = callback(it->gen, context))
            return result;
    }
    return 0; // Continue iteration.
}

int Map::lumobjCount() const
{
    return d->lumobjs.count();
}

Lumobj &Map::addLumobj(Lumobj const &lumobj)
{
    d->lumobjs.append(new Lumobj(lumobj));
    Lumobj &lum = *d->lumobjs.last();

    lum.setMap(this);
    lum.setIndexInMap(d->lumobjs.count() - 1);
    DENG2_ASSERT(lum.bspLeafAtOrigin().hasSubspace());
    lum.bspLeafAtOrigin().subspace().link(lum);
    R_AddContact(lum); // For spreading purposes.

    return lum;
}

void Map::removeLumobj(int which)
{
    if(which >= 0 && which < lumobjCount())
    {
        delete d->lumobjs.takeAt(which);
    }
}

void Map::removeAllLumobjs()
{
    for(ConvexSubspace *subspace : d->subspaces)
    {
        subspace->unlinkAllLumobjs();
    }
    qDeleteAll(d->lumobjs);
    d->lumobjs.clear();
}

Lumobj &Map::lumobj(int index) const
{
    if(Lumobj *lum = lumobjPtr(index)) return *lum;
    /// @throw MissingObjectError  Invalid Lumobj reference specified.
    throw MissingObjectError("Map::lumobj", "Unknown Lumobj index:" + String::number(index));
}

Lumobj *Map::lumobjPtr(int index) const
{
    if(index >= 0 && index < d->lumobjs.count())
    {
        return d->lumobjs.at(index);
    }
    return nullptr;
}

LoopResult Map::forAllLumobjs(std::function<LoopResult (Lumobj &)> func) const
{
    for(Lumobj *lob : d->lumobjs)
    {
        if(auto result = func(*lob)) return result;
    }
    return LoopContinue;
}

int Map::biasSourceCount() const
{
    return d->bias.sources.count();
}

BiasSource &Map::addBiasSource(BiasSource const &biasSource)
{
    if(biasSourceCount() < MAX_BIAS_SOURCES)
    {
        d->bias.sources.append(new BiasSource(biasSource));
        return *d->bias.sources.last();
    }
    /// @throw FullError  Attempt to add a new bias source when already at capcity.
    throw FullError("Map::addBiasSource", "Already at full capacity:" + String::number(MAX_BIAS_SOURCES));
}

void Map::removeBiasSource(int which)
{
    if(which >= 0 && which < biasSourceCount())
    {
        delete d->bias.sources.takeAt(which);
    }
}

void Map::removeAllBiasSources()
{
    qDeleteAll(d->bias.sources);
    d->bias.sources.clear();
}

BiasSource &Map::biasSource(int index) const
{
   if(BiasSource *bsrc = biasSourcePtr(index)) return *bsrc;
   /// @throw MissingObjectError  Invalid BiasSource reference specified.
   throw MissingObjectError("Map::biasSource", "Unknown BiasSource index:" + String::number(index));
}

BiasSource *Map::biasSourcePtr(int index) const
{
   if(index >= 0 && index < d->bias.sources.count())
   {
       return d->bias.sources.at(index);
   }
   return nullptr;
}

/**
 * @todo Implement a blockmap for these?
 * @todo Cache this result (MRU?).
 */
BiasSource *Map::biasSourceNear(Vector3d const &point) const
{
    BiasSource *nearest = 0;
    coord_t minDist = 0;
    for(BiasSource *src : d->bias.sources)
    {
        coord_t dist = (src->origin() - point).length();
        if(!nearest || dist < minDist)
        {
            minDist = dist;
            nearest = src;
        }
    }
    return nearest;
}

LoopResult Map::forAllBiasSources(std::function<LoopResult (BiasSource &)> func) const
{
    for(BiasSource *bsrc : d->bias.sources)
    {
        if(auto result = func(*bsrc)) return result;
    }
    return LoopContinue;
}

int Map::indexOf(BiasSource const &bsrc) const
{
    return d->bias.sources.indexOf(const_cast<BiasSource *>(&bsrc));
}

uint Map::biasCurrentTime() const
{
    return d->bias.currentTime;
}

uint Map::biasLastChangeOnFrame() const
{
    return d->bias.lastChangeOnFrame;
}

#endif // __CLIENT__

void Map::update()
{
#ifdef __CLIENT__
    d->updateParticleGens(); // Defs might've changed.

    // Update all surfaces.
    for(Sector *sector : d->sectors)
    {
        sector->forAllPlanes([] (Plane &plane)
        {
            plane.surface().markForDecorationUpdate();
            return LoopContinue;
        });
    }

    for(Line *line : d->lines)
    for(int i = 0; i < 2; ++i)
    {
        LineSide &side = line->side(i);
        if(!side.hasSections()) continue;

        side.top   ().markForDecorationUpdate();
        side.middle().markForDecorationUpdate();
        side.bottom().markForDecorationUpdate();
    }

    /// @todo Is this even necessary?
    for(Polyobj *polyobj : d->polyobjs)
    for(Line *line : polyobj->lines())
    {
        line->front().middle().markForDecorationUpdate();
    }

    // Rebuild the surface material lists.
    buildMaterialLists();

#endif // __CLIENT__

    // Reapply values defined in MapInfo (they may have changed).
    Record const &inf = mapInfo();

    _ambientLightLevel = inf.getf("ambient") * 255;
    _globalGravity     = inf.getf("gravity");
    _effectiveGravity  = _globalGravity;

#ifdef __CLIENT__
    // Reconfigure the sky.
    /// @todo Sky needs breaking up into multiple components. There should be
    /// a representation on server side and a logical entity which the renderer
    /// visualizes. We also need multiple concurrent skies for BOOM support.
    defn::Sky skyDef;
    if(Record const *def = defs.skies.tryFind("id", inf.gets("skyId")))
    {
        skyDef = *def;
    }
    else
    {
        skyDef = inf.subrecord("sky");
    }
    sky().configure(&skyDef);
#endif
}

#ifdef __CLIENT__
void Map::worldSystemFrameBegins(bool resetNextViewer)
{
    DENG2_ASSERT(&worldSys().map() == this); // Sanity check.

    // Interpolate the map ready for drawing view(s) of it.
    d->lerpTrackedPlanes(resetNextViewer);
    d->lerpScrollingSurfaces(resetNextViewer);

    if(!freezeRLs)
    {
        // Initialize and/or update the LightGrid.
        initLightGrid();

        d->biasBeginFrame();

        removeAllLumobjs();

        d->removeAllContacts();

        // Generate surface decorations for the frame.
        if(useLightDecorations)
        {
            // Perform scheduled redecoration.
            d->surfaceDecorator().redecorate();

            // Generate lumobjs for all decorations who want them.
            for(Line *line : d->lines)
            for(int i = 0; i < 2; ++i)
            {
                LineSide &side = line->side(i);
                if(!side.hasSections()) continue;

                d->generateLumobjs(side.middle());
                d->generateLumobjs(side.bottom());
                d->generateLumobjs(side.top());
            }

            for(Sector *sector : d->sectors)
            {
                sector->forAllPlanes([this] (Plane &plane)
                {
                    d->generateLumobjs(plane.surface());
                    return LoopContinue;
                });
            }
        }

        // Spawn omnilights for mobjs?
        if(useDynLights)
        {
            for(Sector *sector : d->sectors)
            for(mobj_t *iter = sector->firstMobj(); iter; iter = iter->sNext)
            {
                Mobj_GenerateLumobjs(iter);
            }
        }

        d->generateMobjContacts();

        d->linkAllParticles();
        d->linkAllContacts();
    }
}

/// @return  @c false= Continue iteration.
static int expireClMobjsWorker(mobj_t *mo, void *context)
{
    uint const nowTime = *static_cast<uint *>(context);

    // Already deleted?
    if(mo->thinker.function == (thinkfunc_t)-1)
        return 0;

    // Don't expire player mobjs.
    if(mo->dPlayer) return 0;

    ClientMobjThinkerData::RemoteSync *info = ClMobj_GetInfo(mo);
    DENG2_ASSERT(info != 0);

    if((info->flags & (CLMF_UNPREDICTABLE | CLMF_HIDDEN | CLMF_NULLED)) || !mo->info)
    {
        // Has this mobj timed out?
        if(nowTime - info->time > CLMOBJ_TIMEOUT)
        {
            LOGDEV_MAP_MSG("Mobj %i has expired (%i << %i), in state %s [%c%c%c]")
                    << mo->thinker.id
                    << info->time << nowTime
                    << Def_GetStateName(mo->state)
                    << (info->flags & CLMF_UNPREDICTABLE? 'U' : '_')
                    << (info->flags & CLMF_HIDDEN?        'H' : '_')
                    << (info->flags & CLMF_NULLED?        '0' : '_');

            // Too long. The server will probably never send anything
            // for this mobj, so get rid of it. (Both unpredictable
            // and hidden mobjs are not visible or bl/seclinked.)
            Mobj_Destroy(mo);
        }
    }

    return 0;
}

void Map::expireClMobjs()
{
    uint nowTime = Timer_RealMilliseconds();
    clMobjIterator(expireClMobjsWorker, &nowTime);
}

#endif // __CLIENT__

String Map::elementSummaryAsStyledText() const
{
#define TABBED(count, label) String(_E(Ta) "  %1 " _E(Tb) "%2\n").arg(count).arg(label)

    String str;
    QTextStream os(&str);

    if(lineCount())    os << TABBED(lineCount(),    "Lines");
    //if(sideCount())    os << TABBED(sideCount(),    "Sides");
    if(sectorCount())  os << TABBED(sectorCount(),  "Sectors");
    if(vertexCount())  os << TABBED(vertexCount(),  "Vertexes");
    if(polyobjCount()) os << TABBED(polyobjCount(), "Polyobjs");

    return str.rightStrip();

#undef TABBED
}

String Map::objectSummaryAsStyledText() const
{
#define TABBED(count, label) String(_E(Ta) "  %1 " _E(Tb) "%2\n").arg(count).arg(label)

    int thCountInStasis = 0;
    int thCount = thinkers().count(&thCountInStasis);

    String str;
    QTextStream os(&str);

    if(thCount)           os << TABBED(thCount,            String("Thinkers (%1 in stasis)").arg(thCountInStasis));
#ifdef __CLIENT__
    if(biasSourceCount()) os << TABBED(biasSourceCount(),  "Bias Sources");
    if(generatorCount())  os << TABBED(generatorCount(),   "Generators");
    if(lumobjCount())     os << TABBED(lumobjCount(),      "Lumobjs");
#endif

    return str.rightStrip();

#undef TABBED
}

D_CMD(InspectMap)
{
    DENG2_UNUSED3(src, argc, argv);

    LOG_AS("inspectmap (Cmd)");

    if(!App_WorldSystem().hasMap())
    {
        LOG_SCR_WARNING("No map is currently loaded");
        return false;
    }

    Map &map = App_WorldSystem().map();

    LOG_SCR_NOTE(_E(b) "%s - %s")
            << Con_GetString("map-name")
            << Con_GetString("map-author");
    LOG_SCR_MSG("\n");

    LOG_SCR_MSG(    _E(l) "Uri: "    _E(.) _E(i) "%s" _E(.)
              /*" " _E(l) " OldUid: " _E(.) _E(i) "%s" _E(.)*/
                    _E(l) " Music: "  _E(.) _E(i) "%i")
            << (map.def()? map.def()->composeUri().asText() : "(unknown map)")
            /*<< map.oldUniqueId()*/
            << Con_GetInteger("map-music");

    if(map.def() && map.def()->sourceFile()->hasCustom())
    {
        LOG_SCR_MSG(_E(l) "Source: " _E(.) _E(i) "\"%s\"")
                << NativePath(map.def()->sourceFile()->composePath()).pretty();
    }

    LOG_SCR_MSG("\n");

    if(map.isEditable())
    {
        LOG_MSG(_E(D) "Editing " _E(b) "Enabled");
    }

    LOG_SCR_MSG(_E(D) "Elements:");
    LOG_SCR_MSG("%s") << map.elementSummaryAsStyledText();

    if(map.thinkers().isInited())
    {
        LOG_SCR_MSG(_E(D) "Objects:");
        LOG_SCR_MSG("%s") << map.objectSummaryAsStyledText();
    }

    LOG_SCR_MSG(_E(R) "\n");

    Vector2d geometryDimensions = Vector2d(map.bounds().max) - Vector2d(map.bounds().min);
    LOG_SCR_MSG(_E(l) "Geometry dimensions: " _E(.) _E(i)) << geometryDimensions.asText();

    if(map.hasBspTree())
    {
        LOG_SCR_MSG(_E(l) "BSP: " _E(.) _E(i)) << map.bspTree().summary();
    }

    if(!map.subspaceBlockmap().isNull())
    {
        LOG_SCR_MSG(_E(l) "Subspace blockmap: " _E(.) _E(i)) << map.subspaceBlockmap().dimensions().asText();
    }
    if(!map.lineBlockmap().isNull())
    {
        LOG_SCR_MSG(_E(l) "Line blockmap: "     _E(.) _E(i)) << map.lineBlockmap().dimensions().asText();
    }
    if(!map.mobjBlockmap().isNull())
    {
        LOG_SCR_MSG(_E(l) "Mobj blockmap: "     _E(.) _E(i)) << map.mobjBlockmap().dimensions().asText();
    }
    if(!map.polyobjBlockmap().isNull())
    {
        LOG_SCR_MSG(_E(l) "Polyobj blockmap: "  _E(.) _E(i)) << map.polyobjBlockmap().dimensions().asText();
    }

#ifdef __CLIENT__
    if(map.hasLightGrid())
    {
        LOG_SCR_MSG(_E(l) "LightGrid: " _E(.) _E(i)) << map.lightGrid().dimensions().asText();
    }
#endif

    return true;

#undef TABBED
}

void Map::consoleRegister() // static
{
    Mobj_ConsoleRegister();

    C_VAR_INT("bsp-factor",                 &bspSplitFactor, CVF_NO_MAX, 0, 0);
#ifdef __CLIENT__
    C_VAR_INT("rend-bias-grid-multisample", &lgMXSample,     0, 0, 7);
#endif

    C_CMD("inspectmap", "", InspectMap);
}

/// Runtime map editing -----------------------------------------------------

/// Used when sorting vertex line owners.
static Vertex *rootVtx;

/**
 * Compares the angles of two lines that share a common vertex.
 *
 * pre: rootVtx must point to the vertex common between a and b
 *      which are (lineowner_t*) ptrs.
 */
static int lineAngleSorter(void const *a, void const *b)
{
    binangle_t angles[2];

    LineOwner *own[2] = { (LineOwner *)a, (LineOwner *)b };
    for(uint i = 0; i < 2; ++i)
    {
        if(own[i]->_link[Anticlockwise]) // We have a cached result.
        {
            angles[i] = own[i]->angle();
        }
        else
        {
            Line *line = &own[i]->line();
            Vertex const &otherVtx = line->vertex(&line->from() == rootVtx? 1:0);

            fixed_t dx = otherVtx.origin().x - rootVtx->origin().x;
            fixed_t dy = otherVtx.origin().y - rootVtx->origin().y;

            own[i]->_angle = angles[i] = bamsAtan2(-100 *dx, 100 * dy);

            // Mark as having a cached angle.
            own[i]->_link[Anticlockwise] = (LineOwner *) 1;
        }
    }

    return (angles[1] - angles[0]);
}

/**
 * Merge left and right line owner lists into a new list.
 *
 * @return  The newly merged list.
 */
static LineOwner *mergeLineOwners(LineOwner *left, LineOwner *right,
    int (*compare) (void const *a, void const *b))
{
    LineOwner tmp;
    LineOwner *np = &tmp;

    tmp._link[Clockwise] = np;
    while(left && right)
    {
        if(compare(left, right) <= 0)
        {
            np->_link[Clockwise] = left;
            np = left;

            left = &left->next();
        }
        else
        {
            np->_link[Clockwise] = right;
            np = right;

            right = &right->next();
        }
    }

    // At least one of these lists is now empty.
    if(left)
    {
        np->_link[Clockwise] = left;
    }
    if(right)
    {
        np->_link[Clockwise] = right;
    }

    // Is the list empty?
    if(!tmp.hasNext())
        return NULL;

    return &tmp.next();
}

static LineOwner *splitLineOwners(LineOwner *list)
{
    if(!list) return NULL;

    LineOwner *lista = list;
    LineOwner *listb = list;
    LineOwner *listc = list;

    do
    {
        listc = listb;
        listb = &listb->next();
        lista = &lista->next();
        if(lista)
        {
            lista = &lista->next();
        }
    } while(lista);

    listc->_link[Clockwise] = NULL;
    return listb;
}

/**
 * This routine uses a recursive mergesort algorithm; O(NlogN)
 */
static LineOwner *sortLineOwners(LineOwner *list,
    int (*compare) (void const *a, void const *b))
{
    if(list && list->_link[Clockwise])
    {
        LineOwner *p = splitLineOwners(list);

        // Sort both halves and merge them back.
        list = mergeLineOwners(sortLineOwners(list, compare),
                               sortLineOwners(p, compare), compare);
    }
    return list;
}

static void setVertexLineOwner(Vertex *vtx, Line *lineptr, LineOwner **storage)
{
    if(!lineptr) return;

    // Has this line already been registered with this vertex?
    LineOwner const *own = vtx->firstLineOwner();
    while(own)
    {
        if(&own->line() == lineptr)
            return; // Yes, we can exit.

        own = &own->next();
    }

    // Add a new owner.
    vtx->_numLineOwners++;
    LineOwner *newOwner = (*storage)++;

    newOwner->_line = lineptr;
    newOwner->_link[Anticlockwise] = NULL;

    // Link it in.
    // NOTE: We don't bother linking everything at this stage since we'll
    // be sorting the lists anyway. After which we'll finish the job by
    // setting the prev and circular links.
    // So, for now this is only linked singlely, forward.
    newOwner->_link[Clockwise] = vtx->_lineOwners;
    vtx->_lineOwners = newOwner;

    // Link the line to its respective owner node.
    if(vtx == &lineptr->from())
        lineptr->_vo1 = newOwner;
    else
        lineptr->_vo2 = newOwner;
}

#ifdef DENG2_DEBUG
/**
 * Determines whether the specified vertex @a v has a correctly formed line
 * owner ring.
 */
static bool vertexHasValidLineOwnerRing(Vertex &v)
{
    LineOwner const *base = v.firstLineOwner();
    LineOwner const *cur = base;
    do
    {
        if(&cur->prev().next() != cur) return false;
        if(&cur->next().prev() != cur) return false;

    } while((cur = &cur->next()) != base);
    return true;
}
#endif

/**
 * Generates the line owner rings for each vertex. Each ring includes all
 * the lines which the vertex belongs to sorted by angle, (the rings are
 * arranged in clockwise order, east = 0).
 */
void buildVertexLineOwnerRings(QList<Vertex *> const &vertexs, QList<Line *> &editableLines)
{
    LOG_AS("buildVertexLineOwnerRings");

    /*
     * Step 1: Find and link up all line owners.
     */
    // We know how many vertex line owners we need (numLines * 2).
    LineOwner *lineOwners = (LineOwner *) Z_Malloc(sizeof(LineOwner) * editableLines.count() * 2, PU_MAPSTATIC, 0);
    LineOwner *allocator = lineOwners;

    for(Line *line : editableLines)
    for(int p = 0; p < 2; ++p)
    {
        setVertexLineOwner(&line->vertex(p), line, &allocator);
    }

    /*
     * Step 2: Sort line owners of each vertex and finalize the rings.
     */
    for(Vertex *v : vertexs)
    {
        if(!v->_numLineOwners) continue;

        // Sort them; ordered clockwise by angle.
        rootVtx = v;
        v->_lineOwners = sortLineOwners(v->_lineOwners, lineAngleSorter);

        // Finish the linking job and convert to relative angles.
        // They are only singly linked atm, we need them to be doubly
        // and circularly linked.
        binangle_t firstAngle = v->_lineOwners->angle();
        LineOwner *last = v->_lineOwners;
        LineOwner *p = &last->next();
        while(p)
        {
            p->_link[Anticlockwise] = last;

            // Convert to a relative angle between last and this.
            last->_angle = last->angle() - p->angle();

            last = p;
            p = &p->next();
        }
        last->_link[Clockwise] = v->_lineOwners;
        v->_lineOwners->_link[Anticlockwise] = last;

        // Set the angle of the last owner.
        last->_angle = last->angle() - firstAngle;

/*#ifdef DENG2_DEBUG
        LOG_MAP_VERBOSE("Vertex #%i: line owners #%i")
            << editmap.vertexes.indexOf(v) << v->lineOwnerCount();

        LineOwner const *base = v->firstLineOwner();
        LineOwner const *cur = base;
        uint idx = 0;
        do
        {
            LOG_MAP_VERBOSE("  %i: p= #%05i this= #%05i n= #%05i, dANG= %-3.f")
                << idx << cur->prev().line().indexInMap() << cur->line().indexInMap()
                << cur->next().line().indexInMap() << BANG2DEG(cur->angle());

            idx++;
        } while((cur = &cur->next()) != base);
#endif*/

        // Sanity check.
        DENG2_ASSERT(vertexHasValidLineOwnerRing(*v));
    }
}

bool Map::isEditable() const
{
    return d->editingEnabled;
}

struct VertexInfo
{
    Vertex *vertex = nullptr;  ///< Vertex for this info.
    Vertex *equiv = nullptr;   ///< Determined equivalent vertex.
    uint refCount = 0;         ///< Line -> Vertex reference count.

    /// @todo Math here is not correct (rounding directionality). -ds
    int compareVertexOrigins(VertexInfo const &other) const
    {
        DENG2_ASSERT(vertex && other.vertex);

        if(this == &other) return 0;
        if(vertex == other.vertex) return 0;

        // Order is firstly X axis major.
        if(int(vertex->origin().x) != int(other.vertex->origin().x))
        {
            return int(vertex->origin().x) - int(other.vertex->origin().x);
        }

        // Order is secondly Y axis major.
        return int(vertex->origin().y) - int(other.vertex->origin().y);
    }

    bool operator < (VertexInfo const &other) const
    {
        return compareVertexOrigins(other) < 0;
    }
};

void pruneVertexes(Mesh &mesh, Map::Lines const &lines)
{
    /*
     * Step 1 - Find equivalent vertexes:
     */

    // Populate the vertex info.
    QVector<VertexInfo> vertexInfo(mesh.vertexCount());
    int ord = 0;
    for(Vertex *vertex : mesh.vertexs())
    {
        vertexInfo[ord++].vertex = vertex;
    }

    {
        // Sort a copy to place near vertexes adjacently.
        QVector<VertexInfo> sortedInfo(vertexInfo);
        qSort(sortedInfo.begin(), sortedInfo.end());

        // Locate equivalent vertexes in the sorted info.
        for(int i = 0; i < sortedInfo.count() - 1; ++i)
        {
            VertexInfo &a = sortedInfo[i];
            VertexInfo &b = sortedInfo[i + 1];

            // Are these equivalent?
            /// @todo fixme: What about polyobjs? They need unique vertexes! -ds
            if(a.compareVertexOrigins(b) == 0)
            {
                b.equiv = (a.equiv? a.equiv : a.vertex);
            }
        }
    }

    /*
     * Step 2 - Replace line references to equivalent vertexes:
     */

    // Count line -> vertex references.
    for(Line *line : lines)
    {
        vertexInfo[line->from().indexInMap()].refCount++;
        vertexInfo[  line->to().indexInMap()].refCount++;
    }

    // Perform the replacement.
    for(Line *line : lines)
    {
        while(vertexInfo[line->from().indexInMap()].equiv)
        {
            VertexInfo &info = vertexInfo[line->from().indexInMap()];

            info.refCount--;
            line->replaceFrom(*info.equiv);

            vertexInfo[line->from().indexInMap()].refCount++;
        }

        while(vertexInfo[line->to().indexInMap()].equiv)
        {
            VertexInfo &info = vertexInfo[line->to().indexInMap()];

            info.refCount--;
            line->replaceTo(*info.equiv);

            vertexInfo[line->to().indexInMap()].refCount++;
        }
    }

    /*
     * Step 3 - Prune vertexes:
     */
    int prunedCount = 0, numUnused = 0;
    for(VertexInfo const &info : vertexInfo)
    {
        Vertex *vertex = info.vertex;

        if(info.refCount) continue;

        mesh.removeVertex(*vertex);

        prunedCount += 1;
        if(!info.equiv) numUnused += 1;
    }

    if(prunedCount)
    {
        // Re-index with a contiguous range of indices.
        int ord = 0;
        for(Vertex *vertex : mesh.vertexs())
        {
            vertex->setIndexInMap(ord++);
        }

        /// Update lines. @todo Line should handle this itself.
        for(Line *line : lines)
        {
            line->updateSlopeType();
            line->updateAABox();
        }

        LOGDEV_MAP_NOTE("Pruned %d vertexes (%d equivalents, %d unused)")
                << prunedCount << (prunedCount - numUnused) << numUnused;
    }
}

bool Map::endEditing()
{
    if(!d->editingEnabled) return true; // Huh?

    d->editingEnabled = false;

    LOG_AS("Map");
    LOG_MAP_VERBOSE("Editing ended");
    LOGDEV_MAP_VERBOSE("New elements: %d Vertexes, %d Lines, %d Polyobjs and %d Sectors")
            << d->mesh.vertexCount()        << d->editable.lines.count()
            << d->editable.polyobjs.count() << d->editable.sectors.count();

    /*
     * Perform cleanup on the new map elements.
     */
    pruneVertexes(d->mesh, d->editable.lines);

    // Ensure lines with only one sector are flagged as blocking.
    for(Line *line : d->editable.lines)
    {
        if(!line->hasFrontSector() || !line->hasBackSector())
            line->setFlags(DDLF_BLOCKING);
    }

    buildVertexLineOwnerRings(d->mesh.vertexs(), d->editable.lines);

    /*
     * Move the editable elements to the "static" element lists.
     */

    // Collate sectors:
    DENG2_ASSERT(d->sectors.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    d->sectors.reserve(d->editable.sectors.count());
#endif
    d->sectors.append(d->editable.sectors);
    d->editable.sectors.clear();

    // Collate lines:
    DENG2_ASSERT(d->lines.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    d->lines.reserve(d->editable.lines.count());
#endif
    d->lines.append(d->editable.lines);
    d->editable.lines.clear();

    // Collate polyobjs:
    DENG2_ASSERT(d->polyobjs.isEmpty());
#ifdef DENG2_QT_4_7_OR_NEWER
    d->polyobjs.reserve(d->editable.polyobjs.count());
#endif
    while(!d->editable.polyobjs.isEmpty())
    {
        d->polyobjs.append(d->editable.polyobjs.takeFirst());
        Polyobj *polyobj = d->polyobjs.back();

        // Create half-edge geometry and line segments for each line.
        for(Line *line : polyobj->lines())
        {
            HEdge *hedge = polyobj->mesh().newHEdge(line->from());

            hedge->setTwin(polyobj->mesh().newHEdge(line->to()));
            hedge->twin().setTwin(hedge);

            LineSideSegment *seg = line->front().addSegment(*hedge);
#ifdef __CLIENT__
            seg->setLength(line->length());
#else
            DENG_UNUSED(seg);
#endif
        }

        polyobj->buildUniqueVertexes();
        polyobj->updateOriginalVertexCoords();
    }

    // Determine the map bounds.
    d->updateBounds();
    LOG_MAP_VERBOSE("Geometry bounds:") << Rectangled(d->bounds.min, d->bounds.max).asText();

    // Build a line blockmap.
    d->initLineBlockmap();

    // Build a new BspTree.
    if(!d->buildBspTree())
        return false;

    // The mobj and polyobj blockmaps are maintained dynamically.
    d->initMobjBlockmap();
    d->initPolyobjBlockmap();

    // Finish lines.
    for(Line *line : d->lines)
    for(int i = 0; i < 2; ++i)
    {
        line->side(i).updateSurfaceNormals();
        line->side(i).updateAllSoundEmitterOrigins();
    }

    // Finish sectors.
    for(Sector *sector : d->sectors)
    {
        d->buildClusters(*sector);
        sector->buildSides();
        sector->chainSoundEmitters();
    }

    // Finish planes.
    for(Sector *sector : d->sectors)
    {
        sector->forAllPlanes([] (Plane &plane)
        {
            plane.updateSoundEmitterOrigin();
            return LoopContinue;
        });
    }

    // We can now initialize the convex subspace blockmap.
    d->initSubspaceBlockmap();

    // Prepare the thinker lists.
    d->thinkers.reset(new Thinkers);

    return true;
}

Vertex *Map::createVertex(Vector2d const &origin, int archiveIndex)
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createVertex", "Editing is not enabled");

    Vertex *vtx = d->mesh.newVertex(origin);

    vtx->setMap(this);
    vtx->setIndexInArchive(archiveIndex);

    /// @todo Don't do this here.
    vtx->setIndexInMap(d->mesh.vertexCount() - 1);

    return vtx;
}

Line *Map::createLine(Vertex &v1, Vertex &v2, int flags, Sector *frontSector,
    Sector *backSector, int archiveIndex)
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createLine", "Editing is not enabled");

    Line *line = new Line(v1, v2, flags, frontSector, backSector);
    d->editable.lines.append(line);

    line->setMap(this);
    line->setIndexInArchive(archiveIndex);

    /// @todo Don't do this here.
    line->setIndexInMap(d->editable.lines.count() - 1);
    line->front().setIndexInMap(Map::toSideIndex(line->indexInMap(), Line::Front));
    line->back().setIndexInMap(Map::toSideIndex(line->indexInMap(), Line::Back));

    return line;
}

Sector *Map::createSector(float lightLevel, Vector3f const &lightColor,
    int archiveIndex)
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createSector", "Editing is not enabled");

    Sector *sector = new Sector(lightLevel, lightColor);
    d->editable.sectors.append(sector);

    sector->setMap(this);
    sector->setIndexInArchive(archiveIndex);

    /// @todo Don't do this here.
    sector->setIndexInMap(d->editable.sectors.count() - 1);

    return sector;
}

Polyobj *Map::createPolyobj(Vector2d const &origin)
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::createPolyobj", "Editing is not enabled");

    void *region = M_Calloc(POLYOBJ_SIZE);
    Polyobj *po = new (region) Polyobj(origin);
    d->editable.polyobjs.append(po);

    /// @todo Don't do this here.
    po->setIndexInMap(d->editable.polyobjs.count() - 1);

    return po;
}

Map::Lines const &Map::editableLines() const
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editableLines", "Editing is not enabled");
    return d->editable.lines;
}

Map::Sectors const &Map::editableSectors() const
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editableSectors", "Editing is not enabled");
    return d->editable.sectors;
}

Map::Polyobjs const &Map::editablePolyobjs() const
{
    if(!d->editingEnabled)
        /// @throw EditError  Attempted when not editing.
        throw EditError("Map::editablePolyobjs", "Editing is not enabled");
    return d->editable.polyobjs;
}

void EditableElements::clearAll()
{
    qDeleteAll(lines);
    lines.clear();

    qDeleteAll(sectors);
    sectors.clear();

    foreach(Polyobj *po, polyobjs)
    {
        po->~Polyobj();
        M_Free(po);
    }
    polyobjs.clear();
}

} // namespace de
