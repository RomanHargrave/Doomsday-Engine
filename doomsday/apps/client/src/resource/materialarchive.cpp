/** @file materialarchive.cpp Material Archive.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2015 Daniel Swanson <danij@dengine.net>
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
#include "de_console.h"
#include <doomsday/uri.h>
#include <de/StringPool>

#include "resource/materialarchive.h"

/// For identifying the archived format version. Written to disk.
#define MATERIALARCHIVE_VERSION     4

#define ASEG_MATERIAL_ARCHIVE       112

// Used to denote unknown Material references in records. Written to disk.
#define UNKNOWN_MATERIALNAME        "DD_BADTX"

namespace de {

static String readArchivedPath(reader_s &reader)
{
    char _path[9];
    Reader_Read(&reader, _path, 8); _path[8] = 0;
    return QString(QByteArray(_path, qstrlen(_path)).toPercentEncoding());
}

static void readArchivedUri(Uri &uri, int version, reader_s &reader)
{
    if(version >= 4)
    {
        // A serialized, percent encoded URI.
        Uri_Read(reinterpret_cast<uri_s *>(&uri), &reader);
    }
    else if(version == 3)
    {
        // A percent encoded textual URI.
        ddstring_t *_uri = Str_NewFromReader(&reader);
        uri.setUri(Str_Text(_uri), RC_NULL);
        Str_Delete(_uri);
    }
    else if(version == 2)
    {
        // An unencoded textual URI.
        ddstring_t *_uri = Str_NewFromReader(&reader);
        uri.setUri(QString(QByteArray(Str_Text(_uri), Str_Length(_uri)).toPercentEncoding()), RC_NULL);
        Str_Delete(_uri);
    }
    else // ver 1
    {
        // A short textual path (unencoded).
        uri.setPath(readArchivedPath(reader));

        // Plus a legacy scheme id.
        int oldSchemeId = Reader_ReadByte(&reader);
        switch(oldSchemeId)
        {
        case 0: uri.setScheme("Textures"); break;
        case 1: uri.setScheme("Flats");    break;
        case 2: uri.setScheme("Sprites");  break;
        case 3: uri.setScheme("System");   break;
        default:
            throw Error("readArchiveUri", QString("Unknown old-scheme id #%1, expected [0..3)").arg(oldSchemeId));
        }
    }
}

/**
 * Mappings between URI and Material.
 * The pointer user value holds a pointer to the resolved Material (if found).
 * The integer user value tracks whether a material has yet been looked up.
 */
typedef StringPool Records;
typedef StringPool::Id SerialId;

static Material *findRecordMaterial(Records &records, SerialId id)
{
    // Time to lookup the material for the record's URI?
    if(!records.userValue(id))
    {
        Material *material = 0;
        try
        {
            material = &App_ResourceSystem().material(Uri(records.stringRef(id), RC_NULL));
        }
        catch(ResourceSystem::MissingManifestError const &)
        {} // Ignore this error.

        records.setUserPointer(id, material);
        records.setUserValue(id, true);
        return material;
    }

    return (Material *) records.userPointer(id);
}

DENG2_PIMPL(MaterialArchive)
{
    int version = MATERIALARCHIVE_VERSION;

    bool useSegments = false;   ///< Segment id assertion (Hexen saves).
    Records records;            ///< Mappings between URI and Material.
    int numFlats = 0;           ///< Used with older versions.

    Instance(Public *i) : Base(i)
    {}

    inline SerialId insertRecord(Uri const &uri)
    {
        return records.intern(uri.compose());
    }

    void beginSegment(int seg, writer_s &writer)
    {
        if(!useSegments) return;
        Writer_WriteUInt32(&writer, seg);
    }

    void assertSegment(int seg, reader_s &reader)
    {
        if(!useSegments) return;

        int i = Reader_ReadUInt32(&reader);
        if(i != seg)
        {
            throw MaterialArchive::ReadError("MaterialArchive::assertSegment",
                                             QString("Expected ASEG_MATERIAL_ARCHIVE (%1), but got %2")
                                                 .arg(ASEG_MATERIAL_ARCHIVE).arg(i));
        }
    }

    void writeHeader(writer_s &writer)
    {
        beginSegment(ASEG_MATERIAL_ARCHIVE, writer);
        Writer_WriteByte(&writer, version);
    }

    void readHeader(reader_s &reader)
    {
        assertSegment(ASEG_MATERIAL_ARCHIVE, reader);
        version = Reader_ReadByte(&reader);
    }

    void readGroup(reader_s &reader)
    {
        DENG_ASSERT(version >= 1);

        // Read the group header.
        int num = Reader_ReadUInt16(&reader);

        // Read the group records.
        Uri uri;
        for(int i = 0; i < num; ++i)
        {
            readArchivedUri(uri, version, reader);
            insertRecord(uri);
        }
    }

    void writeGroup(writer_s &writer)
    {
        // Write the group header.
        Writer_WriteUInt16(&writer, records.size());

        // Write the group records.
        Uri uri;
        for(int i = 1; i < int(records.size()) + 1; ++i)
        {
            uri.setUri(records.stringRef(i), RC_NULL);
            Uri_Write(reinterpret_cast<uri_s const *>(&uri), &writer);
        }
    }
};

MaterialArchive::MaterialArchive(int useSegments, bool recordSymbolicMaterials)
    : d(new Instance(this))
{
    d->useSegments = useSegments;

    if(recordSymbolicMaterials)
    {
        // The first material is the special "unknown material".
        d->insertRecord(de::Uri(UNKNOWN_MATERIALNAME, RC_NULL));
    }
}

struct findUniqueSerialIdWorker_params {
    Records *records;
    Material *material;
};

static int findUniqueSerialIdWorker(SerialId id, void *parameters)
{
    findUniqueSerialIdWorker_params *parm = (findUniqueSerialIdWorker_params*) parameters;

    // Is this the material we are looking for?
    if(findRecordMaterial(*parm->records, id) == parm->material)
        return id;

    return 0; // Continue iteration.
}

materialarchive_serialid_t MaterialArchive::findUniqueSerialId(Material *material) const
{
    if(!material) return 0; // Invalid.

    findUniqueSerialIdWorker_params parm;
    parm.records  = &d->records;
    parm.material = material;

    SerialId found = d->records.iterate(findUniqueSerialIdWorker, &parm);
    if(found) return found; // Yes. Return existing serial.

    return d->records.size() + 1;
}

Material *MaterialArchive::find(materialarchive_serialid_t serialId, int group) const
{
    if(serialId <= 0 || serialId > d->records.size() + 1) return 0; // Invalid.

    // A group offset?
    if(d->version < 1 && group == 1)
    {
        // Group 1 = walls (skip over the flats).
        serialId += d->numFlats;
    }

    if(d->version <= 1)
    {
        // The special case "unknown" material?
        Uri uri(d->records.stringRef(serialId), RC_NULL);
        if(!uri.path().toStringRef().compareWithoutCase(UNKNOWN_MATERIALNAME))
            return 0;
    }

    return findRecordMaterial(d->records, serialId);
}

materialarchive_serialid_t MaterialArchive::addRecord(Material const &material)
{
    SerialId id = d->insertRecord(material.manifest().composeUri());
    d->records.setUserPointer(id, const_cast<Material *>(&material));
    d->records.setUserValue(id, true);
    return materialarchive_serialid_t(id);
}

int MaterialArchive::count() const
{
    return d->records.size();
}

void MaterialArchive::write(writer_s &writer) const
{
    d->writeHeader(writer);
    d->writeGroup(writer);
}

void MaterialArchive::read(reader_s &reader, int forcedVersion)
{
    d->records.clear();

    d->readHeader(reader);
    // Are we interpreting a specific version?
    if(forcedVersion >= 0)
    {
        d->version = forcedVersion;
    }

    if(d->version >= 1)
    {
        d->readGroup(reader);
        return;
    }

    // The old format saved materials used on floors and walls into seperate
    // groups. At this time only Flats could be used on floors and Textures
    // on walls.
    {
        // Group 0 (floors)
        Uri uri("Flats", "");
        d->numFlats = Reader_ReadUInt16(&reader);
        for(int i = 0; i < d->numFlats; ++i)
        {
            uri.setPath(readArchivedPath(reader));
            d->insertRecord(uri);
        }
    }
    {
        // Group 1 (walls)
        Uri uri("Textures", "");
        int num = Reader_ReadUInt16(&reader);
        for(int i = 0; i < num; ++i)
        {
            uri.setPath(readArchivedPath(reader));
            d->insertRecord(uri);
        }
    }
}

} // namespace de

/*
 * C Wrapper API:
 */

#define DENG_NO_API_MACROS_MATERIAL_ARCHIVE

#include "api_materialarchive.h"

#define TOINTERNAL(inst) \
    reinterpret_cast<de::MaterialArchive *>(inst)

#define TOINTERNAL_CONST(inst) \
    reinterpret_cast<de::MaterialArchive const*>(inst)

#define SELF(inst) \
    DENG2_ASSERT(inst); \
    de::MaterialArchive *self = TOINTERNAL(inst)

#define SELF_CONST(inst) \
    DENG2_ASSERT(inst); \
    de::MaterialArchive const *self = TOINTERNAL_CONST(inst)

#undef MaterialArchive_New
MaterialArchive *MaterialArchive_New(int useSegments)
{
    auto *archive = new de::MaterialArchive(useSegments);

    // Populate the archive using the application's global/main Material collection.
    App_ResourceSystem().forAllMaterials([&archive] (Material &material)
    {
        archive->addRecord(material);
        return de::LoopContinue;
    });

    return reinterpret_cast<MaterialArchive *>(archive);
}

#undef MaterialArchive_NewEmpty
MaterialArchive *MaterialArchive_NewEmpty(int useSegments)
{
    return reinterpret_cast<MaterialArchive *>(new de::MaterialArchive(useSegments, false /*don't populate*/));
}

#undef MaterialArchive_Delete
void MaterialArchive_Delete(MaterialArchive *arc)
{
    if(arc)
    {
        SELF(arc);
        delete self;
    }
}

#undef MaterialArchive_FindUniqueSerialId
materialarchive_serialid_t MaterialArchive_FindUniqueSerialId(MaterialArchive const *arc, Material *mat)
{
    SELF_CONST(arc);
    return self->findUniqueSerialId(mat);
}

#undef MaterialArchive_Find
Material *MaterialArchive_Find(MaterialArchive const *arc, materialarchive_serialid_t serialId, int group)
{
    SELF_CONST(arc);
    return self->find(serialId, group);
}

#undef MaterialArchive_Count
int MaterialArchive_Count(MaterialArchive const *arc)
{
    SELF_CONST(arc);
    return self->count();
}

#undef MaterialArchive_Write
void MaterialArchive_Write(MaterialArchive const *arc, Writer *writer)
{
    SELF_CONST(arc);
    self->write(*writer);
}

#undef MaterialArchive_Read
void MaterialArchive_Read(MaterialArchive *arc, Reader *reader, int forcedVersion)
{
    SELF(arc);
    self->read(*reader, forcedVersion);
}

DENG_DECLARE_API(MaterialArchive) =
{
    { DE_API_MATERIAL_ARCHIVE },
    MaterialArchive_New,
    MaterialArchive_NewEmpty,
    MaterialArchive_Delete,
    MaterialArchive_FindUniqueSerialId,
    MaterialArchive_Find,
    MaterialArchive_Count,
    MaterialArchive_Write,
    MaterialArchive_Read
};
