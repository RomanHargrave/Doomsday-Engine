/*
 * The Doomsday Engine Project -- libcore
 *
 * Copyright © 2004-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBDENG2_SOCKET_H
#define LIBDENG2_SOCKET_H

#include "../libcore.h"
#include "../IByteArray"
#include "../Address"
#include "../Transmitter"

#include <QTcpSocket>
#include <QHostInfo>
#include <QList>
#include <QFlags>

/// Largest message sendable using the protocol.
#define DENG2_SOCKET_MAX_PAYLOAD_SIZE (1 << 22) // 4 MB

namespace de {

class Message;

/**
 * TCP/IP network socket.
 *
 * ListenSocket constructs Socket instances for incoming connections.
 *
 * @ingroup net
 */
class DENG2_PUBLIC Socket : public QObject, public Transmitter
{
    Q_OBJECT

public:
    /// Creating the TCP/IP connection failed. @ingroup errors
    DENG2_ERROR(ConnectionError);

    /// Error subclass for all situations where the socket is left unusable. @ingroup errors
    DENG2_ERROR(BrokenError);

    /// The TCP/IP connection was disconnected. @ingroup errors
    DENG2_SUB_ERROR(BrokenError, DisconnectedError);

    /// Encountered a problem related to the messaging protocol. @ingroup errors
    DENG2_SUB_ERROR(BrokenError, ProtocolError);

    /// There is no peer connected. @ingroup errors
    DENG2_SUB_ERROR(BrokenError, PeerError);

    /// These flags are written to the sent headers.
    enum HeaderFlag {
        Huffman = 0x1,
        Channel1 = 0x2
    };
    Q_DECLARE_FLAGS(HeaderFlags, HeaderFlag)

public:
    Socket();

    /**
     * Opens a socket to @a address and waits (blocks) until the connection has
     * been formed. The socket is ready to be used after the constructor
     * returns. If the connection can be formed within the specified @a timeOut
     * threshold, an exception is thrown.
     *
     * @param address  Address to connect to.
     * @param timeOut  Maximum time to wait for connection.
     */
    Socket(Address const &address, TimeDelta const &timeOut);

    virtual ~Socket();

    /**
     * Opens a connection to @a address and returns immediately. The
     * connected() signal is emitted when the connection is ready to use.
     *
     * @param address  Address to connect to.
     */
    void connect(Address const &address);

    /**
     * Opens a connection to a host and returns immediately. If the IP address
     * is not known, this must be called instead of connect() as Address does
     * not work with domain names.
     *
     * The connected() signal is emitted when the connection is ready to use. A
     * disconnected() signal is emitted if the domain name lookup fails.
     *
     * @param domainNameWithOptionalPort  Domain name or IP address, with
     * optional port appended (e.g., "myhost.com:13209").
     * @param defaultPort  Port number to use if not specified in the first argument.
     */
    void connectToDomain(String const &domainNameWithOptionalPort, duint16 defaultPort = 0);

    /**
     * Returns the currently active channel.
     */
    duint channel() const;

    /**
     * Sets the active channel.
     *
     * @param number  Channel number.
     */
    void setChannel(duint number);

    // Implements Transmitter.
    /**
     * Sends the given data over the socket.  Copies the data into
     * a temporary buffer before sending. The data is sent on the current
     * sending channel.
     *
     * @param packet  Data to send.
     */
    void send(IByteArray const &packet);

    /**
     * Sends the given data over the socket.  Copies the data into
     * a temporary buffer before sending. The data is sent on the current
     * sending channel.
     *
     * @param data  Data to send.
     *
     * @return  Reference to this socket.
     */
    Socket &operator << (IByteArray const &data);

    /**
     * Returns the next received message. If nothing has been received,
     * returns @c NULL.
     *
     * @return  Received bytes. Caller gets ownership of the message.
     */
    Message *receive();

    /**
     * Returns a pointer to the next received message, if one is available.
     *
     * @return  Message. Socket retains ownership.
     */
    Message *peek();

    /**
     * Determines the IP address and port of the remote end of a connected socket.
     *
     * @return  Address of the peer.
     */
    Address peerAddress() const;

    /**
     * Determines if the socket is open for communications.
     */
    bool isOpen() const;

    /**
     * Determines if the socket is on the local computer.
     */
    bool isLocal() const;

    /**
     * Determines whether there are any incoming messages waiting.
     */
    bool hasIncoming() const;

    /**
     * Determines the amount of data waiting to be sent out.
     */
    dsize bytesBuffered() const;

    /**
     * Blocks until all outgoing data has been written to the socket.
     */
    void flush();

    /**
     * Close the socket.
     */
    void close();

    /**
     * Allows or disallows the socket from outputting log output.
     *
     * @param noLogOutput  @c true to be quiet, @c false to allow output.
     */
    void setQuiet(bool noLogOutput);

signals:
    void addressResolved();
    void connected();
    void messagesReady();
    void disconnected();
    void error(QString errorMessage);

public slots:
    void socketDisconnected();
    void socketError(QAbstractSocket::SocketError socketError);
    void readIncomingBytes();
    void reconnect();

private slots:
    void hostResolved(QHostInfo const &);
    void bytesWereWritten(qint64 bytes);
    void socketDestroyed();

protected:
    /// Create a Socket object for a previously opened socket.
    Socket(QTcpSocket *existingSocket);

    void initialize();

    /**
     * Receives a specific number of bytes from the socket. Blocks
     * until all the data has been read correctly. An exception is
     * thrown if the connection fails.
     */
    void receiveBytes(duint count, dbyte *buffer);

    void send(IByteArray const &packet, duint channel);

private:
    DENG2_PRIVATE(d)

    /**
     * ListenSocket creates instances of Socket so it needs to use
     * the special private constructor that takes an existing
     * socket data pointer as a parameter.
     */
    friend class ListenSocket;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Socket::HeaderFlags)

} // namespace de

#endif // LIBDENG2_SOCKET_H
