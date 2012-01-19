/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "RtpsUdpTransport.h"
#include "RtpsUdpInst.h"
#include "RtpsUdpSendStrategy.h"
#include "RtpsUdpReceiveStrategy.h"

#include "dds/DCPS/transport/framework/NetworkAddress.h"
#include "dds/DCPS/AssociationData.h"

#include "dds/DCPS/RTPS/BaseMessageUtils.h"
#include "dds/DCPS/RTPS/RtpsMessageTypesTypeSupportImpl.h"

#include "ace/CDR_Base.h"
#include "ace/Log_Msg.h"
#include "ace/Sock_Connect.h"


namespace OpenDDS {
namespace DCPS {

RtpsUdpTransport::RtpsUdpTransport(const TransportInst_rch& inst)
  : handshake_condition_(connections_lock_)
{
  if (!inst.is_nil()) {
    configure(inst.in());
  }
}

RtpsUdpDataLink*
RtpsUdpTransport::make_datalink(const GuidPrefix_t& local_prefix)
{
  TransportReactorTask_rch rt = reactor_task();
  ACE_NEW_RETURN(link_,
                 RtpsUdpDataLink(this, local_prefix, config_i_.in(), rt.in()),
                 0);

  RtpsUdpSendStrategy* send_strategy;
  ACE_NEW_RETURN(send_strategy, RtpsUdpSendStrategy(link_.in()), 0);
  link_->send_strategy(send_strategy);

  RtpsUdpReceiveStrategy* recv_strategy;
  ACE_NEW_RETURN(recv_strategy, RtpsUdpReceiveStrategy(link_.in()), 0);
  link_->receive_strategy(recv_strategy);

  if (!link_->open(unicast_socket_)) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("RtpsUdpTransport::make_datalink: ")
                      ACE_TEXT("failed to open DataLink for socket %d\n"),
                      unicast_socket_.get_handle()),
                     0);
  }

  // RtpsUdpDataLink now owns the socket
  unicast_socket_.set_handle(ACE_INVALID_HANDLE);

  return RtpsUdpDataLink_rch(link_)._retn();
}

DataLink*
RtpsUdpTransport::find_datalink_i(const RepoId& /*local_id*/,
                                  const RepoId& /*remote_id*/,
                                  const TransportBLOB& /*remote_data*/,
                                  bool /*remote_reliable*/,
                                  const ConnectionAttribs& /*attribs*/,
                                  bool /*active*/)
{
  // We're not going to use find_datalink_i() for this transport.
  // Instead, each new association will use either connect or accept.
  return 0;
}

DataLink*
RtpsUdpTransport::connect_datalink_i(const RepoId& local_id,
                                     const RepoId& remote_id,
                                     const TransportBLOB& remote_data,
                                     bool remote_reliable,
                                     const ConnectionAttribs& attribs)
{
  RtpsUdpDataLink_rch link = link_;
  if (link_.is_nil()) {
    link = make_datalink(local_id.guidPrefix);
    if (link.is_nil()) {
      return 0;
    }
  }

  if (use_datalink(local_id, remote_id, remote_data,
                   attribs.local_reliable_, remote_reliable)) {
    return link._retn();
  } else {
    ACE_Time_Value abs_timeout = ACE_OS::gettimeofday()
      + config_i_->handshake_timeout_;
    ACE_GUARD_RETURN(ACE_Thread_Mutex, g, connections_lock_, 0);
    while (!link->handshake_done(local_id, remote_id)) {
      if (handshake_condition_.wait(&abs_timeout) == -1) {
        return 0;
      }
    }
    return link._retn();
  }
  return 0;
}

DataLink*
RtpsUdpTransport::accept_datalink(ConnectionEvent& ce)
{
  const std::string ttype = "rtps_udp";
  const CORBA::ULong num_blobs = ce.remote_association_.remote_data_.length();
  RtpsUdpDataLink_rch link = link_;

  for (CORBA::ULong idx = 0; idx < num_blobs; ++idx) {
    if (ce.remote_association_.remote_data_[idx].transport_type.in() == ttype) {
      if (link_.is_nil()) {
        link = make_datalink(ce.local_id_.guidPrefix);
        if (link.is_nil()) {
          // we're not going to be able to recover in another iteration
          return 0;
        }
      }

      use_datalink(ce.local_id_, ce.remote_association_.remote_id_,
                   ce.remote_association_.remote_data_[idx].data,
                   ce.attribs_.local_reliable_,
                   ce.remote_association_.remote_reliable_);
      return link._retn();
    }
  }

  return 0;
}

void
RtpsUdpTransport::handshake(const RepoId& /*local_id*/,
                            const RepoId& /*remote_id*/)
{
  // [active/writer side] By the time this is called, the shared
  // state in the data link (remote_readers_, remote_writers_) has been updated.
  // The associate() thread may be waiting in connect_datalink_i(), wake it.
  ACE_GUARD(ACE_Thread_Mutex, g, connections_lock_);
  handshake_condition_.broadcast();
}

void
RtpsUdpTransport::stop_accepting(ConnectionEvent& /*ce*/)
{
  // nothing to do here, we don't defer any accept actions in accept_datalink()
}

bool
RtpsUdpTransport::use_datalink(const RepoId& local_id,
                               const RepoId& remote_id,
                               const TransportBLOB& remote_data,
                               bool local_reliable,
                               bool remote_reliable)
{
  bool requires_inline_qos;
  ACE_INET_Addr addr = get_connection_addr(remote_data, requires_inline_qos);
  link_->add_locator(remote_id, addr, requires_inline_qos);
  link_->associated(local_id, remote_id, local_reliable, remote_reliable);
  return link_->handshake_done(local_id, remote_id);
}

ACE_INET_Addr
RtpsUdpTransport::get_connection_addr(const TransportBLOB& remote,
                                      bool& requires_inline_qos) const
{
  using namespace OpenDDS::RTPS;
  LocatorSeq locators;
  DDS::ReturnCode_t result =
    blob_to_locators(remote, locators, requires_inline_qos);
  if (result != DDS::RETCODE_OK) {
    return ACE_INET_Addr();
  }

  for (CORBA::ULong i = 0; i < locators.length(); ++i) {
    ACE_INET_Addr addr;
    // If conversion was successful
    if (locator_to_address(addr, locators[i]) == 0) {
      // if this is a unicast address, or if we are allowing multicast
      if (!addr.is_multicast() || config_i_->use_multicast_) {
        return addr;
      }
    }
  }

  // Return default address
  return ACE_INET_Addr();
}

bool
RtpsUdpTransport::connection_info_i(TransportLocator& info) const
{
  using namespace OpenDDS::RTPS;

  LocatorSeq locators;
  CORBA::ULong idx = 0;

  // multicast first so it's preferred by remote peers
  if (config_i_->use_multicast_) {
    locators.length(2);
    locators[0].kind =
      (config_i_->multicast_group_address_.get_type() == AF_INET6)
      ? LOCATOR_KIND_UDPv6 : LOCATOR_KIND_UDPv4;
    locators[0].port = config_i_->multicast_group_address_.get_port_number();
    RTPS::address_to_bytes(locators[0].address, 
                           config_i_->multicast_group_address_);
    idx = 1;

  } else {
    locators.length(1);
  }

  locators[idx].kind = (config_i_->local_address_.get_type() == AF_INET6)
                       ? LOCATOR_KIND_UDPv6 : LOCATOR_KIND_UDPv4;
  locators[idx].port = config_i_->local_address_.get_port_number();
  RTPS::address_to_bytes(locators[idx].address, 
                         config_i_->local_address_);

  info.transport_type = "rtps_udp";
  RTPS::locators_to_blob(locators, info.data);
  return true;
}

bool
RtpsUdpTransport::configure_i(TransportInst* config)
{
  config_i_ = RcHandle<RtpsUdpInst>(dynamic_cast<RtpsUdpInst*>(config), false);

  if (config_i_.is_nil()) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("RtpsUdpTransport::configure_i: ")
                      ACE_TEXT("invalid configuration!\n")),
                     false);
  }

  // Open the socket here so that any addresses/ports left
  // unspecified in the RtpsUdpInst are known by the time we get to
  // connection_info_i().  Opening the sockets here also allows us to
  // detect and report errors during DataReader/Writer setup instead
  // of during association.

  if (unicast_socket_.open(config_i_->local_address_) != 0) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("RtpsUdpTransport::configure_i: socket open:")
                      ACE_TEXT("%m\n")),
                     false);
  }

  if (config_i_->local_address_.is_any()) {

    size_t count;
    ACE_INET_Addr* addrs_raw = 0;
    const int res = ACE::get_ip_interfaces(count, addrs_raw);
    ACE_Auto_Array_Ptr<ACE_INET_Addr> addrs(addrs_raw);
    if (res != 0) {
      ACE_ERROR_RETURN((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: RtpsUdpDataLink::configure_i - %p\n"),
        ACE_TEXT("ACE::get_ip_interfaces")), false);
    }

    for (int i = 0; i < static_cast<int>(count); ++i) {
      if (!addrs[i].is_loopback()) {
        config_i_->local_address_ = addrs[i];
      }
    }

    // if it's still "any" at this point, we may only have loopback interface
    if (config_i_->local_address_.is_any() && count > 0) {
      config_i_->local_address_ = addrs[0];
    }
  } 

  if (config_i_->local_address_.get_port_number() == 0) {

    ACE_INET_Addr address;
    if (unicast_socket_.get_local_addr(address) != 0) {
      ACE_ERROR_RETURN((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: RtpsUdpDataLink::configure_i - %p\n"),
        ACE_TEXT("cannot get local addr")), false);
    }
    config_i_->local_address_.set_port_number(address.get_port_number());
  }

  create_reactor_task();
  return true;
}

void
RtpsUdpTransport::shutdown_i()
{
  if (!link_.is_nil()) {
    link_->transport_shutdown();
  }
  link_ = 0;
  config_i_ = 0;
}

void
RtpsUdpTransport::release_datalink_i(DataLink*, bool /*release_pending*/)
{
  link_ = 0;
}


} // namespace DCPS
} // namespace OpenDDS
