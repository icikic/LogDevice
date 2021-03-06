/**
 * Copyright (c) 2017-present, Facebook, Inc. and its affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#pragma once

#include <folly/futures/Future.h>

#include "logdevice/common/Address.h"
#include "logdevice/common/ClientID.h"
#include "logdevice/common/Socket.h"

namespace facebook { namespace logdevice {

class SocketAdapter;

/**
 * this will we a wrapper around our socket which knows about protocol and
 * serialization
 */
class Connection : public Socket_DEPRECATED {
 public:
  /**
   * Constructs a new Connection, to be connected to a LogDevice
   * server. The calling thread must be a Worker thread.
   *
   * @param server_name     id of server to connect to
   * @param socket_type     type of socket
   * @param connection_type type of connection
   * @param peer_type       type of peer
   * @param flow_group      traffic shaping state shared between sockets
   *                        with the same bandwidth constraints.
   * @params deps           SocketDependencies provides a way to callback into
   *                        higher layers and provides notification mechanism.
   *                        It depends on dependencies for stuff like Stats and
   *                        config and other data.
   *
   * @return  on success, a new fully constructed Connection is returned. It is
   *          expected that the Connection will be registered with the Worker's
   *          Sender under server_name. On failure throws ConstructorFailed and
   *          sets err to:
   *
   *     INVALID_THREAD  current thread is not running a Worker (debug build
   *                     asserts)
   *     NOTINCONFIG     server_name does not appear in cluster config
   *     INTERNAL        failed to initialize a libevent timer (unlikely)
   */
  Connection(NodeID server_name,
             SocketType socket_type,
             ConnectionType connection_type,
             PeerType peer_type,
             FlowGroup& flow_group,
             std::unique_ptr<SocketDependencies> deps);

  Connection(NodeID server_name,
             SocketType socket_type,
             ConnectionType connection_type,
             PeerType peer_type,
             FlowGroup& flow_group,
             std::unique_ptr<SocketDependencies> deps,
             std::unique_ptr<SocketAdapter> sock_adapter);

  /**
   * Constructs a new Connection from a TCP socket fd that was returned by
   * accept(). The thread must run a Worker. On success the socket is emplaced
   * on this Sender's .client_sockets_ map with client_name as key.
   *
   * @param fd        fd of the accepted socket. The caller passes
   *                  responsibility for closing fd to the constructor.
   * @param client_name local identifier assigned to this passively accepted
   *                    connection (aka "client address")
   * @param client_addr sockaddr we got from accept() for this client connection
   * @param conn_token  used to keep track of all accepted connections
   * @param type        type of socket
   * @param flow_group  traffic shaping state shared between sockets
   *                    with the same bandwidth constraints.
   * @params deps       SocketDependencies provides a way to callback into
   *                    higher layers and provides notification mechanism.
   *                    It depends on dependencies for stuff like Stats and
   *                    config and other data.
   *
   * @return  on success, a new fully constructed Connection is returned. On
   *          failure throws ConstructorFailed and sets err to:
   *
   *     INVALID_THREAD  current thread is not running a Worker (debug build
   *                     asserts)
   *     NOMEM           a libevent function could not allocate memory
   *     INTERNAL        failed to set fd non-blocking (unlikely) or failed to
   *                     initialize a libevent timer (unlikely).
   */
  Connection(int fd,
             ClientID client_name,
             const Sockaddr& client_addr,
             ResourceBudget::Token conn_token,
             SocketType type,
             ConnectionType conntype,
             FlowGroup& flow_group,
             std::unique_ptr<SocketDependencies> deps);

  Connection(int fd,
             ClientID client_name,
             const Sockaddr& client_addr,
             ResourceBudget::Token conn_token,
             SocketType type,
             ConnectionType conntype,
             FlowGroup& flow_group,
             std::unique_ptr<SocketDependencies> deps,
             std::unique_ptr<SocketAdapter> sock_adapter);

  /**
   * Disconnects, deletes the underlying bufferevent, and closes the TCP socket.
   */
  ~Connection() override;

  Connection(const Connection&) = delete;
  Connection(Connection&&) = delete;
  Connection& operator=(const Connection&) = delete;
  Connection& operator=(Connection&&) = delete;

  Socket_DEPRECATED::SendStatus
  sendBuffer(std::unique_ptr<folly::IOBuf>&& buffer_chain) override;

  void close(Status reason) override;

  void onBytesPassedToTCP(size_t nbytes) override;

  int dispatchMessageBody(ProtocolHeader header,
                          std::unique_ptr<folly::IOBuf> msg_buffer) override;

  bool msgRetryTimerArmed() {
    return retry_receipt_of_message_.isScheduled();
  }

 protected:
  void scheduleWriteChain();

  void drainSendQueue();
};
}} // namespace facebook::logdevice
