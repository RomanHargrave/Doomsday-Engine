#define DENG_NO_API_MACROS_MATERIALS

#include "de_base.h"
#include "de_resource.h"
#include "api_material.h"

using namespace de;

#undef DD_MaterialForTextureUri
DENG_EXTERN_C Material *DD_MaterialForTextureUri(uri_s const *textureUri)
{
    if(!textureUri) return 0; // Not found.

    try
    {
        de::Uri uri = App_ResourceSystem().textureManifest(reinterpret_cast<de::Uri const &>(*textureUri)).composeUri();
        uri.setScheme(DD_MaterialSchemeNameForTextureScheme(uri.scheme()));
        return &App_ResourceSystem().material(uri);
    }
    catch(MaterialManifest::MissingMaterialError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    catch(ResourceSystem::UnknownSchemeError const &er)
    {
        // Log but otherwise ignore this error.
        LOG_RES_WARNING(er.asText() + ", ignoring.");
    }
    catch(ResourceSystem::MissingManifestError const &)
    {} // Ignore this error.

    return 0; // Not found.
}

#undef Materials_ComposeUri
DENG_EXTERN_C struct uri_s *Materials_ComposeUri(materialid_t materialId)
{
    MaterialManifest &manifest = App_ResourceSystem().toMaterialManifest(materialId);
    return reinterpret_cast<uri_s *>(new de::Uri(manifest.composeUri()));
}

#undef Materials_ResolveUri
DENG_EXTERN_C materialid_t Materials_ResolveUri(struct uri_s const *uri)
{
    try
    {
        return App_ResourceSystem().materialManifest(*reinterpret_cast<de::Uri const *>(uri)).id();
    }
    catch(ResourceSystem::MissingManifestError const &)
    {} // Ignore this error.
    return NOMATERIALID;
}

#undef Materials_ResolveUriCString
DENG_EXTERN_C materialid_t Materials_ResolveUriCString(char const *uriCString)
{
    if(uriCString && uriCString[0])
    {
        try
        {
            return App_ResourceSystem().materialManifest(de::Uri(uriCString, RC_NULL)).id();
        }
        catch(ResourceSystem::MissingManifestError const &)
        {} // Ignore this error.
    }
    return NOMATERIALID;
}

DENG_DECLARE_API(Material) =
{
    { DE_API_MATERIALS },
    DD_MaterialForTextureUri,
    Materials_ComposeUri,
    Materials_ResolveUri,
    Materials_ResolveUriCString
};
