/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_TCPCONNECTION_H
#define OPENDDS_TCPCONNECTION_H

#include "TcpInst.h"
#include "TcpInst_rch.h"
#ifdef __BORLANDC__
#  include "TcpDataLink.h"
#endif
#include "TcpDataLink_rch.h"
#include "TcpConnection_rch.h"
#include "TcpSendStrategy_rch.h"
#include "TcpReceiveStrategy_rch.h"
#include "TcpReconnectTask.h"
#include "dds/DCPS/RcObject_T.h"
#include "ace/SOCK_Stream.h"
#include "ace/Svc_Handler.h"
#include "ace/INET_Addr.h"
#include "ace/Synch.h"

namespace OpenDDS {
namespace DCPS {

class TcpConnection
  : public ACE_Svc_Handler<ACE_SOCK_STREAM, ACE_NULL_SYNCH>,
      public RcObject<ACE_SYNCH_MUTEX> {
public:

  /// States are used during reconnecting.
  enum ReconnectState {
    INIT_STATE,
    LOST_STATE,
    RECONNECTED_STATE,
    PASSIVE_WAITING_STATE,
    PASSIVE_TIMEOUT_CALLED_STATE
  };

  TcpConnection();
  virtual ~TcpConnection();

  std::size_t& id();

  /// Attempt an active connection establishment to the remote address.
  /// The local address is sent to the remote (passive) side to
  /// identify ourselves to the remote side.
  int active_connect(const ACE_INET_Addr& remote_address,
                     const ACE_INET_Addr& local_address,
                     CORBA::Long          priority,
                     TcpInst_rch tcp_config);

  /// This will be called by the DataLink (that "owns" us) when
  /// the TcpTransport has been told to shutdown(), or when
  /// the DataLink finds itself no longer needed, and is
  /// "self-releasing".
  void disconnect();

  // Note that the acceptor that calls the open() method will pass
  // itself in as a void*.
  virtual int open(void* arg);

  void set_receive_strategy(TcpReceiveStrategy* receive_strategy);

  void remove_receive_strategy();

  /// Give a "copy" of the TcpSendStrategy object to this
  /// connection object.
  void set_send_strategy(TcpSendStrategy* send_strategy);

  void remove_send_strategy();

  /// We pass this "event" along to the receive_strategy.
  virtual int handle_input(ACE_HANDLE);

  /// Handle back pressure when sending.
  virtual int handle_output(ACE_HANDLE);

  virtual int close(u_long);
  virtual int handle_close(ACE_HANDLE, ACE_Reactor_Mask);

  void set_sock_options(TcpInst* tcp_config);

  int reconnect(bool on_new_association = false);

  /// Return true if the object represents the connector side, otherwise
  /// it's the acceptor side. The acceptor/connector role is not changed
  /// when re-establishing the connection.
  bool is_connector() const;

  /// Return true if connection is connected.
  bool is_connected() const;

  void transfer(TcpConnection* connection);

  int handle_timeout(const ACE_Time_Value &tv,
                     const void *arg);

  /// Cache the reference to the datalink object for lost connection
  /// callbacks.
  void set_datalink(TcpDataLink* link);

  void notify_lost_on_backpressure_timeout();

  ACE_INET_Addr get_remote_address();

  void  relink(bool do_suspend);

  /// Called by the reconnect task to inform us that the
  /// link (& any associated data can be torn down.
  /// This call is done with no DCPS/transport locks held.
  bool tear_link();

  void shutdown();

  /// Access TRANSPORT_PRIORITY.value policy value if set.
  CORBA::Long& transport_priority();
  CORBA::Long  transport_priority() const;

private:

  /// Attempt an active connection establishment to the remote address.
  /// The local address is sent to the remote (passive) side to
  /// identify ourselves to the remote side.
  /// Note this method is not thread protected. The caller need acquire
  /// the reconnect_lock_ before calling this function.
  int active_establishment(const ACE_INET_Addr& remote_address,
                           const ACE_INET_Addr& local_address,
                           TcpInst_rch tcp_config);

  int active_reconnect_i();
  int passive_reconnect_i();
  int active_reconnect_on_new_association();

  typedef ACE_SYNCH_MUTEX     LockType;
  typedef ACE_Guard<LockType> GuardType;

  /// Lock to avoid the reconnect() called multiple times when
  /// both send() and recv() fail.
  LockType  reconnect_lock_;
  /// Flag indicates if connected or disconneted. It's set to true
  /// when actively connecting or passively acepting succeeds and set
  /// to false whenever the peer stream is closed.
  ACE_Atomic_Op<ACE_SYNCH_MUTEX, bool>  connected_;
  /// Flag indicate this connection object is the connector or acceptor.
  bool      is_connector_;
  /// Reference to the receiving strategy.
  TcpReceiveStrategy_rch receive_strategy_;
  /// Reference to the send strategy.
  TcpSendStrategy_rch send_strategy_;
  /// Remote address.
  ACE_INET_Addr remote_address_;
  /// Local address.
  ACE_INET_Addr local_address_;
  /// The configuration used by this connection.
  TcpInst_rch tcp_config_;
  /// Datalink object which is needed for connection lost callback.
  TcpDataLink_rch      link_;
  /// TODO: This can be removed since we do not need it for checking the
  /// the new connection state during handle_timeout.
  /// The "new" TcpConnection that replaces this "old" object in the
  /// TcpDataLink object. This is needed for checking if the connection is
  /// re-established as the acceptor side when timer goes off.
  TcpConnection_rch    new_con_;

  /// Keep a copy of the old connection object in the new connection object
  /// to help control of the old connection object deletion.
  TcpConnection_rch    old_con_;

  /// The id of the scheduled timer. The timer is scheduled to check if the connection
  /// is re-established during the passive_reconnect_duration_. This id controls
  /// that the timer is just scheduled once when there are multiple threads detect
  /// the lost connection.
  int passive_reconnect_timer_id_;

  /// The task to do the reconnecting.
  /// TODO: We might need reuse the PerConnectionSynch thread
  /// to do the reconnecting or create the reconnect task when
  /// we need reconnect.
  TcpReconnectTask reconnect_task_;

  /// The state indicates each step of the reconnecting.
  ReconnectState reconnect_state_;

  /// Last time the connection is re-established.
  ACE_Time_Value last_reconnect_attempted_;

  /// TRANSPORT_PRIORITY.value policy value.
  CORBA::Long transport_priority_;

  /// shutdown flag
  bool shutdown_;

  /// Small unique identifying value.
  std::size_t id_;
};

} // namespace DCPS
} // namespace OpenDDS

#if defined (__ACE_INLINE__)
#include "TcpConnection.inl"
#endif /* __ACE_INLINE__ */

#endif  /* OPENDDS_TCPCONNECTION_H */
