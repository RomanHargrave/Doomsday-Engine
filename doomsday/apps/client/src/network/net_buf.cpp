/** @file
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2006-2013 Daniel Swanson <danij@dengine.net>
 * @authors Copyright © 2006 Jamie Jones <jamie_jones_au@yahoo.com.au>
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

/**
 * Network Message Handling and Buffering
 */

#include "de_platform.h"
#include "de_system.h"
#include "de_network.h"
#include "de_console.h"
#include "de_misc.h"

#include <de/memory.h>
#include <de/c_wrapper.h>
#include <de/ByteRefArray>

#define MSG_MUTEX_NAME  "MsgQueueMutex"

dd_bool allowSending;
netbuffer_t netBuffer;

// The message queue: list of incoming messages waiting for processing.
static netmessage_t *msgHead, *msgTail;
static int entryCount;

// A mutex is used to protect the addition and removal of messages from
// the message queue.
static mutex_t msgMutex;

// Number of bytes of outgoing data transmitted.
static size_t numOutBytes;

// Number of bytes sent over the network (compressed).
static size_t numSentBytes;

Reader* Reader_NewWithNetworkBuffer(void)
{
    return Reader_NewWithBuffer((const byte*) netBuffer.msg.data, netBuffer.length);
}

/**
 * Initialize the low-level network subsystem. This is called always
 * during startup (via Sys_Init()).
 */
void N_Init(void)
{
    // Create a mutex for the message queue.
    msgMutex = Sys_CreateMutex(MSG_MUTEX_NAME);

    allowSending = false;

    //N_SockInit();
    N_MasterInit();
}

/**
 * Shut down the low-level network interface. Called during engine
 * shutdown (not before).
 */
void N_Shutdown(void)
{
    // Any queued messages will be destroyed.
    N_ClearMessages();

    N_MasterShutdown();

    allowSending = false;

    // Close the handle of the message queue mutex.
    Sys_DestroyMutex(msgMutex);
    msgMutex = 0;
}

/**
 * Acquire or release ownership of the message queue mutex.
 *
 * @return          @c true, if successful.
 */
dd_bool N_LockQueue(dd_bool doAcquire)
{
    if(doAcquire)
        Sys_Lock(msgMutex);
    else
        Sys_Unlock(msgMutex);
    return true;
}

/**
 * Adds the given netmessage_s to the queue of received messages.
 * We use a mutex to synchronize access to the message queue.
 *
 * @note This is called in the network receiver thread.
 */
void N_PostMessage(netmessage_t *msg)
{
    N_LockQueue(true);

    // This will be the latest message.
    msg->next = NULL;

    // Set the timestamp for reception.
    msg->receivedAt = Timer_RealSeconds();

    if(msgTail)
    {
        // There are previous messages.
        msgTail->next = msg;
    }

    // The tail pointer points to the last message.
    msgTail = msg;

    // If there is no head, this'll be the first message.
    if(msgHead == NULL)
        msgHead = msg;

    // One new message available.
    entryCount++;

    N_LockQueue(false);
}

/**
 * Extracts the next message from the queue of received messages.
 * The caller must release the message when it's no longer needed,
 * using N_ReleaseMessage().
 *
 * We use a mutex to synchronize access to the message queue. This is
 * called in the Doomsday thread.
 *
 * @return              @c NULL, if no message is found;
 */
netmessage_t *N_GetMessage(void)
{
    // This is the message we'll return.
    netmessage_t *msg = NULL;

    N_LockQueue(true);
    if(msgHead != NULL)
    {
        msg = msgHead;

        // Check for simulated latency.
        if(netSimulatedLatencySeconds > 0 &&
           (Timer_RealSeconds() - msg->receivedAt < netSimulatedLatencySeconds))
        {
            // This message has not been received yet.
            msg = NULL;
        }
        else
        {
            // If there are no more messages, the tail pointer must be
            // cleared, too.
            if(!msgHead->next)
                msgTail = NULL;

            // Advance the head pointer.
            msgHead = msgHead->next;

            if(msg)
            {
                // One less message available.
                entryCount--;
            }
        }
    }
    N_LockQueue(false);

    // Identify the sender.
    if(msg)
    {
        msg->player = N_IdentifyPlayer(msg->sender);
    }
    return msg;
}

/**
 * Frees the message.
 */
void N_ReleaseMessage(netmessage_t *msg)
{
    if(msg->handle)
    {
        delete [] reinterpret_cast<byte *>(msg->handle);
        msg->handle = 0;
    }
    M_Free(msg);
}

/**
 * Empties the message buffers.
 */
void N_ClearMessages(void)
{
    if(!msgMutex) return; // Not initialized yet.

    netmessage_t *msg;
    float oldSim = netSimulatedLatencySeconds;

    // No simulated latency now.
    netSimulatedLatencySeconds = 0;

    while((msg = N_GetMessage()) != NULL)
        N_ReleaseMessage(msg);

    netSimulatedLatencySeconds = oldSim;

    // The queue is now empty.
    msgHead = msgTail = NULL;
    entryCount = 0;
}

/**
 * Send the data in the netbuffer. The message is sent using an
 * unreliable, nonsequential (i.e. fast) method.
 *
 * Handles broadcasts using recursion.
 * Clients can only send stuff to the server.
 */
void N_SendPacket(int flags)
{
#ifdef __SERVER__
    uint dest = 0;
#else
    DENG2_UNUSED(flags);
#endif

    // Is the network available?
    if(!allowSending)
        return;

    // Figure out the destination DPNID.
#ifdef __SERVER__
    {
        if(netBuffer.player >= 0 && netBuffer.player < DDMAXPLAYERS)
        {
            if(/*(ddpl->flags & DDPF_LOCAL) ||*/
               !clients[netBuffer.player].connected)
            {
                // Do not send anything to local or disconnected players.
                return;
            }

            dest = clients[netBuffer.player].nodeID;
        }
        else
        {
            // Broadcast to all non-local players, using recursive calls.
            for(int i = 0; i < DDMAXPLAYERS; ++i)
            {
                netBuffer.player = i;
                N_SendPacket(flags);
            }

            // Reset back to -1 to notify of the broadcast.
            netBuffer.player = NSP_BROADCAST;
            return;
        }
    }
#endif

    // This is what will be sent.
    numOutBytes += netBuffer.headerLength + netBuffer.length;

    try
    {
#ifdef __CLIENT__
        de::Transmitter &out = Net_ServerLink();
#else
        de::Transmitter &out = App_ServerSystem().user(dest);
#endif

        out << de::ByteRefArray(&netBuffer.msg, netBuffer.headerLength + netBuffer.length);
    }
    catch(de::Error const &er)
    {
        LOGDEV_NET_WARNING("N_SendPacket failed: ") << er.asText();
    }
}

void N_AddSentBytes(size_t bytes)
{
    numSentBytes += bytes;
}

/**
 * @return The player number that corresponds network node @a id.
 */
int N_IdentifyPlayer(nodeid_t id)
{
#ifdef __SERVER__
    // What is the corresponding player number? Only the server keeps
    // a list of all the IDs.
    for(int i = 0; i < DDMAXPLAYERS; ++i)
    {
        if(clients[i].nodeID == id)
            return i;
    }
    return -1;
#else
    DENG2_UNUSED(id);
#endif

    // Clients receive messages only from the server.
    return 0;
}

/**
 * Retrieves the next incoming message.
 *
 * @return  The next message waiting in the incoming message queue.
 *          When the message is no longer needed you must call
 *          N_ReleaseMessage() to delete it.
 */
netmessage_t *N_GetNextMessage(void)
{
    netmessage_t       *msg;

    while((msg = N_GetMessage()) != NULL)
    {
        return msg;
    }
    return NULL; // There are no more messages.
}

/**
 * An attempt is made to extract a message from the message queue.
 *
 * @return          @c true, if a message successfull.
 */
dd_bool N_GetPacket(void)
{
    netmessage_t *msg;

    // If there are net events pending, let's not return any packets
    // yet. The net events may need to be processed before the
    // packets.
    if(N_NEPending())
        return false;

    netBuffer.player = -1;
    netBuffer.length = 0;

    if((msg = N_GetNextMessage()) == NULL)
    {
        // No messages at this time.
        return false;
    }

    // There was a packet!
    netBuffer.player = msg->player;
    netBuffer.length = msg->size - netBuffer.headerLength;

    if(sizeof(netBuffer.msg) >= msg->size)
    {
        memcpy(&netBuffer.msg, msg->data, msg->size);
    }
    else
    {
        LOG_NET_ERROR("Received an oversized packet with %i bytes") << msg->size;

        N_ReleaseMessage(msg);
        return false;
    }

    // The message can now be freed.
    N_ReleaseMessage(msg);

    // We have no idea who sent this (on serverside).
    if(netBuffer.player == -1)
        return false;

    return true;
}

/**
 * Print low-level information about the network buffer.
 */
void N_PrintBufferInfo(void)
{
    N_PrintTransmissionStats();
}

/**
 * Print status information about the workings of data compression
 * in the network buffer.
 *
 * @note  Currently numOutBytes excludes transmission header, while
 *        numSentBytes includes every byte written to the socket.
 *        In other words, the efficiency includes protocol overhead.
 */
void N_PrintTransmissionStats(void)
{
    if(numOutBytes == 0)
    {
        LOG_NET_MSG("Transmission efficiency: Nothing has been sent yet");
    }
    else
    {
        LOG_NET_MSG("Transmission efficiency: %.3f%% (data: %i bytes, sent: %i "
                    "bytes)") << (100 - (100.0f * numSentBytes) / numOutBytes)
                                 << numOutBytes << numSentBytes;
    }
}
