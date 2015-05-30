/** @file scriptsystem.cpp  Subsystem for scripts.
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

#include "de/ScriptSystem"
#include "de/App"
#include "de/Record"
#include "de/Module"
#include "de/Version"
#include "de/ArrayValue"
#include "de/NumberValue"
#include "de/RecordValue"
#include "de/BlockValue"
#include "de/DictionaryValue"
#include "de/math.h"

#include <QMap>

namespace de {

Value *Function_Path_WithoutFileName(Context &, Function::ArgumentValues const &args)
{
    return new TextValue(args.at(0)->asText().fileNamePath());
}

Value *Function_String_FileNamePath(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().fileNamePath());
}

Value *Function_String_FileNameExtension(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().fileNameExtension());
}

Value *Function_String_FileNameWithoutExtension(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().fileNameWithoutExtension());
}

Value *Function_String_FileNameAndPathWithoutExtension(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().fileNameAndPathWithoutExtension());
}

Value *Function_String_Upper(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().upper());
}

Value *Function_String_Lower(Context &ctx, Function::ArgumentValues const &)
{
    return new TextValue(ctx.instanceScope().asText().lower());
}

Value *Function_Dictionary_Keys(Context &ctx, Function::ArgumentValues const &)
{    
    return ctx.instanceScope().as<DictionaryValue>().contentsAsArray(DictionaryValue::Keys);
}

Value *Function_Dictionary_Values(Context &ctx, Function::ArgumentValues const &)
{
    return ctx.instanceScope().as<DictionaryValue>().contentsAsArray(DictionaryValue::Values);
}

static File const &fileInstance(Context &ctx)
{
    Record const *obj = ctx.instanceScope().as<RecordValue>().record();
    if(!obj)
    {
        throw Value::IllegalError("ScriptSystem::fileInstance", "No File instance available");
    }

    // The record is expected to have a path (e.g., File info record).
    return App::rootFolder().locate<File>(obj->gets("path", "/"));
}

Value *Function_File_Locate(Context &ctx, Function::ArgumentValues const &args)
{
    Path const relativePath = args.at(0)->asText();

    if(File const *found = fileInstance(ctx).tryFollowPath(relativePath)->maybeAs<File>())
    {
        return new RecordValue(found->info());
    }

    // Wasn't there, result is None.
    return 0;
}

Value *Function_File_Read(Context &ctx, Function::ArgumentValues const &)
{
    QScopedPointer<BlockValue> data(new BlockValue);
    fileInstance(ctx) >> *data;
    return data.take();
}

Value *Function_File_ReadUtf8(Context &ctx, Function::ArgumentValues const &)
{
    Block raw;
    fileInstance(ctx) >> raw;
    return new TextValue(String::fromUtf8(raw));
}

static ScriptSystem *_scriptSystem = 0;

DENG2_PIMPL(ScriptSystem)
, DENG2_OBSERVES(Record, Deletion)
{
    Binder binder;

    /// Built-in special modules. These are constructed by native code and thus not
    /// parsed from any script.
    typedef QMap<String, Record *> NativeModules;
    NativeModules nativeModules; // not owned
    Record coreModule;  // Script: built-in script classes and functions.
    Record versionModule; // Version: information about the platform and build
    Record pathModule;    // Path: path manipulation routines (based on native classes Path, NativePath, String)

    /// Resident modules.
    typedef QMap<String, Module *> Modules; // owned
    Modules modules;

    QList<Path> additionalImportPaths;

    Instance(Public *i) : Base(*i)
    {
        initCoreModule();

        // Setup the Version module.
        {
            Version ver;
            Record &mod = versionModule;
            ArrayValue *num = new ArrayValue;
            *num << NumberValue(ver.major) << NumberValue(ver.minor)
                 << NumberValue(ver.patch) << NumberValue(ver.build);
            mod.addArray  ("VERSION",  num                       ).setReadOnly();
            mod.addText   ("TEXT",     ver.asText()              ).setReadOnly();
            mod.addNumber ("BUILD",    ver.build                 ).setReadOnly();
            mod.addText   ("OS",       Version::operatingSystem()).setReadOnly();
            mod.addNumber ("CPU_BITS", Version::cpuBits()        ).setReadOnly();
            mod.addBoolean("DEBUG",    Version::isDebugBuild()   ).setReadOnly();
#ifdef DENG_STABLE
            mod.addBoolean("STABLE",   true).setReadOnly();
#else
            mod.addBoolean("STABLE",   false).setReadOnly();
#endif
            addNativeModule("Version", mod);
        }

        // Setup the Path module.
        binder.init(pathModule)
                << DENG2_FUNC(Path_WithoutFileName, "withoutFileName", "path");
        addNativeModule("Path", pathModule);
    }

    ~Instance()
    {
        qDeleteAll(modules.values());

        DENG2_FOR_EACH(NativeModules, i, nativeModules)
        {
            i.value()->audienceForDeletion() -= this;
        }
    }

    void initCoreModule()
    {
        // Dictionary
        {
            Record &dict = coreModule.addRecord("Dictionary");
            binder.init(dict)
                    << DENG2_FUNC_NOARG(Dictionary_Keys, "keys")
                    << DENG2_FUNC_NOARG(Dictionary_Values, "values");
        }

        // String
        {
            Record &dict = coreModule.addRecord("String");
            binder.init(dict)
                    << DENG2_FUNC_NOARG(String_Upper, "upper")
                    << DENG2_FUNC_NOARG(String_Lower, "lower")
                    << DENG2_FUNC_NOARG(String_FileNamePath, "fileNamePath")
                    << DENG2_FUNC_NOARG(String_FileNameExtension, "fileNameExtension")
                    << DENG2_FUNC_NOARG(String_FileNameWithoutExtension, "fileNameWithoutExtension")
                    << DENG2_FUNC_NOARG(String_FileNameAndPathWithoutExtension, "fileNameAndPathWithoutExtension");
        }

        // File
        {
            Record &dict = coreModule.addRecord("File");
            binder.init(dict)
                    << DENG2_FUNC      (File_Locate, "locate", "relativePath")
                    << DENG2_FUNC_NOARG(File_Read, "read")
                    << DENG2_FUNC_NOARG(File_ReadUtf8, "readUtf8");
        }

        addNativeModule("Core", coreModule);
    }

    void addNativeModule(String const &name, Record &module)
    {
        nativeModules.insert(name, &module); // not owned
        module.audienceForDeletion() += this;
    }

    void removeNativeModule(String const &name)
    {
        if(!nativeModules.contains(name)) return;

        nativeModules[name]->audienceForDeletion() -= this;
        nativeModules.remove(name);
    }

    void recordBeingDeleted(Record &record)
    {
        QMutableMapIterator<String, Record *> iter(nativeModules);
        while(iter.hasNext())
        {
            iter.next();
            if(iter.value() == &record)
            {
                iter.remove();
            }
        }
    }
};

ScriptSystem::ScriptSystem() : d(new Instance(this))
{
    _scriptSystem = this;
}

ScriptSystem::~ScriptSystem()
{
    _scriptSystem = 0;
}

void ScriptSystem::addModuleImportPath(Path const &path)
{
    d->additionalImportPaths << path;
}

void ScriptSystem::removeModuleImportPath(Path const &path)
{
    d->additionalImportPaths.removeOne(path);
}

void ScriptSystem::addNativeModule(String const &name, Record &module)
{
    d->addNativeModule(name, module);
}

void ScriptSystem::removeNativeModule(String const &name)
{
    d->removeNativeModule(name);
}

Record &ScriptSystem::nativeModule(String const &name)
{
    Instance::NativeModules::const_iterator foundNative = d->nativeModules.constFind(name);
    DENG2_ASSERT(foundNative != d->nativeModules.constEnd());
    return *foundNative.value();
}

StringList ScriptSystem::nativeModules() const
{
    return d->nativeModules.keys();
}

namespace internal {
    static bool sortFilesByModifiedAt(File *a, File *b) {
        DENG2_ASSERT(a != b);
        return de::cmp(a->status().modifiedAt, b->status().modifiedAt) < 0;
    }
}

File const *ScriptSystem::tryFindModuleSource(String const &name, String const &localPath)
{
    // Fall back on the default if the config hasn't been imported yet.
    std::auto_ptr<ArrayValue> defaultImportPath(new ArrayValue);
    defaultImportPath->add("");
    defaultImportPath->add("*"); // Newest module with a matching name.
    ArrayValue const *importPath = defaultImportPath.get();
    try
    {
        importPath = &App::config().geta("importPath");
    }
    catch(Record::NotFoundError const &)
    {}

    // Compile a list of all possible import locations.
    StringList importPaths;
    DENG2_FOR_EACH_CONST(ArrayValue::Elements, i, importPath->elements())
    {
        importPaths << (*i)->asText();
    }
    foreach(Path const &path, d->additionalImportPaths)
    {
        importPaths << path;
    }

    // Search all import locations.
    foreach(String dir, importPaths)
    {
        String p;
        FileSystem::FoundFiles matching;
        File *found = 0;
        if(dir.empty())
        {
            if(!localPath.empty())
            {
                // Try the local folder.
                p = localPath / name;
            }
            else
            {
                continue;
            }
        }
        else if(dir == "*")
        {
            App::fileSystem().findAll(name + ".de", matching);
            if(matching.empty())
            {
                continue;
            }
            matching.sort(internal::sortFilesByModifiedAt);
            found = matching.back();
            LOG_SCR_VERBOSE("Chose ") << found->path() << " out of " << dint(matching.size())
                                      << " candidates (latest modified)";
        }
        else
        {
            p = dir / name;
        }
        if(!found)
        {
            found = App::rootFolder().tryLocateFile(p + ".de");
        }
        if(found)
        {
            return found;
        }
    }

    return 0;
}

File const &ScriptSystem::findModuleSource(String const &name, String const &localPath)
{
    File const *src = tryFindModuleSource(name, localPath);
    if(!src)
    {
        throw NotFoundError("ScriptSystem::findModuleSource", "Cannot find module '" + name + "'");
    }
    return *src;
}

Record &ScriptSystem::builtInClass(String const &name)
{
    return const_cast<Record &>(ScriptSystem::get().nativeModule("Core")
                                .getAs<RecordValue>(name).dereference());
}

ScriptSystem &ScriptSystem::get()
{
    DENG2_ASSERT(_scriptSystem);
    return *_scriptSystem;
}

Record &ScriptSystem::importModule(String const &name, String const &importedFromPath)
{
    LOG_AS("ScriptSystem::importModule");

    // There are some special native modules.
    Instance::NativeModules::const_iterator foundNative = d->nativeModules.constFind(name);
    if(foundNative != d->nativeModules.constEnd())
    {
        return *foundNative.value();
    }

    // Maybe we already have this module?
    Instance::Modules::iterator found = d->modules.find(name);
    if(found != d->modules.end())
    {
        return found.value()->names();
    }

    // Get it from a file, then.
    File const *src = tryFindModuleSource(name, importedFromPath.fileNamePath());
    if(src)
    {
        Module *module = new Module(*src);
        d->modules.insert(name, module); // owned
        return module->names();
    }

    throw NotFoundError("ScriptSystem::importModule", "Cannot find module '" + name + "'");
}

void ScriptSystem::timeChanged(Clock const &)
{
    // perform time-based processing/scheduling/events
}

} // namespace de
