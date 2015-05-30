/** @file api_console.cpp  Public Console API.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#define DENG_NO_API_MACROS_CONSOLE
#include <doomsday/console/exec.h>
#include <doomsday/console/cmd.h>
#include <doomsday/console/var.h>
#include "api_console.h"
#include "dd_main.h"

#undef Con_SetUri2
void Con_SetUri2(char const *path, uri_s const *uri, int svFlags)
{
    cvar_t* var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetUri2(var, *reinterpret_cast<de::Uri const *>(uri), svFlags);
}

#undef Con_SetUri
void Con_SetUri(char const *path, uri_s const *uri)
{
    Con_SetUri2(path, uri, 0);
}

#undef Con_SetString2
void Con_SetString2(char const *path, char const *text, int svFlags)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetString2(var, text, svFlags);
}

#undef Con_SetString
void Con_SetString(char const *path, char const *text)
{
    Con_SetString2(path, text, 0);
}

#undef Con_SetInteger2
void Con_SetInteger2(char const *path, int value, int svFlags)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetInteger2(var, value, svFlags);
}

#undef Con_SetInteger
void Con_SetInteger(char const *path, int value)
{
    Con_SetInteger2(path, value, 0);
}

#undef Con_SetFloat2
void Con_SetFloat2(char const *path, float value, int svFlags)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return;
    CVar_SetFloat2(var, value, svFlags);
}

#undef Con_SetFloat
void Con_SetFloat(char const *path, float value)
{
    Con_SetFloat2(path, value, 0);
}

#undef Con_GetInteger
int Con_GetInteger(char const *path)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Integer(var);
}

#undef Con_GetFloat
float Con_GetFloat(char const *path)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Float(var);
}

#undef Con_GetByte
byte Con_GetByte(char const *path)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return 0;
    return CVar_Byte(var);
}

#undef Con_GetString
char const *Con_GetString(char const *path)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return "";
    return CVar_String(var);
}

#undef Con_GetUri
uri_s const *Con_GetUri(char const *path)
{
    return reinterpret_cast<uri_s const *>(&CVar_Uri(Con_FindVariable(path)));
}

cvartype_t Con_GetVariableType(char const *path)
{
    cvar_t *var = Con_FindVariable(path);
    if(!var) return CVT_NULL;
    return var->type;
}

/**
 * Wrapper for Con_Execute
 * Public method for plugins to execute console commands.
 */
int DD_Execute(int silent, const char* command)
{
    return Con_Execute(CMDS_GAME, command, silent, false);
}

/**
 * Exported version of Con_Executef
 */
int DD_Executef(int silent, const char *command, ...)
{
    va_list         argptr;
    char            buffer[4096];

    va_start(argptr, command);
    vsprintf(buffer, command, argptr);
    va_end(argptr);
    return Con_Execute(CMDS_GAME, buffer, silent, false);
}

DENG_DECLARE_API(Con) =
{
    { DE_API_CONSOLE },

    Con_Open,
    Con_AddCommand,
    Con_AddVariable,
    Con_AddCommandList,
    Con_AddVariableList,

    Con_GetVariableType,

    Con_GetByte,
    Con_GetInteger,
    Con_GetFloat,
    Con_GetString,
    Con_GetUri,

    Con_SetInteger2,
    Con_SetInteger,

    Con_SetFloat2,
    Con_SetFloat,

    Con_SetString2,
    Con_SetString,

    Con_SetUri2,
    Con_SetUri,

    App_Error,

    DD_Execute,
    DD_Executef
};
