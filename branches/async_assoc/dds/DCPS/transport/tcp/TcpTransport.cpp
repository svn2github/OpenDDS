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
#include "dds/DCPS/GuidConverter.h"

#include "dds/DCPS/async_debug.h"

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
      Priority priority,
      bool active)
{
   const ACE_INET_Addr remote_address =
         AssociationData::get_remote_address(remote);
   const bool is_loopback = remote_address == tcp_config_->local_address_;
   return PriorityKey(priority, remote_address, is_loopback, active);
}

TransportImpl::AcceptConnectResult
TcpTransport::connect_datalink(const RemoteTransport& remote,
      const ConnectionAttribs& attribs,
      TransportClient* client)
{
   DBG_ENTRY_LVL("TcpTransport", "connect_datalink", 6);

   //### Debug statements to track where associate is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink ENTRY\n"));

   const PriorityKey key =
         blob_to_key(remote.blob_, attribs.priority_, true /*active*/);

   VDBG_LVL((LM_DEBUG, "(%P|%t) TcpTransport::connect_datalink PriorityKey "
         "prio=%d, addr=%C:%hu, is_loopback=%d, is_active=%d\n",
         key.priority(), key.address().get_host_addr(),
         key.address().get_port_number(), key.is_loopback(),
         key.is_active()), 2);

   TcpDataLink_rch link;
   {
      //### Debug statements to track where associate is failing
      if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink LOCKING links_lock_\n"));
      GuardType guard(links_lock_);
      //### Debug statements to track where associate is failing
      if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink LOCKED links_lock_\n"));
      if (find_datalink_i(key, link, client, remote.repo_id_)) {
         //### Debug statements to track where associate is failing
         if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink Found existing datalink\n"));
         //### Debug statements to track where associate is failing
         if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink RELEASE links_lock_\n"));
         return AcceptConnectResult(link._retn());
      }
      link = new TcpDataLink(key.address(), this, attribs.priority_,
            key.is_loopback(), true /*active*/);
      if (links_.bind(key, link) != 0 /*OK*/) {
         ACE_ERROR((LM_ERROR, "(%P|%t) ERROR: TcpTransport::connect_datalink "
               "Unable to bind new TcpDataLink to "
               "TcpTransport in links_ map.\n"));
         //### Debug statements to track where associate is failing
         if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink RELEASE links_lock_\n"));
         return AcceptConnectResult();
      }
      //### Debug statements to track where associate is failing
      if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink RELEASE links_lock_\n"));
   }

   //### Debug statements to track where associate is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink CREATING NEW LINK AND CONNECTION\n"));

   TcpConnection_rch connection =
         new TcpConnection(key.address(), link->transport_priority(), tcp_config_);
   connection->set_datalink(link.in());

   TcpConnection* pConn = connection.in();
   TcpConnection_rch reactor_refcount(connection); // increment for reactor callback

   //### Debug statements to track where associate is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink ABOUT TO TELL connector_ TO CONNECT\n"));
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink ABOUT TO TELL connector_ TO CONNECT, is pConn nil? %s\n", pConn==0 ? "YES" : "NO"));
   ACE_TCHAR str[64];
   key.address().addr_to_string(str,sizeof(str)/sizeof(str[0]));
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink ABOUT TO TELL connector_ TO CONNECT, remote_address is: %s  \n",str ));

   // Can't make this call while holding onto TransportClient::lock_
   const int ret =
         connector_.connect(pConn, key.address(), ACE_Synch_Options::asynch);

   //### Debug statements to track where associate is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink CALL TO CONNECT RETURNED\n"));
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink CALL TO CONNECT RETURNED WITH errno = %s\n", errno == EWOULDBLOCK ? "EWOULDBLOCK" : "OTHER"));
   if (ret == -1 && errno != EWOULDBLOCK) {

      //### Debug statements to track where associate is failing
      if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink ERROR\n"));
      VDBG_LVL((LM_ERROR, "(%P|%t) TcpTransport::connect_datalink error %m.\n"), 2);
      return AcceptConnectResult();
   }

   //### Debug statements to track where associate is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink CONNECT RETURNED WITH VALUE: %d\n", ret));

   // Don't decrement count when reactor_refcount goes out of scope, see
   // TcpConnection::open()
   (void) reactor_refcount._retn();

   if (ret == 0) {
      // connect() completed synchronously and called TcpConnection::active_open().
      VDBG_LVL((LM_DEBUG, "(%P|%t) TcpTransport::connect_datalink "
            "completed synchronously.\n"), 2);
      return AcceptConnectResult(link._retn());
   }

   //### Debug statements to track where associate is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink ABOUT TO ADD_ON_START_CALLBACK\n"));

   if (!link->add_on_start_callback(client, remote.repo_id_)) {
      // link was started by the reactor thread before we could add a callback
      //### Debug statements to track where associate is failing
      if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink COULDN'T ADD_ON_START_CALLBACK B/C REACTOR THREAD STARTED LINK ALREADY\n"));

      VDBG_LVL((LM_DEBUG, "(%P|%t) TcpTransport::connect_datalink got link.\n"), 2);
      return AcceptConnectResult(link._retn());
   }

   //### Debug statements to track where associate is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_datalink ADD_ON_START_CALLBACK SUCCESSFUL CONNECT_DATALINK PENDING\n"));

   add_pending_connection(client, link.in());
   VDBG_LVL((LM_DEBUG, "(%P|%t) TcpTransport::connect_datalink pending.\n"), 2);
   return AcceptConnectResult(AcceptConnectResult::ACR_SUCCESS);
}

void
TcpTransport::async_connect_failed(const PriorityKey& key)
{

   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::async_connect_failed --> begin\n"));

   ACE_ERROR((LM_ERROR, "(%P|%t) ERROR: Failed to make active connection.\n"));
   GuardType guard(links_lock_);
   TcpDataLink_rch link;
   links_.find(key, link);
   links_.unbind(key);
   guard.release();

   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::async_connect_failed --> invoke_on_start_callbacks\n"));

   link->invoke_on_start_callbacks(false);

   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::async_connect_failed --> end\n"));
}

bool
TcpTransport::find_datalink_i(const PriorityKey& key, TcpDataLink_rch& link,
      TransportClient* client, const RepoId& remote_id)
{
   //### Debug statements to track where associate is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::find_datalink_i enter method\n"));

   if (links_.find(key, link) == 0 /*OK*/) {
      //TODO: check old TcpTransport::find_datalink_i interaction with TcpConnection
      if (!link->add_on_start_callback(client, remote_id)) {
         VDBG_LVL((LM_DEBUG, ACE_TEXT("(%P|%t) TcpTransport::find_datalink_i ")
               ACE_TEXT("link found, already started.\n")), 2);
         // Since the link was already started, we won't get an "on start"
         // callback, and the link is immediately usable.
         return true;
      }
      VDBG_LVL((LM_DEBUG, ACE_TEXT("(%P|%t) TcpTransport::find_datalink_i ")
            ACE_TEXT("link found, pending.\n")), 2);
      add_pending_connection(client, link.in());
      link = 0; // don't return link to TransportClient
      return true;

   } else if (pending_release_links_.find(key, link) == 0 /*OK*/) {
      if (link->cancel_release()) {
         link->set_release_pending(false);
         if (pending_release_links_.unbind(key, link) == 0 /*OK*/
               && links_.bind(key, link) == 0 /*OK*/) {
            VDBG_LVL((LM_DEBUG, ACE_TEXT("(%P|%t) TcpTransport::find_datalink_i ")
                  ACE_TEXT("link from pending release list.\n")), 2);
            return true;
         }
      }
      link = 0; // don't return link to TransportClient
      return false;
   }
   return false;
}

TransportImpl::AcceptConnectResult
TcpTransport::accept_datalink(const RemoteTransport& remote,
      const ConnectionAttribs& attribs,
      TransportClient* client)
{
  //### Debug statements to track where connection is failing
  GuidConverter remote_conv(remote.repo_id_);
  GuidConverter local_conv(attribs.local_id_);
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::accept_datalink --> enter %C accepting connection from %C\n", std::string(local_conv).c_str(), std::string(remote_conv).c_str()));
   GuardType guard(connections_lock_);
   const PriorityKey key =
         blob_to_key(remote.blob_, attribs.priority_, false /* !active */);

   VDBG_LVL((LM_DEBUG, "(%P|%t) TcpTransport::accept_datalink PriorityKey "
         "prio=%d, addr=%C:%hu, is_loopback=%d, is_active=%d\n", attribs.priority_,
         key.address().get_host_addr(), key.address().get_port_number(),
         key.is_loopback(), key.is_active()), 2);

   TcpDataLink_rch link;
   {
      GuardType guard(links_lock_);
      if (find_datalink_i(key, link, client, remote.repo_id_)) {
        //### Debug statements to track where connection is failing
        if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::accept_datalink --> found datalink, return link\n"));
         return AcceptConnectResult(link._retn());

      } else {
        //### Debug statements to track where connection is failing
        if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::accept_datalink --> link not found, create link\n"));
         link = new TcpDataLink(key.address(), this, key.priority(),
               key.is_loopback(), key.is_active());
         if (links_.bind(key, link) != 0 /*OK*/) {
            ACE_ERROR((LM_ERROR,
                  "(%P|%t) ERROR: TcpTransport::accept_datalink "
                  "Unable to bind new TcpDataLink to "
                  "TcpTransport in links_ map.\n"));
            return AcceptConnectResult();
         }
      }
   }

   TcpConnection_rch connection;
   const ConnectionMap::iterator iter = connections_.find(key);
   if (iter != connections_.end()) {
     //### Debug statements to track where connection is failing
     if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::accept_datalink --> existing connection found\n"));
      connection = iter->second;
      connections_.erase(iter);
   }

   if (connection.is_nil()) {
     //### Debug statements to track where connection is failing
     if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::accept_datalink --> no existing connection found\n"));
      if (!link->add_on_start_callback(client, remote.repo_id_)) {
        //### Debug statements to track where connection is failing
        if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::accept_datalink --> couldn't add_on_start_callback\n"));
         VDBG_LVL((LM_DEBUG, "(%P|%t) TcpTransport::accept_datalink "
               "got started link %@.\n", link.in()), 2);
         return AcceptConnectResult(link._retn());
      }
      //### Debug statements to track where connection is failing
      if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::accept_datalink --> add_on_start_callback successful\n"));
      VDBG_LVL((LM_DEBUG, "(%P|%t) TcpTransport::accept_datalink "
            "no existing TcpConnection.\n"), 2);
      //### Debug statements to track where connection is failing
      if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::accept_datalink --> try to add_pending_connection\n"));
      add_pending_connection(client, link.in());
      //### Debug statements to track where connection is failing
      if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::accept_datalink --> return ACR_SUCCESS and finish connection later\n"));
      // no link ready, passive_connection will complete later
      return AcceptConnectResult(AcceptConnectResult::ACR_SUCCESS);
   }

   guard.release(); // connect_tcp_datalink() isn't called with connections_lock_
   if (connect_tcp_datalink(link, connection) == -1) {
      GuardType guard(links_lock_);
      links_.unbind(key);
      link = 0;
   }
   VDBG_LVL((LM_DEBUG, "(%P|%t) TcpTransport::accept_datalink "
         "connected link %@.\n", link.in()), 2);
   return AcceptConnectResult(link._retn());
}

void
TcpTransport::stop_accepting_or_connecting(TransportClient* client,
      const RepoId& remote_id)
{
   //### Debug statements to track where connection is failing
   GuidConverter remote_converted(remote_id);
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::stop_accepting_or_connecting --> enter to stop TransportClient connecting to Remote: %C\n", std::string(remote_converted).c_str() ));
   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::stop_accepting_or_connecting --> trying to LOCK connections_lock_\n"));
   GuardType guard(connections_lock_);
   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::stop_accepting_or_connecting --> LOCKED connections_lock_\n"));
   typedef std::multimap<TransportClient*, DataLink_rch>::iterator iter_t;
   const std::pair<iter_t, iter_t> range =
         pending_connections_.equal_range(client);
   for (iter_t iter = range.first; iter != range.second; ++iter) {
      //### Debug statements
      GuidConverter remote_rosc(remote_id);
      if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::stop_accepting_or_connecting --> about to remove_on_start_callback for client connecting to Remote: %C\n", std::string(remote_rosc).c_str() ));
      iter->second->remove_on_start_callback(client, remote_id);
   }
   pending_connections_.erase(range.first, range.second);
   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::stop_accepting_or_connecting --> RELEASING connections_lock_\n"));
   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::stop_accepting_or_connecting --> exit\n"));
}

bool
TcpTransport::configure_i(TransportInst* config)
{
   //### Debug statements to track where associate is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::configure_i --> BEGIN\n"));

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

   connector_.open(reactor_task_->get_reactor());

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
        size_t pos = this->tcp_config_->local_address_str_.find_last_of(
                                                ":]", std::string::npos, 2);
         std::string str = this->tcp_config_->local_address_str_.substr(0, pos + 1);
      if (this->tcp_config_->local_address_str_[pos] == ']') {
        str += ":";
      }
         str += out.str();
         this->tcp_config_->local_address_str_ = str;
      }
   }

   //### Debug statements to track where associate is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::configure_i --> END SUCCESS\n"));

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
      this->pending_connections_.clear();
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

  //### Debug statements to track where connection is failing
  if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::release_datalink --> begin\n"));

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
         //### Debug statements to track where connection is failing
         if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::release_datalink --> set_release_pending(true)\n"));
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
   switch (linkAction) {
   case StopLink:
     //### Debug statements to track where connection is failing
     if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::release_datalink --> stop link\n"));
      link->stop();
      break;

   case ScheduleLinkRelease:
     //### Debug statements to track where connection is failing
     if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::release_datalink --> schedule_delayed_release\n"));
      link->schedule_delayed_release();
      break;

   case None:
      break;
   }

   if (DCPS_debug_level > 9) {
      std::stringstream buffer;
      buffer << *link;
      ACE_DEBUG((LM_DEBUG,
            ACE_TEXT("(%P|%t) TcpTransport::release_datalink() - ")
            ACE_TEXT("link with priority %d released.\n%C"),
            link->transport_priority(),
            buffer.str().c_str()));
   }
   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::release_datalink --> end\n"));
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

   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::passive_connection --> begin\n"));


   DBG_ENTRY_LVL("TcpTransport", "passive_connection", 6);

   const PriorityKey key(connection->transport_priority(),
         remote_address,
         remote_address == tcp_config_->local_address_,
         connection->is_connector());

   VDBG_LVL((LM_DEBUG, ACE_TEXT("(%P|%t) TcpTransport::passive_connection() - ")
         ACE_TEXT("established with %C:%d.\n"),
         remote_address.get_host_name(),
         remote_address.get_port_number()), 2);

   GuardType connection_guard(connections_lock_);
   TcpDataLink_rch link;
   {
      GuardType guard(links_lock_);
      links_.find(key, link);
   }

   if (!link.is_nil()) {
      connection_guard.release();
      if (connect_tcp_datalink(link, connection) == -1) {
         VDBG_LVL((LM_ERROR,
               ACE_TEXT("(%P|%t) ERROR: connect_tcp_datalink failed\n")), 5);
         GuardType guard(links_lock_);
         links_.unbind(key);
      } else {
         con_checker_->add(connection);
      }
      return;
   }

   // If we reach this point, this link was not in links_, so the
   // accept_datalink() call hasn't happened yet.  Store in connections_ for the
   // accept_datalink() method to find.
   VDBG_LVL((LM_DEBUG, "(%P|%t) # of bef connections: %d\n", connections_.size()), 5);
   const ConnectionMap::iterator where = connections_.find(key);
   if (where != connections_.end()) {
      ACE_ERROR((LM_ERROR,
            ACE_TEXT("(%P|%t) ERROR: TcpTransport::passive_connection() - ")
            ACE_TEXT("connection with %C:%d at priority %d already exists, ")
            ACE_TEXT("overwriting previously established connection.\n"),
            remote_address.get_host_name(),
            remote_address.get_port_number(),
            connection->transport_priority()));
   }
   connections_[key] = connection;
   VDBG_LVL((LM_DEBUG, "(%P|%t) # of aftr connections: %d\n", connections_.size()), 5);

   con_checker_->add(connection);
}

/// Common code used by accept_datalink(), passive_connection(), and active completion.
int
TcpTransport::connect_tcp_datalink(const TcpDataLink_rch& link,
      const TcpConnection_rch& connection)
{

   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_tcp_datalink --> begin\n"));

   DBG_ENTRY_LVL("TcpTransport", "connect_tcp_datalink", 6);

   ++last_link_;
   if (DCPS_debug_level > 4) {
      ACE_DEBUG((LM_DEBUG,
            ACE_TEXT("(%P|%t) TcpTransport::connect_tcp_datalink() [%d] - ")
            ACE_TEXT("creating send strategy with priority %d.\n"),
            last_link_, link->transport_priority()));
   }
   connection->id() = last_link_;

   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_tcp_datalink --> create send_strategy\n"));

   TransportSendStrategy_rch send_strategy =
         new TcpSendStrategy(last_link_, link, this->tcp_config_, connection,
               new TcpSynchResource(connection,
                     this->tcp_config_->max_output_pause_period_),
                     this->reactor_task_, link->transport_priority());

   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_tcp_datalink --> create receive_strategy\n"));

   TransportStrategy_rch receive_strategy =
         new TcpReceiveStrategy(link, connection, this->reactor_task_);

   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_tcp_datalink --> call connect on link\n"));

   if (link->connect(connection, send_strategy, receive_strategy) != 0) {
      //### Debug statements to track where connection is failing
      if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_tcp_datalink --> connect failed --> end FAILURE\n"));

      return -1;
   }

   //### Debug statements to track where connection is failing
   if (ASYNC_debug) ACE_DEBUG((LM_DEBUG, "(%P|%t|%T) ASYNC_DBG:TcpTransport::connect_tcp_datalink --> end SUCCESS\n"));

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
