/** @file map.h  World map.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2015 Daniel Swanson <danij@dengine.net>
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

#ifndef DENG_WORLD_MAP_H
#define DENG_WORLD_MAP_H

#include <functional>
#include <QList>
#include <QHash>
#include <QSet>
#include <doomsday/uri.h>
#include <de/BinaryTree>
#include <de/Observers>
#include <de/Vector>

#include "Mesh"

#include "BspNode"
#include "Line"
#include "Polyobj"

#ifdef __CLIENT__
#  include "world/p_object.h"
#  include "client/clplanemover.h"
#  include "client/clpolymover.h"

#  include "world/worldsystem.h"
#  include "Generator"

#  include "BiasSource"
#  include "Lumobj"
#endif

class MapDef;

class BspLeaf;
class ConvexSubspace;
class LineBlockmap;
class Plane;
class Sector;
class SectorCluster;
class Sky;
class Surface;
class Vertex;

#ifdef __CLIENT__
class BiasTracker;
#endif

namespace de {

class Blockmap;
class EntityDatabase;
#ifdef __CLIENT__
class LightGrid;
#endif
class Thinkers;

/**
 * World map.
 *
 * @ingroup world
 */
class Map
#ifdef __CLIENT__
: DENG2_OBSERVES(WorldSystem, FrameBegin)
#endif
{
    DENG2_NO_COPY  (Map)
    DENG2_NO_ASSIGN(Map)

public:
    /// Base error for runtime map editing errors. @ingroup errors
    DENG2_ERROR(EditError);

    /// Required map element is missing. @ingroup errors
    DENG2_ERROR(MissingElementError);

    /// Required map object is missing. @ingroup errors
    DENG2_ERROR(MissingObjectError);

    /// Required blockmap is missing. @ingroup errors
    DENG2_ERROR(MissingBlockmapError);

    /// Required BSP data is missing. @ingroup errors
    DENG2_ERROR(MissingBspTreeError);

    /// Required thinker lists are missing. @ingroup errors
    DENG2_ERROR(MissingThinkersError);

#ifdef __CLIENT__
    /// Required light grid is missing. @ingroup errors
    DENG2_ERROR(MissingLightGridError);

    /// Attempted to add a new element/object when full. @ingroup errors
    DENG2_ERROR(FullError);
#endif

    /// Notified when the map is about to be deleted.
    DENG2_DEFINE_AUDIENCE(Deletion, void mapBeingDeleted(Map const &map))

    /// Notified when a one-way window construct is first found.
    DENG2_DEFINE_AUDIENCE(OneWayWindowFound, void oneWayWindowFound(Line &line, Sector &backFacingSector))

    /// Notified when an unclosed sector is first found.
    DENG2_DEFINE_AUDIENCE(UnclosedSectorFound, void unclosedSectorFound(Sector &sector, Vector2d const &nearPoint))

    /*
     * Constants:
     */
#ifdef __CLIENT__
    static dint const MAX_BIAS_SOURCES = 8 * 32;  // Hard limit due to change tracking.

    /// Maximum number of generators per map.
    static dint const MAX_GENERATORS = 512;
#endif

    typedef de::BinaryTree<BspElement *> BspTree;

#ifdef __CLIENT__
    typedef QSet<Plane *> PlaneSet;
    typedef QSet<Surface *> SurfaceSet;
    typedef QHash<thid_t, struct mobj_s *> ClMobjHash;
#endif

public: /// @todo make private:
    coord_t _globalGravity    = 0;  ///< The defined gravity for this map.
    coord_t _effectiveGravity = 0;  ///< The effective gravity for this map.
    dint _ambientLightLevel   = 0;  ///< Ambient lightlevel for the current map.

public:
    /**
     * Construct a new map initially configured in an editable state. Whilst
     * editable new map elements can be added, thereby allowing the map to be
     * constructed dynamically. When done editing @ref endEditing() should be
     * called to switch the map into a non-editable (i.e., playable) state.
     *
     * @param mapDefinition  Definition for the map (Can be set later, @ref setDef).
     */
    explicit Map(MapDef *mapDefinition = nullptr);

    /**
     * Returns the definition for the map.
     */
    MapDef *def() const;

    /**
     * Change the definition associated with the map to @a newMapDefinition.
     */
    void setDef(MapDef *newMapDefinition);

    /**
     * Returns the effective map-info definition Record for the map.
     *
     * @see WorldSystem::mapInfoForMapUri()
     */
    de::Record const &mapInfo() const;

    /**
     * Returns the points which describe the boundary of the map coordinate
     * space, which, are defined by the minimal and maximal vertex coordinates
     * of the non-editable, non-polyobj line geometries).
     */
    AABoxd const &bounds() const;

    inline Vector2d origin() const {
        return Vector2d(bounds().min);
    }

    inline Vector2d dimensions() const {
        return Vector2d(bounds().max) - Vector2d(bounds().min);
    }

    /**
     * Returns the minimum ambient light level for the whole map.
     */
    dint ambientLightLevel() const;

    /**
     * Returns the currently effective gravity multiplier for the map.
     */
    coord_t gravity() const;

    /**
     * Change the effective gravity multiplier for the map.
     *
     * @param newGravity  New gravity multiplier.
     */
    void setGravity(coord_t newGravity);

    /**
     * To be called following an engine reset to update the map state.
     */
    void update();

#ifdef __CLIENT__
public:  // Light sources ---------------------------------------------------------

    /**
     * Returns the total number of BiasSources in the map.
     */
    dint biasSourceCount() const;

    /**
     * Attempt to add a new bias light source to the map (a copy is made).
     *
     * @note At most @ref MAX_BIAS_SOURCES are supported for technical reasons.
     *
     * @return  Reference to the newly added bias source.
     *
     * @see biasSourceCount()
     * @throws FullError  Once capacity is reached.
     */
    BiasSource &addBiasSource(BiasSource const &biasSource = BiasSource());

    /**
     * Removes the specified bias light source from the map.
     *
     * @see removeAllBiasSources()
     */
    void removeBiasSource(dint which);

    /**
     * Remove all bias sources from the map.
     *
     * @see removeBiasSource()
     */
    void removeAllBiasSources();

    /**
     * Lookup a BiasSource by it's unique @a index.
     */
    BiasSource &biasSource(dint index) const;
    BiasSource *biasSourcePtr(dint index) const;

    /**
     * Finds the bias source nearest to the specified map space @a point.
     *
     * @note This result is not cached. May return @c 0 if no bias sources exist.
     */
    BiasSource *biasSourceNear(Vector3d const &point) const;

    /**
     * Iterate through the BiasSources of the map.
     *
     * @param func  Callback to make for each BiasSource.
     */
    LoopResult forAllBiasSources(std::function<LoopResult (BiasSource &)> func) const;

    /**
     * Lookup the unique index for the given bias @a source.
     */
    dint indexOf(BiasSource const &source) const;

    /**
     * Returns the time in milliseconds when the current render frame began. Used
     * for interpolation purposes.
     */
    duint biasCurrentTime() const;

    /**
     * Returns the frameCount of the current render frame. Used for tracking changes
     * to bias sources/surfaces.
     */
    duint biasLastChangeOnFrame() const;

    // Luminous-objects -----------------------------------------------------------

    /**
     * Returns the total number of lumobjs in the map.
     */
    dint lumobjCount() const;

    /**
     * Add a new lumobj to the map (a copy is made).
     *
     * @return  Reference to the newly added lumobj.
     */
    Lumobj &addLumobj(Lumobj const &lumobj = Lumobj());

    /**
     * Removes the specified lumobj from the map.
     *
     * @see removeAllLumobjs()
     */
    void removeLumobj(dint which);

    /**
     * Remove all lumobjs from the map.
     *
     * @see removeLumobj()
     */
    void removeAllLumobjs();

    /**
     * Lookup a Lumobj in the map by it's unique @a index.
     */
    Lumobj &lumobj(dint index) const;
    Lumobj *lumobjPtr(dint index) const;

    /**
     * Iterate through the Lumpobjs of the map.
     *
     * @param func  Callback to make for each Lumobj.
     */
    LoopResult forAllLumobjs(std::function<LoopResult (Lumobj &)> func) const;
#endif // __CLIENT__

public:  // Lines & Line-Sides ----------------------------------------------------

    /**
     * Returns the total number of Lines in the map.
     */
    dint lineCount() const;

    /**
     * Lookup a Line in the map by it's unique @a index.
     */
    Line &line(dint index) const;
    Line *linePtr(dint index) const;

    /**
     * Iterate through the Lines of the map.
     *
     * @param func  Callback to make for each Line.
     */
    LoopResult forAllLines(std::function<LoopResult (Line &)> func) const;

    /**
     * Lines and Polyobj lines (note polyobj lines are iterated first).
     *
     * @note validCount should be incremented before calling this to begin a new
     * logical traversal. Otherwise Lines marked with a validCount equal to this will
     * be skipped over (can be used to avoid processing a line multiple times during
     * complex / non-linear traversals.
     *
     * @param box    Axis-aligned bounding box in which Lines must be Blockmap-linked.
     * @param flags  @ref lineIteratorFlags
     * @param func   Callback to make for each Line.
     */
    LoopResult forAllLinesInBox(AABoxd const &box, dint flags, std::function<LoopResult (Line &)> func) const;

    /**
     * @overload
     */
    inline LoopResult forAllLinesInBox(AABoxd const &box, std::function<LoopResult (Line &)> func) const
    {
        return forAllLinesInBox(box, LIF_ALL, func);
    }

    /**
     * The callback function will be called once for each line that crosses the object.
     * This means all the lines will be two-sided.
     */
    LoopResult forAllLinesTouchingMobj(struct mobj_s &mob, std::function<LoopResult (Line &)> func) const;

    // ---

    /**
     * Returns the total number of Line::Sides in the map.
     */
    inline dint sideCount() const { return lineCount() * 2; }

    /**
     * Lookup a LineSide in the map by it's unique @a index.
     *
     * @see toSideIndex()
     */
    LineSide &side(dint index) const;
    LineSide *sidePtr(dint index) const;

    /**
     * Helper function which returns the relevant side index given a @a lineIndex
     * and @a side identifier.
     *
     * Indices are produced as follows:
     * @code
     *  lineIndex / 2 + (backSide? 1 : 0);
     * @endcode
     *
     * @param lineIndex  Index of the line in the map.
     * @param side       Side of the line. @c =0 the Line::Front else Line::Back
     *
     * @return  Unique index for the identified side.
     */
    static dint toSideIndex(dint lineIndex, dint side);

public:  // Map-objects -----------------------------------------------------------

    LoopResult forAllMobjsTouchingLine(Line &line, std::function<LoopResult (struct mobj_s &)> func) const;

    /**
     * Increment validCount before using this. 'func' is called for each mobj
     * that is (even partly) inside the sector. This is not a 3D test, the
     * mobjs may actually be above or under the sector.
     *
     * (Lovely name; actually this is a combination of SectorMobjs and
     * a bunch of LineMobjs iterations.)
     */
    LoopResult forAllMobjsTouchingSector(Sector &sector, std::function<LoopResult (struct mobj_s &)> func) const;

    /**
     * Links a mobj into both a block and a BSP leaf based on it's (x,y).
     * Sets mobj->bspLeaf properly. Calling with flags==0 only updates
     * the BspLeaf pointer. Can be called without unlinking first.
     * Should be called AFTER mobj translation to (re-)insert the mobj.
     */
    void link(struct mobj_s &mobj, dint flags);

    /**
     * Unlinks a mobj from everything it has been linked to. Should be called
     * BEFORE mobj translation to extract the mobj.
     *
     * @param mobj  Mobj to be unlinked.
     *
     * @return  DDLINK_* flags denoting what the mobj was unlinked from
     * (in case we need to re-link).
     */
    dint unlink(struct mobj_s &mobj);

#ifdef __CLIENT__
public:  // Particle generators -------------------------------------------------------

    /**
     * Returns the total number of @em active generators in the map.
     */
    dint generatorCount() const;

    /**
     * Attempt to spawn a new (particle) generator for the map. If no free identifier
     * is available then @c nullptr is returned.
     */
    Generator *newGenerator();

    /**
     * Iterate over all generators in the map making a callback for each.
     *
     * @param func  Callback to make for each Generator.
     */
    LoopResult forAllGenerators(std::function<LoopResult (Generator &)> func) const;

    /**
     * Iterate over all generators in the map which are present in the identified
     * sector making a callback for each.
     *
     * @param sector  Sector containing the generators to process.
     * @param func    Callback to make for each Generator.
     */
    LoopResult forAllGeneratorsInSector(Sector const &sector, std::function<LoopResult (Generator &)> func) const;

    void unlink(Generator &generator);

#endif // __CLIENT__

public:  // Poly objects ----------------------------------------------------------

    /**
     * Returns the total number of Polyobjs in the map.
     */
    dint polyobjCount() const;

    /**
     * Lookup a Polyobj in the map by it's unique @a index.
     */
    Polyobj &polyobj(dint index) const;
    Polyobj *polyobjPtr(dint index) const;

    /**
     * Iterate through the Polyobjs of the map.
     *
     * @param func  Callback to make for each Polyobj.
     */
    LoopResult forAllPolyobjs(std::function<LoopResult (Polyobj &)> func) const;

    /**
     * Link the specified @a polyobj in any internal data structures for
     * bookkeeping purposes. Should be called AFTER Polyobj rotation and/or
     * translation to (re-)insert the polyobj.
     *
     * @param polyobj  Poly-object to be linked.
     */
    void link(Polyobj &polyobj);

    /**
     * Unlink the specified @a polyobj from any internal data structures for
     * bookkeeping purposes. Should be called BEFORE Polyobj rotation and/or
     * translation to extract the polyobj.
     *
     * @param polyobj  Poly-object to be unlinked.
     */
    void unlink(Polyobj &polyobj);

public:  // Sectors ---------------------------------------------------------------

    /**
     * Returns the total number of Sectors in the map.
     */
    dint sectorCount() const;

    /**
     * Lookup a Sector in the map by it's unique @a index.
     */
    Sector &sector(dint index) const;
    Sector *sectorPtr(dint index) const;

    /**
     * Iterate through the Sectors of the map.
     *
     * @param func  Callback to make for each Sector.
     */
    LoopResult forAllSectors(std::function<LoopResult (Sector &)> func) const;

    /**
     * Increment validCount before calling this routine. The callback function
     * will be called once for each sector the mobj is touching (totally or
     * partly inside). This is not a 3D check; the mobj may actually reside
     * above or under the sector.
     */
    LoopResult forAllSectorsTouchingMobj(struct mobj_s &mob, std::function<LoopResult (Sector &)> func) const;

public:  // Sector clusters -------------------------------------------------------

    /**
     * Returns the total number of SectorClusters in the map.
     */
    dint clusterCount() const;

    /**
     * Determine the SectorCluster which contains @a point and which is on the
     * back side of the BS partition that lies in front of @a point.
     *
     * @param point  Map space coordinates to determine the BSP leaf for.
     *
     * @return  SectorCluster containing the specified point if any or @c nullptr
     * if the clusters have not yet been built.
     */
    SectorCluster *clusterAt(Vector2d const &point) const;

    /**
     * Iterate through the SectorClusters of the map.
     *
     * @param sector  If not @c nullptr, traverse the clusters of this Sector only.
     * @param func    Callback to make for each SectorCluster.
     */
    LoopResult forAllClusters(Sector *sector, std::function<LoopResult (SectorCluster &)> func);

    /**
     * @overload
     */
    inline LoopResult forAllClusters(std::function<LoopResult (SectorCluster &)> func) {
        return forAllClusters(nullptr, func);
    }

public:  // Skies -----------------------------------------------------------------

    /**
     * Returns the logical sky for the map.
     */
    Sky &sky() const;

#ifdef __CLIENT__
    coord_t skyFix(bool ceiling) const;

    inline coord_t skyFixFloor() const   { return skyFix(false /*the floor*/); }
    inline coord_t skyFixCeiling() const { return skyFix(true /*the ceiling*/); }

    void setSkyFix(bool ceiling, coord_t newHeight);

    inline void setSkyFixFloor(coord_t newHeight) {
        setSkyFix(false /*the floor*/, newHeight);
    }
    inline void setSkyFixCeiling(coord_t newHeight) {
        setSkyFix(true /*the ceiling*/, newHeight);
    }
#endif

public:  // Subspaces -------------------------------------------------------------

    /**
     * Returns the total number of subspaces in the map.
     */
    dint subspaceCount() const;

    /**
     * Lookup a Subspace in the map by it's unique @a index.
     */
    ConvexSubspace &subspace(dint index) const;
    ConvexSubspace *subspacePtr(dint index) const;

    /**
     * Iterate through the ConvexSubspaces of the map.
     *
     * @param func  Callback to make for each ConvexSubspace.
     */
    LoopResult forAllSubspaces(std::function<LoopResult (ConvexSubspace &)> func) const;

public:  // Vertexs ---------------------------------------------------------------

    /**
     * Returns the total number of Vertexs in the map.
     */
    dint vertexCount() const;

    /**
     * Lookup a Vertex in the map by it's unique @a index.
     */
    Vertex &vertex(dint index) const;
    Vertex *vertexPtr(dint index) const;

    /**
     * Iterate through the Vertexs of the map.
     *
     * @param func  Callback to make for each Vertex.
     */
    LoopResult forAllVertexs(std::function<LoopResult (Vertex &)> func) const;

public:  // Data structures -------------------------------------------------------

    /**
     * Provides access to the entity database.
     */
    EntityDatabase &entityDatabase() const;

    /**
     * Provides access to the primary @ref Mesh geometry owned by the map. Note that
     * further meshes may be assigned to individual elements of the map should their
     * geometries not be representable as a manifold with the primary mesh (e.g.,
     * polyobjs and BSP leaf "extra" meshes).
     */
    Mesh const &mesh() const;

    /**
     * Provides access to the line blockmap.
     */
    LineBlockmap const &lineBlockmap() const;

    /**
     * Provides access to the mobj blockmap.
     */
    Blockmap const &mobjBlockmap() const;

    /**
     * Provides access to the polyobj blockmap.
     */
    Blockmap const &polyobjBlockmap() const;

    /**
     * Provides access to the convex subspace blockmap.
     */
    Blockmap const &subspaceBlockmap() const;

    /**
     * Provides access to the thinker lists for the map.
     */
    Thinkers /*const*/ &thinkers() const;

    /**
     * Returns @c true iff a BSP tree is available for the map.
     */
    bool hasBspTree() const;

    /**
     * Provides access to map's BSP tree, for efficient traversal.
     */
    BspTree const &bspTree() const;

    /**
     * Determine the BSP leaf on the back side of the BS partition that lies
     * in front of the specified point within the map's coordinate space.
     *
     * @note Always returns a valid BspLeaf although the point may not actually
     * lay within it (however it is on the same side of the space partition)!
     *
     * @param point  Map space coordinates to determine the BSP leaf for.
     *
     * @return  BspLeaf instance for that BSP node's leaf.
     */
    BspLeaf &bspLeafAt(Vector2d const &point) const;

    /**
     * @copydoc bspLeafAt()
     *
     * The test is carried out using fixed-point math for behavior compatible
     * with vanilla DOOM. Note that this means there is a maximum size for the
     * point: it cannot exceed the fixed-point 16.16 range (about 65k units).
     */
    BspLeaf &bspLeafAt_FixedPrecision(Vector2d const &point) const;

    /**
     * Given an @a emitter origin, attempt to identify the map element
     * to which it belongs.
     *
     * @param emitter  The sound emitter to be identified.
     * @param sector   The identified sector if found is written here.
     * @param poly     The identified polyobj if found is written here.
     * @param plane    The identified plane if found is written here.
     * @param surface  The identified line side surface if found is written here.
     *
     * @return  @c true iff @a emitter is an identifiable map element.
     */
    bool identifySoundEmitter(ddmobj_base_t const &emitter, Sector **sector,
        Polyobj **poly, Plane **plane, Surface **surface) const;

#ifdef __CLIENT__

    /**
     * Returns @c true iff a LightGrid has been initialized for the map.
     *
     * @see lightGrid()
     */
    bool hasLightGrid();

    /**
     * Provides access to the light grid for the map.
     *
     * @see hasLightGrid()
     */
    LightGrid &lightGrid();

    /**
     * (Re)-initialize the light grid used for smoothed sector lighting.
     *
     * If the grid has not yet been initialized block light sources are determined
     * at this time (SectorClusters must be built for this).
     *
     * If the grid has already been initialized calling this will perform a full update.
     *
     * @note Initialization may take some time depending on the complexity of the
     * map (physial dimensions, number of sectors) and should therefore be done
     * "off-line".
     */
    void initLightGrid();

    /**
     * Link the given @a surface in all material lists and surface sets which
     * the map maintains to improve performance. Only surfaces attributed to
     * the map will be linked (alien surfaces are ignored).
     *
     * @param surface  The surface to be linked.
     */
    void linkInMaterialLists(Surface *surface);

    /**
     * Unlink the given @a surface in all material lists and surface sets which
     * the map maintains to improve performance.
     *
     * @note The material currently attributed to the surface does not matter
     * for unlinking purposes and the surface will be unlinked from all lists
     * regardless.
     *
     * @param surface  The surface to be unlinked.
     */
    void unlinkInMaterialLists(Surface *surface);

    /**
     * Returns the set of scrolling surfaces for the map.
     */
    SurfaceSet /*const*/ &scrollingSurfaces();

    /**
     * $smoothmatoffset: Roll the surface material offset tracker buffers.
     */
    void updateScrollingSurfaces();

    /**
     * Returns the set of tracked planes for the map.
     */
    PlaneSet /*const*/ &trackedPlanes();

    /**
     * $smoothplane: Roll the height tracker buffers.
     */
    void updateTrackedPlanes();

    /**
     * Perform spreading of all contacts in the specified map space @a region.
     */
    void spreadAllContacts(AABoxd const &region);

#endif // __CLIENT__

public:

    /**
     * Returns a rich formatted, textual summary of the map's elements, suitable
     * for logging.
     */
    String elementSummaryAsStyledText() const;

    /**
     * Returns a rich formatted, textual summary of the map's objects, suitable
     * for logging.
     */
    String objectSummaryAsStyledText() const;

    /**
     * To be called to register the commands and variables of this module.
     */
    static void consoleRegister();

    /**
     * To be called to initialize the dummy element arrays (which are used with
     * the DMU API), with a fixed number of @em shared dummies.
     */
    static void initDummies();

public: /// @todo Most of the following should be private:

    /**
     * Initialize the node piles and link rings. To be called after map load.
     */
    void initNodePiles();

    /**
     * Initialize all polyobjs in the map. To be called after map load.
     */
    void initPolyobjs();

#ifdef __CLIENT__
    /**
     * Fixing the sky means that for adjacent sky sectors the lower sky
     * ceiling is lifted to match the upper sky. The raising only affects
     * rendering, it has no bearing on gameplay.
     */
    void initSkyFix();

    /**
     * Rebuild the surface material lists. To be called when a full update is
     * necessary.
     */
    void buildMaterialLists();

    /**
     * Initializes bias lighting for the map. New light sources are initialized
     * from the loaded Light definitions. Map surfaces are prepared for tracking
     * rays.
     *
     * Must be called before rendering a frame with bias lighting enabled.
     */
    void initBias();

    /**
     * Initialize the map object => BSP leaf "contact" blockmaps.
     */
    void initContactBlockmaps();

    /**
     * Spawn all generators for the map which should be initialized automatically
     * during map setup.
     */
    void initGenerators();

    /**
     * Attempt to spawn all flat-triggered particle generators for the map.
     * To be called after map setup is completed.
     *
     * @note Cannot presently be done in @ref initGenerators() as this is called
     *       during initial Map load and before any saved game has been loaded.
     */
    void spawnPlaneParticleGens();

    /**
     * Destroys all clientside clmobjs in the map. To be called when a network
     * game ends.
     */
    void clearClMobjs();

    /**
     * Deletes hidden, unpredictable or nulled mobjs for which we have not received
     * updates in a while.
     */
    void expireClMobjs();

    /**
     * Find/create a client mobj with the unique identifier @a id. Client mobjs are
     * just like normal mobjs, except they have additional network state.
     *
     * To check whether a given mobj is a client mobj, use Cl_IsClientMobj(). The network
     * state can then be accessed with ClMobj_GetInfo().
     *
     * @param id         Identifier of the client mobj. Every client mobj has a unique
     *                   identifier.
     * @param canCreate  @c true= create a new client mobj if none existing.
     *
     * @return  Pointer to the gameside mobj.
     */
    struct mobj_s *clMobjFor(thid_t id, bool canCreate = false) const;

    /**
     * Iterate all client mobjs, making a callback for each. Iteration ends if a
     * callback returns a non-zero value.
     *
     * @param callback  Function to callback for each client mobj.
     * @param context   Data pointer passed to the callback.
     *
     * @return  @c 0 if all callbacks return @c 0; otherwise the result of the last.
     */
    dint clMobjIterator(dint (*callback) (struct mobj_s *, void *), void *context = nullptr);

    /**
     * Provides readonly access to the client mobj hash.
     */
    ClMobjHash const &clMobjHash() const;

protected:
    /// Observes WorldSystem FrameBegin
    void worldSystemFrameBegins(bool resetNextViewer);

#endif // __CLIENT__

public:  // Editing ---------------------------------------------------------------

    /**
     * Returns @c true iff the map is currently in an editable state.
     */
    bool isEditable() const;

    /**
     * Switch the map from editable to non-editable (i.e., playable) state,
     * incorporating any new map elements, (re)building the BSP, etc...
     *
     * @return  @c true= mode switch was completed successfully.
     */
    bool endEditing();

    /**
     * @see isEditable()
     */
    Vertex *createVertex(Vector2d const &origin,
                         dint archiveIndex = MapElement::NoIndex);

    /**
     * @see isEditable()
     */
    Line *createLine(Vertex &v1, Vertex &v2, dint flags = 0,
                     Sector *frontSector = nullptr, Sector *backSector = nullptr,
                     dint archiveIndex = MapElement::NoIndex);

    /**
     * @see isEditable()
     */
    Polyobj *createPolyobj(Vector2d const &origin);

    /**
     * @see isEditable()
     */
    Sector *createSector(dfloat lightLevel, Vector3f const &lightColor,
                         dint archiveIndex = MapElement::NoIndex);

    /**
     * Provides a list of all the editable lines in the map.
     */
    typedef QList<Line *> Lines;
    Lines const &editableLines() const;

    /**
     * Provides a list of all the editable polyobjs in the map.
     */
    typedef QList<Polyobj *> Polyobjs;
    Polyobjs const &editablePolyobjs() const;

    /**
     * Provides a list of all the editable sectors in the map.
     */
    typedef QList<Sector *> Sectors;
    Sectors const &editableSectors() const;

    inline dint editableLineCount() const    { return editableLines().count(); }

    inline dint editablePolyobjCount() const { return editablePolyobjs().count(); }

    inline dint editableSectorCount() const  { return editableSectors().count(); }

private:
    DENG2_PRIVATE(d)
};

}  // namespace de

#endif  // DENG_WORLD_MAP_H
