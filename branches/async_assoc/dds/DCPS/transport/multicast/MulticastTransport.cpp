/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "MulticastTransport.h"
#include "MulticastDataLink.h"
#include "MulticastReceiveStrategy.h"
#include "MulticastSendStrategy.h"
#include "MulticastSession.h"
#include "BestEffortSessionFactory.h"
#include "ReliableSessionFactory.h"

#include "ace/Log_Msg.h"
#include "ace/Truncate.h"

#include "dds/DCPS/RepoIdConverter.h"
#include "dds/DCPS/AssociationData.h"
#include "dds/DCPS/transport/framework/NetworkAddress.h"
#include "dds/DCPS/transport/framework/TransportExceptions.h"
#include "dds/DCPS/transport/framework/TransportClient.h"

namespace OpenDDS {
namespace DCPS {

MulticastTransport::MulticastTransport(const TransportInst_rch& inst)
: config_i_(0)
{
	if (!inst.is_nil()) {
		if (!configure(inst.in())) {
			throw Transport::UnableToCreate();
		}
	}
}

MulticastTransport::~MulticastTransport()
{
}

////***DataLink*
////MulticastTransport::find_datalink_i(const RepoId& /*local_id*/,
////                                    const RepoId& remote_id,
////                                    const TransportBLOB& /*remote_data*/,
////                                    bool /*remote_reliable*/,
////                                    bool /*remote_durable*/,
////                                    const ConnectionAttribs& /*attribs*/,
////                                    bool active)
////***
///***  For reference on RemoteTransport from TransportImpl
//    struct RemoteTransport {
//    RepoId repo_id_;
//    TransportBLOB blob_;
//    Priority publication_transport_priority_;
//    bool reliable_, durable_;
//  };
//***/
//TransportImpl::AcceptConnectResult
//MulticastTransport::connect_datalink(const RemoteTransport& remote,
//                                     const ConnectionAttribs& attribs,
//                                     TransportClient* client)
//{
//  // To accommodate the one-to-many nature of multicast reservations,
//  // a session layer is used to maintain state between unique pairs
//  // of DomainParticipants over a single DataLink instance. Given
//  // that TransportImpl instances may only be attached to either
//  // Subscribers or Publishers within the same DomainParticipant,
//  // it may be assumed that the local_id always references the same
//  // participant.
//  MulticastDataLink_rch link;
///***  if (active && !this->client_link_.is_nil()) {
//    link = this->client_link_;
//  }
//
//  if (!active && !this->server_link_.is_nil()) {
//    link = this->server_link_;
//  }
//***/
//  const bool active = true;
//
//  if(!this->client_link_.is_nil()) {
//    link = this->client_link_;
//  }
//
//  if (!link.is_nil()) {
//
//    //***MulticastPeer remote_peer = RepoIdConverter(remote_id).participantId();
//	MulticastPeer remote_peer = RepoIdConverter(remote.repo_id_).participantId();
//
//	MulticastSession_rch session = link->find_or_create_session(remote_peer);
///***    MulticastSession_rch session = link->find_session(remote_peer);
//
//    if (session.is_nil()) {
//      // From the framework's point-of-view, no DataLink was found.
//      // This way we will progress to the connect/accept stage for handshaking.
//      return 0;
//    }
//***/
//    if (!session->start(active, this->connections_.count(remote_peer))) {
//      ACE_ERROR_RETURN((LM_ERROR,
//                        ACE_TEXT("(%P|%t) ERROR: ")
//                        /***ACE_TEXT("MulticastTransport[%C]::find_datalink_i: ")***/
//                        ACE_TEXT("MulticastTransport[%C]::connect_datalink: ")
//                        ACE_TEXT("failed to start session for remote peer: 0x%x!\n"),
//                        this->config_i_->name().c_str(), remote_peer),
//                       0);
//    }
//
//    /***VDBG_LVL((LM_DEBUG, "(%P|%t) MulticastTransport[%C]::find_datalink_i "
//              "started session for remote peer: 0x%x\n",
//              this->config_i_->name().c_str(), remote_peer), 2);***/
//    VDBG_LVL((LM_DEBUG, "(%P|%t) MulticastTransport[%C]::connect_datalink "
//              "started session for remote peer: 0x%x\n",
//              this->config_i_->name().c_str(), remote_peer), 2);
//    return AcceptConnectResult(link._retn());
//  }
//
//  //*** If we arrived here the link was nil so need to make datalink
//
//
//  return AcceptConnectResult();
//  //***return link._retn();
//}

MulticastDataLink*
MulticastTransport::make_datalink(const RepoId& local_id,
		Priority priority,
		bool active)
{
	RcHandle<MulticastSessionFactory> session_factory;
	if (this->config_i_->reliable_) {
		ACE_NEW_RETURN(session_factory, ReliableSessionFactory, 0);
	} else {
		ACE_NEW_RETURN(session_factory, BestEffortSessionFactory, 0);
	}

	MulticastPeer local_peer = RepoIdConverter(local_id).participantId();

	VDBG_LVL((LM_DEBUG, "(%P|%t) MulticastTransport[%C]::make_datalink "
			"peers: local 0x%x priority %d active %d\n",
			this->config_i_->name().c_str(), local_peer,
			priority, active), 2);

	MulticastDataLink_rch link;
	ACE_NEW_RETURN(link,
			MulticastDataLink(this,
					session_factory.in(),
					local_peer,
					active),
					0);

	// Configure link with transport configuration and reactor task:
	TransportReactorTask_rch rtask = reactor_task();
	//link->configure(this->config_i_.in(), rtask.in());
	link->configure(config_i_.in(), rtask.in());

	// Assign send strategy:
	MulticastSendStrategy* send_strategy;
	ACE_NEW_RETURN(send_strategy, MulticastSendStrategy(link.in()), 0);
	link->send_strategy(send_strategy);

	// Assign receive strategy:
	MulticastReceiveStrategy* recv_strategy;
	ACE_NEW_RETURN(recv_strategy, MulticastReceiveStrategy(link.in()), 0);
	link->receive_strategy(recv_strategy);

	// Join multicast group:
	if (!link->join(this->config_i_->group_address_)) {
		ACE_TCHAR str[64];
		this->config_i_->group_address_.addr_to_string(str,
				sizeof(str)/sizeof(str[0]));
		ACE_ERROR_RETURN((LM_ERROR,
				ACE_TEXT("(%P|%t) ERROR: ")
				ACE_TEXT("MulticastTransport::make_datalink: ")
				ACE_TEXT("failed to join multicast group: %s!\n"),
				str),
				0);
	}
	return link._retn();
}

MulticastSession*
MulticastTransport::start_session(const MulticastDataLink_rch& link,
		MulticastPeer remote_peer, bool active)
{
	if (link.is_nil()) {
		ACE_ERROR_RETURN((LM_ERROR,
				ACE_TEXT("(%P|%t) ERROR: ")
				ACE_TEXT("MulticastTransport[%C]::start_session: ")
				ACE_TEXT("link is nil\n"),
				this->config_i_->name().c_str()),
				0);
	}

	MulticastSession_rch session = link->find_or_create_session(remote_peer);
	if (session.is_nil()) {
		ACE_ERROR_RETURN((LM_ERROR,
				ACE_TEXT("(%P|%t) ERROR: ")
				ACE_TEXT("MulticastTransport[%C]::start_session: ")
				ACE_TEXT("failed to create session for remote peer: 0x%x!\n"),
				this->config_i_->name().c_str(), remote_peer),
				0);
	}

	if (!session->start(active, this->connections_.count(remote_peer))) {
		ACE_ERROR_RETURN((LM_ERROR,
				ACE_TEXT("(%P|%t) ERROR: ")
				ACE_TEXT("MulticastTransport[%C]::start_session: ")
				ACE_TEXT("failed to start session for remote peer: 0x%x!\n"),
				this->config_i_->name().c_str(), remote_peer),
				0);
	}

	return session._retn();
}

TransportImpl::AcceptConnectResult
MulticastTransport::connect_datalink(const RemoteTransport& remote,
		const ConnectionAttribs& attribs,
		TransportClient* client)
{
	MulticastDataLink_rch link = this->client_link_;
	if (link.is_nil()) {
		link = this->make_datalink(attribs.local_id_, attribs.priority_, true /*active*/);
		this->client_link_ = link;

		if (this->server_link_.is_nil()) {
			// Create the "server" link now, so that it can receive MULTICAST_SYN
			// from any peers that have add_association() first.
			this->server_link_ = make_datalink(attribs.local_id_, attribs.priority_,
					false /*active*/);
		}
	}

	MulticastPeer remote_peer = RepoIdConverter(remote.repo_id_).participantId();

	MulticastSession_rch session =
			this->start_session(link, remote_peer, true /*active*/);
	if (session.is_nil()) {
		return AcceptConnectResult();
	}

	if (remote_peer == RepoIdConverter(attribs.local_id_).participantId()) {
		VDBG_LVL((LM_DEBUG, "(%P|%t) MulticastTransport[%C]::connect_datalink_i "
				"loopback on peer: 0x%x, skipping wait_for_ack\n",
				this->config_i_->name().c_str(), remote_peer), 2);
		return AcceptConnectResult(link._retn());
	}

	if (session->acked()) {
		return AcceptConnectResult(link._retn());
	}

	return AcceptConnectResult(AcceptConnectResult::ACR_SUCCESS);
}

TransportImpl::AcceptConnectResult
MulticastTransport::accept_datalink(const RemoteTransport& remote,
									const ConnectionAttribs& attribs,
									TransportClient* client)
{
	MulticastDataLink_rch link = this->server_link_;
	if (link.is_nil()) {
		link = this->make_datalink(attribs.local_id_, attribs.priority_, false /*passive*/);
		this->server_link_ = link;
	}

	MulticastPeer remote_peer = RepoIdConverter(remote.repo_id_).participantId();
	GuardType guard(this->connections_lock_);

	if (connections_.count(remote_peer)) {
		VDBG((LM_DEBUG, "(%P|%t) MulticastTransport::accept_datalink found\n"));
		MulticastSession_rch session =
			this->start_session(this->server_link_, remote_peer, false /*!active*/);

		if (session.is_nil()){
			link = 0;
		}
		return AcceptConnectResult(link._retn());
	} else {
		this->pending_connections_[remote_peer].push_back(std::pair<TransportClient*, RepoId>(client, attribs.local_id_));
		MulticastSession_rch session =
					this->start_session(this->server_link_, remote_peer, false /*!active*/);
		return AcceptConnectResult(AcceptConnectResult::ACR_SUCCESS);

	}
}

void
MulticastTransport::stop_accepting_or_connecting(TransportClient* client,
                                           const RepoId& remote_id)
{
  VDBG((LM_DEBUG, "(%P|%t) MulticastTransport::stop_accepting_or_connecting\n"));
  GuardType guard(this->connections_lock_);
  for (PendConnMap::iterator it = this->pending_connections_.begin();
       it != this->pending_connections_.end(); ++it) {
    for (size_t i = 0; i < it->second.size(); ++i) {
      if (it->second[i].first == client && it->second[i].second == remote_id) {
        it->second.erase(it->second.begin() + i);
        break;
      }
    }
    if (it->second.empty()) {
      this->pending_connections_.erase(it);
      return;
    }
  }
}

void
MulticastTransport::passive_connection(MulticastPeer peer)
{
	//need to send ack to active side so it can return from connect_datalink
	MulticastSession_rch session =
			this->start_session(this->server_link_, peer, false /*!active*/);

	session->send_synack();

	GuardType guard(this->connections_lock_);
	VDBG_LVL((LM_DEBUG, "(%P|%t) MulticastTransport[%C]::passive_connection "
			"from peer 0x%x\n",
			this->config_i_->name().c_str(), peer), 2);

	  const PendConnMap::iterator pend = this->pending_connections_.find(peer);

	  if (pend != pending_connections_.end()) {
	    VDBG((LM_DEBUG, "(%P|%t) MulticastTransport::passive_connection completing\n"));
	    const DataLink_rch link = static_rchandle_cast<DataLink>(server_link_);
	    for (size_t i = 0; i < pend->second.size(); ++i) {
	      pend->second[i].first->use_datalink(pend->second[i].second, link);
	    }
	    this->pending_connections_.erase(pend);
	  }
	  //if connection was pending, calls to use_datalink finalized the connection
	  //if it was not previously pending, accept_datalink() will finalize connection
	  this->connections_.insert(peer);
}

bool
MulticastTransport::configure_i(TransportInst* config)
{
	this->config_i_ = dynamic_cast<MulticastInst*>(config);
	if (this->config_i_ == 0) {
		ACE_ERROR_RETURN((LM_ERROR,
				ACE_TEXT("(%P|%t) ERROR: ")
				ACE_TEXT("MulticastTransport[%@]::configure_i: ")
				ACE_TEXT("invalid configuration!\n"), this),
				false);
	}
	this->config_i_->_add_ref();

	if (!this->config_i_->group_address_.is_multicast()) {
		ACE_ERROR_RETURN((LM_ERROR,
				ACE_TEXT("(%P|%t) ERROR: ")
				ACE_TEXT("MulticastTransport[%@]::configure_i: ")
				ACE_TEXT("invalid configuration: address %C is not ")
				ACE_TEXT("multicast.\n"),
				this, this->config_i_->group_address_.get_host_addr()),
				false);
	}

	this->create_reactor_task(this->config_i_->async_send_);

	return true;
}

void
MulticastTransport::shutdown_i()
{
	if (!this->client_link_.is_nil()) {
		this->client_link_->transport_shutdown();
	}
	if (!this->server_link_.is_nil()) {
		this->server_link_->transport_shutdown();
	}
	this->config_i_ = 0;
}

bool
MulticastTransport::connection_info_i(TransportLocator& info) const
{
	NetworkAddress network_address(this->config_i_->group_address_);

	ACE_OutputCDR cdr;
	cdr << network_address;

	const CORBA::ULong len = static_cast<CORBA::ULong>(cdr.total_length());
	char* buffer = const_cast<char*>(cdr.buffer()); // safe

	info.transport_type = "multicast";
	info.data = TransportBLOB(len, len, reinterpret_cast<CORBA::Octet*>(buffer));

	return true;
}

void
MulticastTransport::release_datalink(DataLink* /*link*/)
{
	// No-op for multicast: keep both the client_link_ and server_link_ around
	// until the transport is shut down.
}


} // namespace DCPS
} // namespace OpenDDS
