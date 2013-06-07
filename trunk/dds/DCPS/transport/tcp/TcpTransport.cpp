/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "Tcp_pch.h"
#include "TcpTransport.h"
#include "TcpConnectionReplaceTask.h"
#include "TcpAcceptor.h"
#include "TcpSendStrategy.h"
#include "TcpReceiveStrategy.h"
#include "TcpInst.h"
#include "TcpDataLink.h"
#include "TcpSynchResource.h"
#include "TcpConnection.h"
#include "dds/DCPS/transport/framework/NetworkAddress.h"
#include "dds/DCPS/transport/framework/TransportReactorTask.h"
#include "dds/DCPS/transport/framework/EntryExit.h"
#include "dds/DCPS/transport/framework/TransportExceptions.h"
#include "dds/DCPS/AssociationData.h"
#include "dds/DCPS/debug.h"
#include <sstream>

namespace OpenDDS {
namespace DCPS {

TcpTransport::TcpTransport(const TransportInst_rch& inst)
  : acceptor_(new TcpAcceptor(this)),
    con_checker_(new TcpConnectionReplaceTask(this))
{
  DBG_ENTRY_LVL("TcpTransport","TcpTransport",6);
  if (!inst.is_nil()) {
    if (!configure(inst.in())) {
      delete con_checker_;
      delete acceptor_;
      throw Transport::UnableToCreate();
    }
  }
}

TcpTransport::~TcpTransport()
{
  DBG_ENTRY_LVL("TcpTransport","~TcpTransport",6);
  delete acceptor_;

  con_checker_->close(1);  // This could potentially fix a race condition
  delete con_checker_;
}

PriorityKey
TcpTransport::blob_to_key(const TransportBLOB& remote,
                          CORBA::Long priority,
                          bool active)
{
  ACE_INET_Addr remote_address = AssociationData::get_remote_address(remote);

  const bool is_loopback = remote_address == this->tcp_config_->local_address_;

  return PriorityKey(priority, remote_address, is_loopback, active);
}

DataLink*
TcpTransport::find_datalink_i(const RepoId& /*local_id*/,
                              const RepoId& /*remote_id*/,
                              const TransportBLOB& remote_data,
                              bool /*remote_reliable*/,
                              bool /*remote_durable*/,
                              const ConnectionAttribs& attribs,
                              bool active)
{
  DBG_ENTRY_LVL("TcpTransport", "find_datalink_i", 6);

  const PriorityKey key =
    this->blob_to_key(remote_data, attribs.priority_, active);

  VDBG_LVL((LM_DEBUG,
            ACE_TEXT("(%P|%t) TcpTransport::find_datalink_i ")
            ACE_TEXT("remote_address \"%C:%d priority %d ")
            ACE_TEXT("is_loopback %d\"\n"),
            key.address().get_host_name(),
            key.address().get_port_number(),
            attribs.priority_, key.is_loopback()),
          2);

  TcpDataLink_rch link;
  GuardType guard(this->links_lock_);

  // First, we have to try to find an existing (connected) DataLink
  // that suits the caller's needs.

  if (this->links_.find(key, link) == 0) {

    VDBG_LVL((LM_DEBUG,
              ACE_TEXT("(%P|%t) TcpTransport::find_datalink_i ")
              ACE_TEXT("link found, waiting for start.\n")), 2);

    {
      ACE_Reverse_Lock<LockType> rev(this->links_lock_);
      ACE_GUARD_RETURN(ACE_Reverse_Lock<LockType>, rev_guard, rev, 0);
      link->wait_for_start();
    }

    TcpConnection_rch con = link->get_connection();

    if (con.is_nil()) {
      ACE_ERROR_RETURN((LM_ERROR,
                        "(%P|%t) ERROR: Unable to use found datalink for "
                        "remote %C:%d.\n",
                        key.address().get_host_addr(),
                        key.address().get_port_number()),
                       0);
    }

    VDBG_LVL((LM_DEBUG,
              ACE_TEXT("(%P|%t) TcpTransport::find_datalink_i ")
              ACE_TEXT("done waiting for start.\n")), 2);

    if (con->is_connector() && !con->is_connected()) {
      bool on_new_association = true;

      if (con->reconnect(on_new_association) == -1) {
        ACE_ERROR_RETURN((LM_ERROR,
                          "(%P|%t) ERROR: Unable to reconnect to remote %C:%d.\n",
                          key.address().get_host_addr(),
                          key.address().get_port_number()),
                         0);
      }
    }

    // else, This means we may or may not find a suitable (and already connected) DataLink.
    // if con is not a connector and not connected, the passive connecting side will wait
    // for the connection establishment.

    if (DCPS_debug_level >= 5) {
      ACE_DEBUG((LM_DEBUG, "(%P|%t) Found existing connection,"
        " No need for passive connection establishment, transport: %C.\n",
        this->config()->name().c_str()));
    }
    return link._retn();

  } else if (this->pending_release_links_.find(key, link) == 0) {
    if (link->cancel_release()) {
      link->set_release_pending(false);
      if (this->pending_release_links_.unbind(key, link) == 0 && this->links_.bind(key, link) == 0) {
        VDBG_LVL((LM_DEBUG, "(%P|%t) Move link prio=%d addr=%C:%d to links_\n",
                  link->transport_priority(), link->remote_address().get_host_name(),
                  link->remote_address().get_port_number()), 5);
        return link._retn();

      } else {
        // This should not happen.
        ACE_ERROR((LM_ERROR, "(%P|%t) Failed to move link prio=%d addr=%C:%d to links_\n",
                   link->transport_priority(), link->remote_address().get_host_name(),
                   link->remote_address().get_port_number()));
      }
    }
  }

  VDBG_LVL((LM_DEBUG,
            ACE_TEXT("(%P|%t) TcpTransport::find_datalink_i ")
            ACE_TEXT("no datalink found.\n")), 2);

  return 0;
}

DataLink*
TcpTransport::connect_datalink_i(const RepoId& /*local_id*/,
                                 const RepoId& /*remote_id*/,
                                 const TransportBLOB& remote_data,
                                 bool /*remote_reliable*/,
                                 bool /*remote_durable*/,
                                 const ConnectionAttribs& attribs)
{
  DBG_ENTRY_LVL("TcpTransport", "connect_datalink_i", 6);

  const PriorityKey key =
    this->blob_to_key(remote_data, attribs.priority_, true /*active*/);

  TcpDataLink_rch link =
    new TcpDataLink(key.address(), this, attribs.priority_,
                    key.is_loopback(), true /*active*/);

  VDBG_LVL((LM_DEBUG,
            "(%P|%t) TcpTransport::connect_datalink_i link %@ PriorityKey "
            "prio=%d, addr=%C:%hu, is_loopback=%d, is_active=%d\n",
            link.in(), link->transport_priority(),
            link->remote_address().get_host_addr(),
            link->remote_address().get_port_number(),
            (int)link->is_loopback(),
            (int)link->is_active()), 2);

  { // guard scope
    GuardType guard(this->links_lock_);

    // Attempt to bind the TcpDataLink to our links_ map.
    if (this->links_.bind(key, link) != 0) {
      // We failed to bind the new DataLink into our links_ map.
      // On error, we return a NULL pointer.
      ACE_ERROR_RETURN((LM_ERROR,
                        "(%P|%t) ERROR: TcpTransport::connect_datalink_i "
                        "Unable to bind new TcpDataLink to "
                        "TcpTransport in links_ map.\n"), 0);
    }
  }

  // Now we need to attempt to establish a connection for the DataLink.
  int result = this->make_active_connection(key.address(), link);

  if (result != 0) {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) ERROR: Failed to make active connection.\n"));

    link->unblock_wait_for_start();

    GuardType guard(this->links_lock_);
    // Make sure that we unbind the link (that failed to establish a
    // connection) from our links_ map.  We intentionally ignore the
    // return code from the unbind() call since we know that we just
    // did the bind() moments ago - and with the links_lock_ acquired
    // the whole time.
    this->links_.unbind(key);

    // On error, return a NULL pointer.
    return 0;
  }

  // That worked.  Return a reference to the DataLink that the caller will
  // be responsible for.
  return link._retn();
}

DataLink*
TcpTransport::accept_datalink(TransportImpl::ConnectionEvent& ce)
{
  const std::string ttype = "tcp";
  const CORBA::ULong num_blobs = ce.remote_association_.remote_data_.length();

  GuardType guard(this->connections_lock_);
  std::vector<PriorityKey> keys;
  TcpDataLink_rch ready_link; // a datalink that's already ready to use
  PriorityKey ready_key; // key for ready_link
  TcpConnection_rch connection;

  for (CORBA::ULong idx = 0; idx < num_blobs; ++idx) {
    if (ce.remote_association_.remote_data_[idx].transport_type.in() == ttype) {

      const PriorityKey key =
        this->blob_to_key(ce.remote_association_.remote_data_[idx].data,
                          ce.attribs_.priority_, false /*active == false*/);

      TcpDataLink_rch link =
        new TcpDataLink(key.address(), this, ce.attribs_.priority_,
                        key.is_loopback(), false /*active == false*/);
      VDBG_LVL((LM_DEBUG,
                "(%P|%t) TcpTransport::accept_datalink link %@ PriorityKey"
                "prio=%d, addr=%C:%hu, is_loopback=%d, is_active=%d\n",
                link.in(), link->transport_priority(),
                link->remote_address().get_host_addr(),
                link->remote_address().get_port_number(),
                (int)link->is_loopback(),
                (int)link->is_active()), 2);

      {
        GuardType guard(this->links_lock_);
        if (this->links_.bind(key, link) != 0) {
          this->unbind_all(keys, &guard);
          ACE_ERROR_RETURN((LM_ERROR,
                            "(%P|%t) ERROR: TcpTransport::accept_datalink "
                            "Unable to bind new TcpDataLink to "
                            "TcpTransport in links_ map.\n"), 0);
        }
      }

      ConnectionMap::iterator iter = this->connections_.find(key);
      if (iter != this->connections_.end()) {
        connection = iter->second;
        this->connections_.erase(iter);
        ready_link = link;
        ready_key = key;
        break;
      }
      keys.push_back(key);
    }
  }

  if (ready_link.is_nil()) {
    for (size_t i = 0; i < keys.size(); ++i) {
      this->pending_connections_.insert(
        std::pair<ConnectionEvent* const, PriorityKey>(&ce, keys[i]));
      // SunCC doesn't like using make_pair() here.
    }

    VDBG_LVL((LM_DEBUG,
              ACE_TEXT("(%P|%t) TcpTransport::accept_datalink ")
              ACE_TEXT("no existing connection.\n")), 2);
    return 0; // no link ready, passive_connection will signal the event later
  }

  guard.release(); // connect_tcp_datalink() isn't called with connections_lock_
  if (this->connect_tcp_datalink(ready_link, connection) == -1) {
    GuardType guard(this->links_lock_);
    this->links_.unbind(ready_key);
    ready_link = 0;
  }
  this->unbind_all(keys); // these are the blobs that didn't connect
  return ready_link._retn();
}

void
TcpTransport::unbind_all(const std::vector<PriorityKey>& keys)
{
  GuardType guard(this->links_lock_);
  this->unbind_all(keys, &guard);
}

void
TcpTransport::unbind_all(const std::vector<PriorityKey>& keys, GuardType*)
{
  for (size_t i = 0; i < keys.size(); ++i) {
    this->links_.unbind(keys[i]);
  }
}

void
TcpTransport::stop_accepting(TransportImpl::ConnectionEvent& ce)
{
  GuardType guard(this->connections_lock_);
  typedef std::multimap<ConnectionEvent*, PriorityKey>::iterator iter_t;
  std::pair<iter_t, iter_t> range = this->pending_connections_.equal_range(&ce);
  for (iter_t iter = range.first; iter != range.second; ++iter) {
    GuardType guard(this->links_lock_);
    this->links_.unbind(iter->second);
  }
  this->pending_connections_.erase(range.first, range.second);
}

bool
TcpTransport::configure_i(TransportInst* config)
{
  DBG_ENTRY_LVL("TcpTransport", "configure_i", 6);

  // Downcast the config argument to a TcpInst*
  TcpInst* tcp_config =
    static_cast<TcpInst*>(config);

  if (tcp_config == 0) {
    // The downcast failed.
    ACE_ERROR_RETURN((LM_ERROR,
                      "(%P|%t) ERROR: Failed downcast from TransportInst "
                      "to TcpInst.\n"),
                     false);
  }

  this->create_reactor_task();

  // Ask our base class for a "copy" of the reference to the reactor task.
  this->reactor_task_ = reactor_task();

  // Make a "copy" of the reference for ourselves.
  tcp_config->_add_ref();
  this->tcp_config_ = tcp_config;

  // Open the reconnect task
  if (this->con_checker_->open()) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: connection checker failed to open : %p\n"),
                      ACE_TEXT("open")),
                     false);
  }

  // Open our acceptor object so that we can accept passive connections
  // on our this->tcp_config_->local_address_.

  if (this->acceptor_->open(this->tcp_config_->local_address_,
                            this->reactor_task_->get_reactor()) != 0) {
    // Remember to drop our reference to the tcp_config_ object since
    // we are about to return -1 here, which means we are supposed to
    // keep a copy after all.
    TcpInst_rch cfg = this->tcp_config_._retn();

    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: Acceptor failed to open %C:%d: %p\n"),
                      cfg->local_address_.get_host_addr(),
                      cfg->local_address_.get_port_number(),
                      ACE_TEXT("open")),
                     false);
  }

  // update the port number (incase port zero was given).
  ACE_INET_Addr address;

  if (this->acceptor_->acceptor().get_local_addr(address) != 0) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: TcpTransport::configure_i ")
               ACE_TEXT("- %p"),
               ACE_TEXT("cannot get local addr\n")));
  }

  VDBG_LVL((LM_DEBUG,
            ACE_TEXT("(%P|%t) TcpTransport::configure_i listening on %C:%hu\n"),
            address.get_host_name(), address.get_port_number()), 2);

  unsigned short port = address.get_port_number();
  std::stringstream out;
  out << port;

  // As default, the acceptor will be listening on INADDR_ANY but advertise with the fully
  // qualified hostname and actual listening port number.
  if (tcp_config_->local_address_.is_any()) {
    std::string hostname = get_fully_qualified_hostname();

    this->tcp_config_->local_address_.set(port, hostname.c_str());
    this->tcp_config_->local_address_str_ = hostname;
    this->tcp_config_->local_address_str_ += ':' + out.str();
  }

  // Now we got the actual listening port. Update the port nnmber in the configuration
  // if it's 0 originally.
  else if (tcp_config_->local_address_.get_port_number() == 0) {
    this->tcp_config_->local_address_.set_port_number(port);

    if (this->tcp_config_->local_address_str_.length() > 0) {
      size_t pos = this->tcp_config_->local_address_str_.find(':');
      std::string str = this->tcp_config_->local_address_str_.substr(0, pos + 1);
      str += out.str();
      this->tcp_config_->local_address_str_ = str;
    }
  }

  // Ahhh...  The sweet smell of success!
  return true;
}

void
TcpTransport::pre_shutdown_i()
{
  DBG_ENTRY_LVL("TcpTransport","pre_shutdown_i",6);

  GuardType guard(this->links_lock_);

  AddrLinkMap::ENTRY* entry;

  for (AddrLinkMap::ITERATOR itr(this->links_);
       itr.next(entry);
       itr.advance()) {
    entry->int_id_->pre_stop_i();
  }
}

void
TcpTransport::shutdown_i()
{
  DBG_ENTRY_LVL("TcpTransport","shutdown_i",6);

  // Don't accept any more connections.
  this->acceptor_->close();
  this->acceptor_->transport_shutdown();

  this->con_checker_->close(1);

  {
    GuardType guard(this->connections_lock_);

    this->connections_.clear();
  }

  // Disconnect all of our DataLinks, and clear our links_ collection.
  {
    GuardType guard(this->links_lock_);

    AddrLinkMap::ENTRY* entry;

    for (AddrLinkMap::ITERATOR itr(this->links_);
         itr.next(entry);
         itr.advance()) {
      entry->int_id_->transport_shutdown();
    }

    this->links_.unbind_all();

    for (AddrLinkMap::ITERATOR itr(this->pending_release_links_);
         itr.next(entry);
         itr.advance()) {
      entry->int_id_->transport_shutdown();
    }

    this->pending_release_links_.unbind_all();
  }

  // Drop our reference to the TcpInst object.
  this->tcp_config_ = 0;

  // Drop our reference to the TransportReactorTask
  this->reactor_task_ = 0;

  // Tell our acceptor about this event so that it can drop its reference
  // it holds to this TcpTransport object (via smart-pointer).
  this->acceptor_->transport_shutdown();
}

bool
TcpTransport::connection_info_i(TransportLocator& local_info) const
{
  DBG_ENTRY_LVL("TcpTransport", "connection_info_i", 6);

  VDBG_LVL((LM_DEBUG, "(%P|%t) TcpTransport local address str %C\n",
            this->tcp_config_->local_address_str_.c_str()), 2);

  //Always use local address string to provide to DCPSInfoRepo for advertisement.
  NetworkAddress network_order_address(this->tcp_config_->local_address_str_);

  ACE_OutputCDR cdr;
  cdr << network_order_address;
  const CORBA::ULong len = static_cast<CORBA::ULong>(cdr.total_length());
  char* buffer = const_cast<char*>(cdr.buffer()); // safe

  local_info.transport_type = "tcp";
  local_info.data = TransportBLOB(len, len,
                                  reinterpret_cast<CORBA::Octet*>(buffer));
  return true;
}

void
TcpTransport::release_datalink(DataLink* link)
{
  DBG_ENTRY_LVL("TcpTransport", "release_datalink", 6);

  TcpDataLink* tcp_link = static_cast<TcpDataLink*>(link);

  if (tcp_link == 0) {
    // Really an assertion failure
    ACE_ERROR((LM_ERROR,
               "(%P|%t) INTERNAL ERROR - Failed to downcast DataLink to "
               "TcpDataLink.\n"));
    return;
  }

  TcpDataLink_rch released_link;

  // Possible actions that will be taken to release the link.
  enum LinkAction { None, StopLink, ScheduleLinkRelease };
  LinkAction linkAction = None;

  { // Scope for locking to protect the links (and pending_release) containers.
    GuardType guard(this->links_lock_);

    // Attempt to remove the TcpDataLink from our links_ map.
    PriorityKey key(
      tcp_link->transport_priority(),
      tcp_link->remote_address(),
      tcp_link->is_loopback(),
      tcp_link->is_active());

    VDBG_LVL((LM_DEBUG,
              "(%P|%t) TcpTransport::release_datalink link %@ PriorityKey "
              "prio=%d, addr=%C:%hu, is_loopback=%d, is_active=%d\n",
              link,
              tcp_link->transport_priority(),
              tcp_link->remote_address().get_host_addr(),
              tcp_link->remote_address().get_port_number(),
              (int)tcp_link->is_loopback(),
              (int)tcp_link->is_active()), 2);

    if (this->links_.unbind(key, released_link) != 0) {
      ACE_ERROR((LM_ERROR,
                 "(%P|%t) ERROR: Unable to locate DataLink in order to "
                 "release and it.\n"));

    } else if (link->datalink_release_delay() > ACE_Time_Value::zero) {

      VDBG_LVL((LM_DEBUG,
                "(%P|%t) TcpTransport::release_datalink datalink_release_delay "
                "is %: sec %d usec\n",
                link->datalink_release_delay().sec(),
                link->datalink_release_delay().usec()), 4);

      // Atomic value update, safe to perform here.
      released_link->set_release_pending(true);

      switch (this->pending_release_links_.bind(key, released_link)) {
      case -1:
        ACE_ERROR((LM_ERROR,
                   "(%P|%t) ERROR: Unable to bind released TcpDataLink to "
                   "pending_release_links_ map: %p\n", ACE_TEXT("bind")));
        linkAction = StopLink;
        break;
      case 1:
        ACE_ERROR((LM_ERROR,
                   "(%P|%t) ERROR: Unable to bind released TcpDataLink to "
                   "pending_release_links_ map: already bound\n"));
        linkAction = StopLink;
        break;
      case 0:
        linkAction = ScheduleLinkRelease;
        break;
      default:
        break;
      }

    } else { // datalink_release_delay_ is 0
      linkAction = StopLink;
    }
  } // End of locking scope.

  // Actions are executed outside of the lock scope.
  switch( linkAction) {
    case StopLink:
      link->stop();
      break;

    case ScheduleLinkRelease:
      link->schedule_delayed_release();
      break;

    case None:
      break;
  };

  if (DCPS_debug_level > 9) {
    std::stringstream buffer;
    buffer << *link;
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) TcpTransport::release_datalink() - ")
               ACE_TEXT("link with priority %d released.\n%C"),
               link->transport_priority(),
               buffer.str().c_str()));
  }
}

TcpInst*
TcpTransport::get_configuration()
{
  return this->tcp_config_.in();
}

/// This method is called by a TcpConnection object that has been
/// created and opened by our acceptor_ as a result of passively
/// accepting a connection on our local address.  Ultimately, the connection
/// object needs to be paired with a DataLink object that is (or will be)
/// expecting this passive connection to be established.
void
TcpTransport::passive_connection(const ACE_INET_Addr& remote_address,
                                 const TcpConnection_rch& connection)
{
  DBG_ENTRY_LVL("TcpTransport", "passive_connection", 6);

  if (Transport_debug_level > 5) {
    std::stringstream os;
    dump(os);

    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) TcpTransport::passive_connection() - ")
               ACE_TEXT("established with %C:%d.\n%C"),
               remote_address.get_host_name(),
               remote_address.get_port_number(),
               os.str().c_str()));
  }

  const PriorityKey key(connection->transport_priority(),
                        remote_address,
                        remote_address == this->tcp_config_->local_address_,
                        connection->is_connector());

  VDBG_LVL((LM_DEBUG,
            ACE_TEXT("(%P|%t) TcpTransport::passive_connection() - ")
            ACE_TEXT("established with %C:%d.\n"),
            remote_address.get_host_name(),
            remote_address.get_port_number()), 2);

  GuardType guard(this->connections_lock_);
  TcpDataLink_rch link;
  ConnectionEvent* evt = 0;
  typedef std::multimap<ConnectionEvent*, PriorityKey>::iterator iter_t;
  for (iter_t iter = this->pending_connections_.begin();
       iter != pending_connections_.end(); ++iter) {
    if (iter->second == key) {
      GuardType guard(this->links_lock_);
      if (this->links_.find(key, link) == 0 /* found */) {
        VDBG_LVL((LM_DEBUG,
                  ACE_TEXT("(%P|%t) found matching key in links_ and ")
                  ACE_TEXT("pending_connections_\n")), 5);
        evt = iter->first;
        this->pending_connections_.erase(iter);
        break;
      }
    }
  }

  if (!link.is_nil()) { // found in pending_connections_
    // remove other entries for this ConnectionEvent in pending_connections_
    std::pair<iter_t, iter_t> range =
      this->pending_connections_.equal_range(evt);
    for (iter_t iter = range.first; iter != range.second; ++iter) {
      GuardType guard(this->links_lock_);
      this->links_.unbind(iter->second);
    }
    this->pending_connections_.erase(range.first, range.second);

    if (this->connect_tcp_datalink(link, connection) == -1) {
      VDBG_LVL((LM_ERROR,
                ACE_TEXT("(%P|%t) ERROR: connect_tcp_datalink failed\n")), 5);
      GuardType guard(this->links_lock_);
      this->links_.unbind(key);
    } else if (!evt->complete(link._retn())) {
      VDBG_LVL((LM_DEBUG,
                ACE_TEXT("(%P|%t) another connection completed first\n")), 5);
      GuardType guard(this->links_lock_);
      this->links_.unbind(key);
    } else {
      this->con_checker_->add(connection);
    }
    return;
  }

  // If we reach this point, this key was not in pending_connections_, so the
  // accept_datalink() call hasn't happened yet.  Store in connections_ for the
  // accept_datalink() method to find.

  VDBG_LVL((LM_DEBUG, "(%P|%t) # of bef connections: %d\n",
            this->connections_.size()), 5);

  // Check and report and problems.
  ConnectionMap::iterator where = this->connections_.find(key);

  if (where != this->connections_.end()) {
    ACE_ERROR((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: TcpTransport::passive_connection() - ")
               ACE_TEXT("connection with %C:%d at priority %d already exists, ")
               ACE_TEXT("overwriting previously established connection.\n"),
               remote_address.get_host_name(),
               remote_address.get_port_number(),
               connection->transport_priority()));
  }

  this->connections_[key] = connection;

  VDBG_LVL((LM_DEBUG, "(%P|%t) # of aftr connections: %d\n",
            this->connections_.size()), 5);

  // Enqueue the connection to the reconnect task that verifies if the connection
  // is re-established.
  this->con_checker_->add(connection);
}

/// Actively establish a connection to the remote address.
int
TcpTransport::make_active_connection(const ACE_INET_Addr& remote_address,
                                     const TcpDataLink_rch& link)
{
  DBG_ENTRY_LVL("TcpTransport", "make_active_connection", 6);

  // Create the connection object here.
  TcpConnection_rch connection = new TcpConnection;

  if (DCPS_debug_level > 9) {
    std::stringstream buffer;
    buffer << *link;
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) TcpTransport::make_active_connection() - ")
               ACE_TEXT("established with %C:%d and priority %d.\n%C"),
               remote_address.get_host_name(),
               remote_address.get_port_number(),
               link->transport_priority(),
               buffer.str().c_str()));
  }

  // Ask the connection object to attempt the active connection establishment.
  if (connection->active_connect(remote_address,
                                 this->tcp_config_->local_address_,
                                 link->transport_priority(),
                                 this->tcp_config_) != 0) {
    return -1;
  }

  return this->connect_tcp_datalink(link, connection);
}

/// Common code used by accept_datalink(), passive_connection(), and make_active_connection().
int
TcpTransport::connect_tcp_datalink(const TcpDataLink_rch& link,
                                   const TcpConnection_rch& connection)
{
  DBG_ENTRY_LVL("TcpTransport", "connect_tcp_datalink", 6);

  if (DCPS_debug_level > 4) {
    ACE_DEBUG((LM_DEBUG,
               ACE_TEXT("(%P|%t) TcpTransport::connect_tcp_datalink() - ")
               ACE_TEXT("creating send strategy with priority %d.\n"),
               link->transport_priority()));
  }

  TransportSendStrategy_rch send_strategy =
    new TcpSendStrategy(link, this->tcp_config_, connection,
                        new TcpSynchResource(connection,
                          this->tcp_config_->max_output_pause_period_),
                        this->reactor_task_, link->transport_priority());

  TransportStrategy_rch receive_strategy =
    new TcpReceiveStrategy(link, connection, this->reactor_task_);

  if (link->connect(connection, send_strategy, receive_strategy) != 0) {
    return -1;
  }

  return 0;
}

/// This function is called by the TcpReconnectTask thread to check if the passively
/// accepted connection is the re-established connection. If it is, then the "old" connection
/// object in the datalink is replaced by the "new" connection object.
int
TcpTransport::fresh_link(TcpConnection_rch connection)
{
  DBG_ENTRY_LVL("TcpTransport","fresh_link",6);

  TcpDataLink_rch link;
  GuardType guard(this->links_lock_);

  PriorityKey key(connection->transport_priority(),
                  connection->get_remote_address(),
                  connection->get_remote_address() == this->tcp_config_->local_address_,
                  connection->is_connector());

  if (this->links_.find(key, link) == 0) {
    TcpConnection_rch old_con = link->get_connection();

    // The connection is accepted but may not be associated with the datalink
    // at this point. The thread calling add_associations() will associate
    // the datalink with the connection in make_passive_connection().
    if (old_con.is_nil()) {
      return 0;
    }

    if (old_con.in() != connection.in())
      // Replace the "old" connection object with the "new" connection object.
    {
      return link->reconnect(connection.in());
    }
  }

  return 0;
}

void
TcpTransport::unbind_link(DataLink* link)
{
  TcpDataLink* tcp_link = static_cast<TcpDataLink*>(link);

  if (tcp_link == 0) {
    // Really an assertion failure
    ACE_ERROR((LM_ERROR,
               "(%P|%t) TcpTransport::unbind_link INTERNAL ERROR - "
               "Failed to downcast DataLink to TcpDataLink.\n"));
    return;
  }

  // Attempt to remove the TcpDataLink from our links_ map.
  PriorityKey key(
    tcp_link->transport_priority(),
    tcp_link->remote_address(),
    tcp_link->is_loopback(),
    tcp_link->is_active());

  VDBG_LVL((LM_DEBUG,
            "(%P|%t) TcpTransport::unbind_link link %@ PriorityKey "
            "prio=%d, addr=%C:%hu, is_loopback=%d, is_active=%d\n",
            link,
            tcp_link->transport_priority(),
            tcp_link->remote_address().get_host_addr(),
            tcp_link->remote_address().get_port_number(),
            (int)tcp_link->is_loopback(),
            (int)tcp_link->is_active()), 2);

  GuardType guard(this->links_lock_);

  if (this->pending_release_links_.unbind(key) != 0) {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) TcpTransport::unbind_link INTERNAL ERROR - "
               "Failed to find link %@ PriorityKey "
               "prio=%d, addr=%C:%hu, is_loopback=%d, is_active=%d\n",
               link,
               tcp_link->transport_priority(),
               tcp_link->remote_address().get_host_addr(),
               tcp_link->remote_address().get_port_number(),
               (int)tcp_link->is_loopback(),
               (int)tcp_link->is_active()));
  }
}

}
}
