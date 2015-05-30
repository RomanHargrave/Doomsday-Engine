/** @file c_wrapper.h  C wrapper for various libcore classes.
 * @ingroup core
 *
 * Defines a C wrapper API for (some of the) libcore classes. Legacy code
 * can use this wrapper API to access libcore functionality. Note that the
 * identifiers in this file are @em not in the @c de namespace.
 *
 * @note The basic de data types (e.g., dint32) are not available for the C
 * API; instead, only the standard C data types should be used.
 *
 * @authors Copyright © 2011-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_C_WRAPPER_H
#define LIBDENG2_C_WRAPPER_H

#include "libcore.h"

#if defined(__cplusplus) && !defined(DENG2_C_API_ONLY)
using de::dint16;
using de::dint32;
using de::dint64;
using de::duint16;
using de::duint32;
using de::duint64;
using de::dfloat;
using de::ddouble;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * App
 */
DENG2_PUBLIC void App_Log(unsigned int metadata, char const *format, ...);
DENG2_PUBLIC void App_Timer(unsigned int milliseconds, void (*callback)(void));
DENG2_PUBLIC void App_FatalError(char const *msgFormat, ...);

/*
 * CommandLine
 */
DENG2_PUBLIC void CommandLine_Alias(char const *longname, char const *shortname);
DENG2_PUBLIC int CommandLine_Count(void);
DENG2_PUBLIC char const *CommandLine_At(int i);
DENG2_PUBLIC char const *CommandLine_PathAt(int i);
DENG2_PUBLIC char const *CommandLine_Next(void);
DENG2_PUBLIC char const *CommandLine_NextAsPath(void);
DENG2_PUBLIC int CommandLine_Check(char const *check);
DENG2_PUBLIC int CommandLine_CheckWith(char const *check, int num);
DENG2_PUBLIC int CommandLine_Exists(char const *check);
DENG2_PUBLIC int CommandLine_IsOption(int i);
DENG2_PUBLIC int CommandLine_IsMatchingAlias(char const *original, char const *originalOrAlias);

/*
 * LogBuffer
 */
#define DE2_ESC(StringLiteral) "\x1b" #StringLiteral

// Log levels (see de::Log for description).
typedef enum logentry_metadata_e
{
    DE2_LOG_XVERBOSE    = 1,
    DE2_LOG_VERBOSE     = 2,
    DE2_LOG_MESSAGE     = 3,
    DE2_LOG_NOTE        = 4,
    DE2_LOG_WARNING     = 5,
    DE2_LOG_ERROR       = 6,
    DE2_LOG_CRITICAL    = 7,

    DE2_LOG_GENERIC     = 0x10000,
    DE2_LOG_RES         = 0x20000,
    DE2_LOG_MAP         = 0x40000,      ///< Map developer
    DE2_LOG_SCR         = 0x80000,      ///< Script developer
    DE2_LOG_GL          = 0x100000,     ///< GL domain (shaders, etc.)
    DE2_LOG_AUDIO       = 0x200000,     ///< Audio domain
    DE2_LOG_INPUT       = 0x400000,     ///< Input domain
    DE2_LOG_NET         = 0x800000,     ///< Network domain
    DE2_LOG_DEV         = 0x8000000,    ///< Native code developer (i.e., the programmer)

    DE2_DEV_XVERBOSE = DE2_LOG_DEV | DE2_LOG_XVERBOSE,
    DE2_DEV_VERBOSE  = DE2_LOG_DEV | DE2_LOG_VERBOSE,
    DE2_DEV_MSG      = DE2_LOG_DEV | DE2_LOG_MESSAGE,
    DE2_DEV_NOTE     = DE2_LOG_DEV | DE2_LOG_NOTE,
    DE2_DEV_WARNING  = DE2_LOG_DEV | DE2_LOG_WARNING,
    DE2_DEV_ERROR    = DE2_LOG_DEV | DE2_LOG_ERROR,
    DE2_DEV_CRITICAL = DE2_LOG_DEV | DE2_LOG_CRITICAL,

    // RES
    DE2_RES_XVERBOSE = DE2_LOG_RES | DE2_LOG_XVERBOSE,
    DE2_RES_VERBOSE  = DE2_LOG_RES | DE2_LOG_VERBOSE,
    DE2_RES_MSG      = DE2_LOG_RES | DE2_LOG_MESSAGE,
    DE2_RES_NOTE     = DE2_LOG_RES | DE2_LOG_NOTE,
    DE2_RES_WARNING  = DE2_LOG_RES | DE2_LOG_WARNING,
    DE2_RES_ERROR    = DE2_LOG_RES | DE2_LOG_ERROR,
    DE2_RES_CRITICAL = DE2_LOG_RES | DE2_LOG_CRITICAL,

    DE2_DEV_RES_XVERBOSE = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_XVERBOSE,
    DE2_DEV_RES_VERBOSE  = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_VERBOSE,
    DE2_DEV_RES_MSG      = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_MESSAGE,
    DE2_DEV_RES_NOTE     = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_NOTE,
    DE2_DEV_RES_WARNING  = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_WARNING,
    DE2_DEV_RES_ERROR    = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_ERROR,
    DE2_DEV_RES_CRITICAL = DE2_LOG_DEV | DE2_LOG_RES | DE2_LOG_CRITICAL,

    // MAP
    DE2_MAP_XVERBOSE = DE2_LOG_MAP | DE2_LOG_XVERBOSE,
    DE2_MAP_VERBOSE  = DE2_LOG_MAP | DE2_LOG_VERBOSE,
    DE2_MAP_MSG      = DE2_LOG_MAP | DE2_LOG_MESSAGE,
    DE2_MAP_NOTE     = DE2_LOG_MAP | DE2_LOG_NOTE,
    DE2_MAP_WARNING  = DE2_LOG_MAP | DE2_LOG_WARNING,
    DE2_MAP_ERROR    = DE2_LOG_MAP | DE2_LOG_ERROR,
    DE2_MAP_CRITICAL = DE2_LOG_MAP | DE2_LOG_CRITICAL,

    DE2_DEV_MAP_XVERBOSE = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_XVERBOSE,
    DE2_DEV_MAP_VERBOSE  = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_VERBOSE,
    DE2_DEV_MAP_MSG      = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_MESSAGE,
    DE2_DEV_MAP_NOTE     = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_NOTE,
    DE2_DEV_MAP_WARNING  = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_WARNING,
    DE2_DEV_MAP_ERROR    = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_ERROR,
    DE2_DEV_MAP_CRITICAL = DE2_LOG_DEV | DE2_LOG_MAP | DE2_LOG_CRITICAL,

    // SCR
    DE2_SCR_XVERBOSE = DE2_LOG_SCR | DE2_LOG_XVERBOSE,
    DE2_SCR_VERBOSE  = DE2_LOG_SCR | DE2_LOG_VERBOSE,
    DE2_SCR_MSG      = DE2_LOG_SCR | DE2_LOG_MESSAGE,
    DE2_SCR_NOTE     = DE2_LOG_SCR | DE2_LOG_NOTE,
    DE2_SCR_WARNING  = DE2_LOG_SCR | DE2_LOG_WARNING,
    DE2_SCR_ERROR    = DE2_LOG_SCR | DE2_LOG_ERROR,
    DE2_SCR_CRITICAL = DE2_LOG_SCR | DE2_LOG_CRITICAL,

    DE2_DEV_SCR_XVERBOSE = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_XVERBOSE,
    DE2_DEV_SCR_VERBOSE  = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_VERBOSE,
    DE2_DEV_SCR_MSG      = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_MESSAGE,
    DE2_DEV_SCR_NOTE     = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_NOTE,
    DE2_DEV_SCR_WARNING  = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_WARNING,
    DE2_DEV_SCR_ERROR    = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_ERROR,
    DE2_DEV_SCR_CRITICAL = DE2_LOG_DEV | DE2_LOG_SCR | DE2_LOG_CRITICAL,

    // GL
    DE2_GL_XVERBOSE = DE2_LOG_GL | DE2_LOG_XVERBOSE,
    DE2_GL_VERBOSE  = DE2_LOG_GL | DE2_LOG_VERBOSE,
    DE2_GL_MSG      = DE2_LOG_GL | DE2_LOG_MESSAGE,
    DE2_GL_NOTE     = DE2_LOG_GL | DE2_LOG_NOTE,
    DE2_GL_WARNING  = DE2_LOG_GL | DE2_LOG_WARNING,
    DE2_GL_ERROR    = DE2_LOG_GL | DE2_LOG_ERROR,
    DE2_GL_CRITICAL = DE2_LOG_GL | DE2_LOG_CRITICAL,

    DE2_DEV_GL_XVERBOSE = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_XVERBOSE,
    DE2_DEV_GL_VERBOSE  = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_VERBOSE,
    DE2_DEV_GL_MSG      = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_MESSAGE,
    DE2_DEV_GL_NOTE     = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_NOTE,
    DE2_DEV_GL_WARNING  = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_WARNING,
    DE2_DEV_GL_ERROR    = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_ERROR,
    DE2_DEV_GL_CRITICAL = DE2_LOG_DEV | DE2_LOG_GL | DE2_LOG_CRITICAL,

    // AUDIO
    DE2_AUDIO_XVERBOSE = DE2_LOG_AUDIO | DE2_LOG_XVERBOSE,
    DE2_AUDIO_VERBOSE  = DE2_LOG_AUDIO | DE2_LOG_VERBOSE,
    DE2_AUDIO_MSG      = DE2_LOG_AUDIO | DE2_LOG_MESSAGE,
    DE2_AUDIO_NOTE     = DE2_LOG_AUDIO | DE2_LOG_NOTE,
    DE2_AUDIO_WARNING  = DE2_LOG_AUDIO | DE2_LOG_WARNING,
    DE2_AUDIO_ERROR    = DE2_LOG_AUDIO | DE2_LOG_ERROR,
    DE2_AUDIO_CRITICAL = DE2_LOG_AUDIO | DE2_LOG_CRITICAL,

    DE2_DEV_AUDIO_XVERBOSE = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_XVERBOSE,
    DE2_DEV_AUDIO_VERBOSE  = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_VERBOSE,
    DE2_DEV_AUDIO_MSG      = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_MESSAGE,
    DE2_DEV_AUDIO_NOTE     = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_NOTE,
    DE2_DEV_AUDIO_WARNING  = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_WARNING,
    DE2_DEV_AUDIO_ERROR    = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_ERROR,
    DE2_DEV_AUDIO_CRITICAL = DE2_LOG_DEV | DE2_LOG_AUDIO | DE2_LOG_CRITICAL,

    // INPUT
    DE2_INPUT_XVERBOSE = DE2_LOG_INPUT | DE2_LOG_XVERBOSE,
    DE2_INPUT_VERBOSE  = DE2_LOG_INPUT | DE2_LOG_VERBOSE,
    DE2_INPUT_MSG      = DE2_LOG_INPUT | DE2_LOG_MESSAGE,
    DE2_INPUT_NOTE     = DE2_LOG_INPUT | DE2_LOG_NOTE,
    DE2_INPUT_WARNING  = DE2_LOG_INPUT | DE2_LOG_WARNING,
    DE2_INPUT_ERROR    = DE2_LOG_INPUT | DE2_LOG_ERROR,
    DE2_INPUT_CRITICAL = DE2_LOG_INPUT | DE2_LOG_CRITICAL,

    DE2_DEV_INPUT_XVERBOSE = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_XVERBOSE,
    DE2_DEV_INPUT_VERBOSE  = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_VERBOSE,
    DE2_DEV_INPUT_MSG      = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_MESSAGE,
    DE2_DEV_INPUT_NOTE     = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_NOTE,
    DE2_DEV_INPUT_WARNING  = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_WARNING,
    DE2_DEV_INPUT_ERROR    = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_ERROR,
    DE2_DEV_INPUT_CRITICAL = DE2_LOG_DEV | DE2_LOG_INPUT | DE2_LOG_CRITICAL,

    // NET
    DE2_NET_XVERBOSE = DE2_LOG_NET | DE2_LOG_XVERBOSE,
    DE2_NET_VERBOSE  = DE2_LOG_NET | DE2_LOG_VERBOSE,
    DE2_NET_MSG      = DE2_LOG_NET | DE2_LOG_MESSAGE,
    DE2_NET_NOTE     = DE2_LOG_NET | DE2_LOG_NOTE,
    DE2_NET_WARNING  = DE2_LOG_NET | DE2_LOG_WARNING,
    DE2_NET_ERROR    = DE2_LOG_NET | DE2_LOG_ERROR,
    DE2_NET_CRITICAL = DE2_LOG_NET | DE2_LOG_CRITICAL,

    DE2_DEV_NET_XVERBOSE = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_XVERBOSE,
    DE2_DEV_NET_VERBOSE  = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_VERBOSE,
    DE2_DEV_NET_MSG      = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_MESSAGE,
    DE2_DEV_NET_NOTE     = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_NOTE,
    DE2_DEV_NET_WARNING  = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_WARNING,
    DE2_DEV_NET_ERROR    = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_ERROR,
    DE2_DEV_NET_CRITICAL = DE2_LOG_DEV | DE2_LOG_NET | DE2_LOG_CRITICAL
}
logentry_metadata_t;

#define DE2_LOG_DEBUG   (DE2_LOG_DEV | DE2_LOG_VERBOSE)
#define DE2_LOG_TRACE   (DE2_LOG_DEV | DE2_LOG_XVERBOSE)

DENG2_PUBLIC void LogBuffer_EnableStandardOutput(int enable);
DENG2_PUBLIC void LogBuffer_Flush(void);
DENG2_PUBLIC void LogBuffer_Clear(void);
DENG2_PUBLIC void LogBuffer_Printf(unsigned int metadata, char const *format, ...); // note: manual newlines

/*
 * Info
 */
DENG2_OPAQUE(Info)

DENG2_PUBLIC Info *Info_NewFromString(char const *utf8text);
DENG2_PUBLIC Info *Info_NewFromFile(char const *nativePath);
DENG2_PUBLIC void Info_Delete(Info *info);
DENG2_PUBLIC int Info_FindValue(Info *info, char const *path, char *buffer, size_t bufSize);

/*
 * UnixInfo
 */
DENG2_PUBLIC int UnixInfo_GetConfigValue(char const *configFile, char const *key, char *dest, size_t destLen);

/*
 * ByteOrder
 */
DENG2_PUBLIC dint16 LittleEndianByteOrder_ToForeignInt16(dint16 value);
DENG2_PUBLIC dint32 LittleEndianByteOrder_ToForeignInt32(dint32 value);
DENG2_PUBLIC dint64 LittleEndianByteOrder_ToForeignInt64(dint64 value);
DENG2_PUBLIC duint16 LittleEndianByteOrder_ToForeignUInt16(duint16 value);
DENG2_PUBLIC duint32 LittleEndianByteOrder_ToForeignUInt32(duint32 value);
DENG2_PUBLIC duint64 LittleEndianByteOrder_ToForeignUInt64(duint64 value);
DENG2_PUBLIC dfloat LittleEndianByteOrder_ToForeignFloat(dfloat value);
DENG2_PUBLIC ddouble LittleEndianByteOrder_ToForeignDouble(ddouble value);
DENG2_PUBLIC dint16 LittleEndianByteOrder_ToNativeInt16(dint16 value);
DENG2_PUBLIC dint32 LittleEndianByteOrder_ToNativeInt32(dint32 value);
DENG2_PUBLIC dint64 LittleEndianByteOrder_ToNativeInt64(dint64 value);
DENG2_PUBLIC duint16 LittleEndianByteOrder_ToNativeUInt16(duint16 value);
DENG2_PUBLIC duint32 LittleEndianByteOrder_ToNativeUInt32(duint32 value);
DENG2_PUBLIC duint64 LittleEndianByteOrder_ToNativeUInt64(duint64 value);
DENG2_PUBLIC dfloat LittleEndianByteOrder_ToNativeFloat(dfloat value);
DENG2_PUBLIC ddouble LittleEndianByteOrder_ToNativeDouble(ddouble value);

/*
 * BinaryTree
 */
DENG2_OPAQUE(BinaryTree)

/**
 * Create a new BinaryTree node. The binary tree should
 * be destroyed with BinaryTree_Delete() once it is no longer needed.
 *
 * @return  New BinaryTree instance.
 */
DENG2_PUBLIC BinaryTree *BinaryTree_New(void);

/**
 * Create a new BinaryTree node.
 *
 * @param userData  User data to be associated with the new (sub)tree.
 * @return  New BinaryTree instance.
 */
DENG2_PUBLIC BinaryTree *BinaryTree_NewWithUserData(void *userData);

/**
 * Create a new BinaryTree.
 *
 * @param userData  User data to be associated with the new (sub)tree.
 * @param parent  Parent node to associate with the new (sub)tree.
 * @return  New BinaryTree instance.
 */
DENG2_PUBLIC BinaryTree *BinaryTree_NewWithParent(void *userData, BinaryTree *parent);

/**
 * Create a new BinaryTree with right and left subtrees. This binary tree node
 * will take ownership of the subtrees, and destroy them when this node is
 * destroyed.
 *
 * @param userData  User data to be associated with the new (sub)tree.
 * @param rightSubtree  Right child subtree. Can be @a NULL.
 * @param leftSubtree   Left child subtree. Can be @c NULL.
 * @return  New BinaryTree instance.
 */
DENG2_PUBLIC BinaryTree *BinaryTree_NewWithSubtrees(void *userData, BinaryTree *rightSubtree, BinaryTree *leftSubtree);

/**
 * Destroy the binary tree.
 * @param tree  BinaryTree instance.
 */
DENG2_PUBLIC void BinaryTree_Delete(BinaryTree *tree);

DENG2_PUBLIC BinaryTree *BinaryTree_Parent(BinaryTree *tree);

DENG2_PUBLIC int BinaryTree_HasParent(BinaryTree *tree);

DENG2_PUBLIC BinaryTree *BinaryTree_SetParent(BinaryTree *tree, BinaryTree *parent);

/**
 * Given the specified node, return one of it's children.
 *
 * @param tree  BinaryTree instance.
 * @param left  @c true= retrieve the left child.
 *              @c false= retrieve the right child.
 * @return  The identified child if present else @c NULL.
 */
DENG2_PUBLIC BinaryTree *BinaryTree_Child(BinaryTree *tree, int left);

#define BinaryTree_Right(tree) BinaryTree_Child((tree), false)
#define BinaryTree_Left(tree)  BinaryTree_Child((tree), true)

/**
 * Retrieve the user data associated with the specified (sub)tree.
 *
 * @param tree  BinaryTree instance.
 * @return  User data pointer associated with this tree node else @c NULL.
 */
DENG2_PUBLIC void *BinaryTree_UserData(BinaryTree *tree);

/**
 * Set a child of the specified tree.
 *
 * @param tree  BinaryTree instance.
 * @param left  @c true= set the left child.
 *              @c false= set the right child.
 * @param subtree  Ptr to the (child) tree to be linked or @c NULL.
 */
DENG2_PUBLIC BinaryTree *BinaryTree_SetChild(BinaryTree *tree, int left, BinaryTree *subtree);

#define BinaryTree_SetRight(tree, subtree) BinaryTree_SetChild((tree), false, (subtree))
#define BinaryTree_SetLeft(tree, subtree)  BinaryTree_SetChild((tree), true, (subtree))

DENG2_PUBLIC int BinaryTree_HasChild(BinaryTree *tree, int left);

#define BinaryTree_HasRight(tree, subtree) BinaryTree_HasChild((tree), false)
#define BinaryTree_HasLeft(tree, subtree)  BinaryTree_HasChild((tree), true)

/**
 * Set the user data assoicated with the specified (sub)tree.
 *
 * @param tree  BinaryTree instance.
 * @param userData  User data pointer to associate with this tree node.
 */
DENG2_PUBLIC BinaryTree *BinaryTree_SetUserData(BinaryTree *tree, void *userData);

/**
 * Is this node a leaf?
 *
 * @param tree  BinaryTree instance.
 * @return  @c true iff this node is a leaf.
 */
DENG2_PUBLIC int BinaryTree_IsLeaf(BinaryTree *tree);

/**
 * Calculate the height of the given tree.
 *
 * @param tree  BinaryTree instance.
 * @return  Height of the tree.
 */
DENG2_PUBLIC int BinaryTree_Height(BinaryTree *tree);

/**
 * Traverse a binary tree in Preorder.
 *
 * Make a callback for all nodes of the tree (including the root). Traversal
 * continues until all nodes have been visited or a callback returns non-zero at
 * which point traversal is aborted.
 *
 * @param tree  BinaryTree instance.
 * @param callback  Function to call for each object of the tree.
 * @param parameters  Passed to the callback.
 *
 * @return  @c 0= iff all callbacks complete wholly else the return value of the
 * callback last made.
 */
DENG2_PUBLIC int BinaryTree_PreOrder(BinaryTree *tree, int (*callback) (BinaryTree*, void*), void *parameters);

/**
 * Traverse a binary tree in Inorder.
 *
 * Make a callback for all nodes of the tree (including the root). Traversal
 * continues until all nodes have been visited or a callback returns non-zero at
 * which point traversal is aborted.
 *
 * @param tree  BinaryTree instance.
 * @param callback  Function to call for each object of the tree.
 * @param parameters  Passed to the callback.
 *
 * @return  @c 0= iff all callbacks complete wholly else the return value of the
 * callback last made.
 */
DENG2_PUBLIC int BinaryTree_InOrder(BinaryTree *tree, int (*callback) (BinaryTree*, void*), void *parameters);

/**
 * Traverse a binary tree in Postorder.
 *
 * Make a callback for all nodes of the tree (including the root). Traversal
 * continues until all nodes have been visited or a callback returns non-zero at
 * which point traversal is aborted.
 *
 * @param tree  BinaryTree instance.
 * @param callback  Function to call for each object of the tree.
 * @param parameters  Passed to the callback.
 *
 * @return  @c 0= iff all callbacks complete wholly else the return value of the
 * callback last made.
 */
DENG2_PUBLIC int BinaryTree_PostOrder(BinaryTree *tree, int (*callback) (BinaryTree*, void*), void *parameters);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // LIBDENG2_C_WRAPPER_H
