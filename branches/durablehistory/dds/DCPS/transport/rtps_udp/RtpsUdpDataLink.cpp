/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "RtpsUdpDataLink.h"
#include "RtpsUdpTransport.h"
#include "RtpsUdpInst.h"

#include "dds/DCPS/transport/framework/TransportCustomizedElement.h"
#include "dds/DCPS/transport/framework/TransportSendElement.h"
#include "dds/DCPS/transport/framework/TransportSendControlElement.h"

#include "dds/DCPS/RTPS/BaseMessageUtils.h"
#include "dds/DCPS/RTPS/RtpsMessageTypesTypeSupportImpl.h"
#include "dds/DCPS/RTPS/BaseMessageTypes.h"
#include "dds/DCPS/RTPS/MessageTypes.h"

#include "ace/Default_Constants.h"
#include "ace/Log_Msg.h"
#include "ace/Message_Block.h"
#include "ace/Reactor.h"
#include "ace/OS_NS_sys_socket.h" // For setsockopt()

#include <cstring>

#ifndef __ACE_INLINE__
# include "RtpsUdpDataLink.inl"
#endif  /* __ACE_INLINE__ */

namespace {

/// Return the number of CORBA::Longs required for the bitmap representation of
/// sequence numbers between low and high, inclusive (maximum 8 longs).
CORBA::ULong
bitmap_num_longs(const OpenDDS::DCPS::SequenceNumber& low,
                 const OpenDDS::DCPS::SequenceNumber& high)
{
  return std::min(CORBA::ULong(8),
                  CORBA::ULong((high.getValue() - low.getValue() + 32) / 32));
}

}

namespace OpenDDS {
namespace DCPS {

RtpsUdpDataLink::RtpsUdpDataLink(RtpsUdpTransport* transport,
                                 const GuidPrefix_t& local_prefix,
                                 RtpsUdpInst* config,
                                 TransportReactorTask* reactor_task)
  : DataLink(transport, // 3 data link "attributes", below, are unused
             0,         // priority
             false,     // is_loopback
             false),    // is_active
    config_(config),
    reactor_task_(reactor_task, false),
    rtps_customized_element_allocator_(40, sizeof(RtpsCustomizedElement)),
    multi_buff_(this, config->nak_depth_),
    handshake_condition_(lock_),
    nack_reply_(this, &RtpsUdpDataLink::send_nack_replies,
                config->nak_response_delay_),
    heartbeat_reply_(this, &RtpsUdpDataLink::send_heartbeat_replies,
                     config->heartbeat_response_delay_),
    heartbeat_(this)
{
  std::memcpy(local_prefix_, local_prefix, sizeof(GuidPrefix_t));
}

bool
RtpsUdpDataLink::open(const ACE_SOCK_Dgram& unicast_socket)
{
  unicast_socket_ = unicast_socket;

#ifdef ACE_HAS_IPV6
  ACE_INET_Addr uni_addr;
  if (0 != unicast_socket_.get_local_addr(uni_addr)) {
    VDBG((LM_WARNING, "(%P|%t) RtpsUdpDataLink::open: "
          "ACE_SOCK_Dgram::get_local_addr %p\n", ACE_TEXT("")));
  } else {
    unicast_socket_type_ = uni_addr.get_type();
    const unsigned short any_port = 0;
    if (unicast_socket_type_ == AF_INET6) {
      const ACE_UINT32 any_addr = INADDR_ANY;
      ACE_INET_Addr alt_addr(any_port, any_addr);
      if (0 != ipv6_alternate_socket_.open(alt_addr)) {
        VDBG((LM_WARNING, "(%P|%t) RtpsUdpDataLink::open: "
              "ACE_SOCK_Dgram::open %p\n", ACE_TEXT("alternate IPv4")));
      }
    } else {
      ACE_INET_Addr alt_addr(any_port, ACE_IPV6_ANY, AF_INET6);
      if (0 != ipv6_alternate_socket_.open(alt_addr)) {
        VDBG((LM_WARNING, "(%P|%t) RtpsUdpDataLink::open: "
              "ACE_SOCK_Dgram::open %p\n", ACE_TEXT("alternate IPv6")));
      }
    }
  }

  /// FUTURE: Set TTL on the alternate sockets.
#endif

  if (config_->use_multicast_) {
    const std::string& net_if = config_->multicast_interface_;
    if (multicast_socket_.join(config_->multicast_group_address_, 1,
                               net_if.empty() ? 0 :
                               ACE_TEXT_CHAR_TO_TCHAR(net_if.c_str())) != 0) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("RtpsUdpDataLink::open: ")
                        ACE_TEXT("ACE_SOCK_Dgram_Mcast::join failed: %m\n")),
                       false);
    }
  }

  ACE_HANDLE handle = unicast_socket_.get_handle();
  char ttl = static_cast<char>(config_->ttl_);

  if (0 != ACE_OS::setsockopt(handle,
                              IPPROTO_IP,
                              IP_MULTICAST_TTL,
                              &ttl,
                              sizeof(ttl))) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("RtpsUdpDataLink::open: ")
                      ACE_TEXT("failed to set TTL: %d %p\n"),
                      config_->ttl_,
                      ACE_TEXT("ACE_OS::setsockopt(TTL)")),
                     false);
  }

  send_strategy_->send_buffer(&multi_buff_);

  // Set up info_reply_ messages for use with ACKNACKS
  using namespace OpenDDS::RTPS;
  info_reply_.smHeader.submessageId = INFO_REPLY;
  info_reply_.smHeader.flags = 1 /*FLAG_E*/;
  info_reply_.unicastLocatorList.length(1);
  info_reply_.unicastLocatorList[0].kind =
    (config_->local_address_.get_type() == AF_INET6)
    ? LOCATOR_KIND_UDPv6 : LOCATOR_KIND_UDPv4;
  info_reply_.unicastLocatorList[0].port =
    config_->local_address_.get_port_number();
  RTPS::address_to_bytes(info_reply_.unicastLocatorList[0].address,
                         config_->local_address_);
  if (config_->use_multicast_) {
    info_reply_.smHeader.flags |= 2 /*FLAG_M*/;
    info_reply_.multicastLocatorList.length(1);
    info_reply_.multicastLocatorList[0].kind =
      (config_->multicast_group_address_.get_type() == AF_INET6)
      ? LOCATOR_KIND_UDPv6 : LOCATOR_KIND_UDPv4;
    info_reply_.multicastLocatorList[0].port =
      config_->multicast_group_address_.get_port_number();
    RTPS::address_to_bytes(info_reply_.multicastLocatorList[0].address,
                           config_->multicast_group_address_);
  }

  size_t size = 0, padding = 0;
  gen_find_size(info_reply_, size, padding);
  info_reply_.smHeader.submessageLength =
    static_cast<CORBA::UShort>(size + padding) - SMHDR_SZ;

  if (start(static_rchandle_cast<TransportSendStrategy>(send_strategy_),
            static_rchandle_cast<TransportStrategy>(recv_strategy_)) != 0) {
    stop_i();
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("UdpDataLink::open: start failed!\n")),
                     false);
  }

  return true;
}

#ifdef ACE_HAS_IPV6
ACE_SOCK_Dgram&
RtpsUdpDataLink::socket_for(int address_type)
{
  return (address_type == unicast_socket_type_)
    ? unicast_socket_
    : ipv6_alternate_socket_;
}
#endif

void
RtpsUdpDataLink::add_locator(const RepoId& remote_id,
                             const ACE_INET_Addr& address,
                             bool requires_inline_qos)
{
  ACE_GUARD(ACE_Thread_Mutex, g, lock_);
  locators_[remote_id] = RemoteInfo(address, requires_inline_qos);
}

void
RtpsUdpDataLink::get_locators(const RepoId& local_id,
                              std::set<ACE_INET_Addr>& addrs) const
{
  using std::map;
  typedef map<RepoId, RemoteInfo, GUID_tKeyLessThan>::const_iterator iter_t;

  if (local_id == GUID_UNKNOWN) {
    for (iter_t iter = locators_.begin(); iter != locators_.end(); ++iter) {
      addrs.insert(iter->second.addr_);
    }
    return;
  }

  const GUIDSeq_var peers = peer_ids(local_id);
  if (!peers.ptr()) {
    return;
  }
  for (CORBA::ULong i = 0; i < peers->length(); ++i) {
    const ACE_INET_Addr addr = get_locator(peers[i]);
    if (addr != ACE_INET_Addr()) {
      addrs.insert(addr);
    }
  }
}

ACE_INET_Addr
RtpsUdpDataLink::get_locator(const RepoId& remote_id) const
{
  using std::map;
  typedef map<RepoId, RemoteInfo, GUID_tKeyLessThan>::const_iterator iter_t;
  const iter_t iter = locators_.find(remote_id);
  if (iter == locators_.end()) {
    const GuidConverter conv(remote_id);
    ACE_DEBUG((LM_ERROR, "(%P|%t) RtpsUdpDataLink::get_locator_i() - "
      "no locator found for peer %C\n", std::string(conv).c_str()));
    return ACE_INET_Addr();
  }
  return iter->second.addr_;
}

void
RtpsUdpDataLink::associated(const RepoId& local_id, const RepoId& remote_id,
                            bool local_reliable, bool remote_reliable,
                            bool local_durable, bool remote_durable)
{
  if (!local_reliable) {
    return;
  }

  bool enable_heartbeat = false;

  ACE_GUARD(ACE_Thread_Mutex, g, lock_);
  const GuidConverter conv(local_id);
  const EntityKind kind = conv.entityKind();
  if (kind == KIND_WRITER && remote_reliable) {
    RtpsWriter& w = writers_[local_id];
    w.remote_readers_[remote_id].durable_ = remote_durable;
    w.durable_ = local_durable;
    enable_heartbeat = true;

  } else if (kind == KIND_READER) {
    RtpsReaderMap::iterator rr = readers_.find(local_id);
    if (rr == readers_.end()) {
      rr = readers_.insert(RtpsReaderMap::value_type(local_id, RtpsReader()))
        .first;
      rr->second.durable_ = local_durable;
    }
    rr->second.remote_writers_[remote_id];
    reader_index_.insert(RtpsReaderIndex::value_type(remote_id, rr));
  }

  g.release();
  if (enable_heartbeat) {
    heartbeat_.enable();
  }
}

bool
RtpsUdpDataLink::wait_for_handshake(const RepoId& local_id,
                                    const RepoId& remote_id)
{
  if (0 == std::memcmp(local_id.guidPrefix, remote_id.guidPrefix,
                       sizeof(GuidPrefix_t))) {
    return true; // no wait for "loopback" connection
  }
  ACE_Time_Value abs_timeout = ACE_OS::gettimeofday()
    + config_->handshake_timeout_;
  ACE_GUARD_RETURN(ACE_Thread_Mutex, g, lock_, false);
  while (!handshake_done(local_id, remote_id)) {
    if (handshake_condition_.wait(&abs_timeout) == -1) {
      return false;
    }
  }
  return true;
}

bool
RtpsUdpDataLink::handshake_done(const RepoId& local_id, const RepoId& remote_id)
{
  const GuidConverter conv(local_id);
  const EntityKind kind = conv.entityKind();
  if (kind == KIND_WRITER) {
    RtpsWriterMap::iterator rw = writers_.find(local_id);
    if (rw == writers_.end()) {
      return true; // not reliable, no handshaking
    }
    ReaderInfoMap::iterator ri = rw->second.remote_readers_.find(remote_id);
    if (ri == rw->second.remote_readers_.end()) {
      return true; // not reliable, no handshaking
    }
    return ri->second.handshake_done_;

  } else if (kind == KIND_READER) {
    return true; // no handshaking for local reader
  }
  return false;
}

void
RtpsUdpDataLink::send_i(TransportQueueElement* element, bool relink)
{
  // Lock here to maintain the locking order:
  // RtpsUdpDataLink before RtpsUdpSendStrategy
  // which is required for resending due to nacks
  ACE_GUARD(ACE_Thread_Mutex, g, lock_);
  DataLink::send_i(element, relink);
}

void
RtpsUdpDataLink::release_reservations_i(const RepoId& remote_id,
                                        const RepoId& local_id)
{
  ACE_GUARD(ACE_Thread_Mutex, g, lock_);
  using std::pair;
  const GuidConverter conv(local_id);
  const EntityKind kind = conv.entityKind();
  if (kind == KIND_WRITER) {
    const RtpsWriterMap::iterator rw = writers_.find(local_id);

    if (rw != writers_.end()) {
      rw->second.remote_readers_.erase(remote_id);

      if (rw->second.remote_readers_.empty()) {
        writers_.erase(rw);
      }
    }

  } else if (kind == KIND_READER) {
    const RtpsReaderMap::iterator rr = readers_.find(local_id);

    if (rr != readers_.end()) {
      rr->second.remote_writers_.erase(remote_id);

      for (pair<RtpsReaderIndex::iterator, RtpsReaderIndex::iterator> iters =
             reader_index_.equal_range(remote_id);
           iters.first != iters.second;) {
        if (iters.first->second == rr) {
          reader_index_.erase(iters.first++);
        } else {
          ++iters.first;
        }
      }

      if (rr->second.remote_writers_.empty()) {
        readers_.erase(rr);
      }
    }
  }
}

void
RtpsUdpDataLink::stop_i()
{
  nack_reply_.cancel();
  heartbeat_reply_.cancel();
  heartbeat_.disable();
  unicast_socket_.close();
  multicast_socket_.close();
}


// Implementing MultiSendBuffer nested class

void
RtpsUdpDataLink::MultiSendBuffer::retain_all(RepoId pub_id)
{
  ACE_GUARD(ACE_Thread_Mutex, g, outer_->lock_);
  const RtpsWriterMap::iterator wi = outer_->writers_.find(pub_id);
  if (wi != outer_->writers_.end() && !wi->second.send_buff_.is_nil()) {
    wi->second.send_buff_->retain_all(pub_id);
  }
}

void
RtpsUdpDataLink::MultiSendBuffer::insert(SequenceNumber /*transport_seq*/,
                                         TransportSendStrategy::QueueType* q,
                                         ACE_Message_Block* chain)
{
  // Called from TransportSendStrategy::send_packet().
  // RtpsUdpDataLink is already locked.
  const TransportQueueElement* const tqe = q->peek();
  const SequenceNumber seq = tqe->sequence();
  if (seq == SequenceNumber::SEQUENCENUMBER_UNKNOWN()) {
    return;
  }

  const RepoId pub_id = tqe->publication_id();

  const RtpsWriterMap::iterator wi = outer_->writers_.find(pub_id);
  if (wi == outer_->writers_.end()) {
    return; // this datawriter is not reliable
  }

  RcHandle<SingleSendBuffer>& send_buff = wi->second.send_buff_;

  if (send_buff.is_nil()) {
    send_buff = new SingleSendBuffer(outer_->config_->nak_depth_, 1 /*mspp*/);
    send_buff->bind(outer_->send_strategy_.in());
  }

  if (Transport_debug_level > 5) {
    const GuidConverter pub(pub_id);
    ACE_DEBUG((LM_DEBUG, "(%P|%t) RtpsUdpDataLink::MultiSendBuffer::insert() - "
      "pub_id %C seq %q frag %d\n", std::string(pub).c_str(), seq.getValue(),
      (int)tqe->is_fragment()));
  }

  if (tqe->is_fragment()) {
    const RtpsCustomizedElement* const rce =
      dynamic_cast<const RtpsCustomizedElement*>(tqe);
    if (rce) {
      send_buff->insert_fragment(seq, rce->last_fragment(), q, chain);
    } else if (Transport_debug_level) {
      const GuidConverter pub(pub_id);
      ACE_ERROR((LM_ERROR, "(%P|%t) RtpsUdpDataLink::MultiSendBuffer::insert()"
        " - ERROR: couldn't get fragment number for pub_id %C seq %q\n",
        std::string(pub).c_str(), seq.getValue()));
    }
  } else {
    send_buff->insert(seq, q, chain);
  }
}


// Support for the send() data handling path
namespace {
  ACE_Message_Block* submsgs_to_msgblock(const RTPS::SubmessageSeq& subm)
  {
    size_t size = 0, padding = 0;
    for (CORBA::ULong i = 0; i < subm.length(); ++i) {
      if ((size + padding) % 4) {
        padding += 4 - ((size + padding) % 4);
      }
      gen_find_size(subm[i], size, padding);
    }

    ACE_Message_Block* hdr = new ACE_Message_Block(size + padding);

    for (CORBA::ULong i = 0; i < subm.length(); ++i) {
      // byte swapping is handled in the operator<<() implementation
      Serializer ser(hdr, false, Serializer::ALIGN_CDR);
      ser << subm[i];
      const size_t len = hdr->length();
      if (len % 4) {
        hdr->wr_ptr(4 - (len % 4));
      }
    }
    return hdr;
  }
}

TransportQueueElement*
RtpsUdpDataLink::customize_queue_element(TransportQueueElement* element)
{
  const ACE_Message_Block* msg = element->msg();
  if (!msg) {
    return element;
  }

  const RepoId pub_id = element->publication_id();

  ACE_GUARD_RETURN(ACE_Thread_Mutex, g, lock_, 0);

  RTPS::SubmessageSeq subm;

  const RtpsWriterMap::iterator rw = writers_.find(pub_id);

  bool gap_ok = true;
  if (rw != writers_.end() && !rw->second.remote_readers_.empty()) {
    for (ReaderInfoMap::iterator ri = rw->second.remote_readers_.begin();
         ri != rw->second.remote_readers_.end(); ++ri) {
      if (ri->second.expecting_durable_data()) {
        // Can't add an in-line GAP if some Data Reader is expecting durable
        // data, the GAP could cause that Data Reader to ignore the durable
        // data.  The other readers will eventually learn about the GAP by
        // sending an ACKNACK and getting a GAP reply.
        gap_ok = false;
        break;
      }
    }
  }

  if (gap_ok) {
    add_gap_submsg(subm, *element);
  }

  const SequenceNumber seq = element->sequence();
  if (rw != writers_.end() && seq != SequenceNumber::SEQUENCENUMBER_UNKNOWN()) {
    rw->second.expected_ = seq;
    ++rw->second.expected_;
  }

  TransportSendElement* tse = dynamic_cast<TransportSendElement*>(element);
  TransportCustomizedElement* tce =
    dynamic_cast<TransportCustomizedElement*>(element);
  TransportSendControlElement* tsce =
    dynamic_cast<TransportSendControlElement*>(element);

  ACE_Message_Block* data = 0;
  bool durable = false;

  // Based on the type of 'element', find and duplicate the data payload
  // continuation block.
  if (tsce) {        // Control message
    if (RtpsSampleHeader::control_message_supported(tsce->header().message_id_)) {
      data = msg->cont()->duplicate();
      // Create RTPS Submessage(s) in place of the OpenDDS DataSampleHeader
      RtpsSampleHeader::populate_data_control_submessages(
                subm, *tsce, this->requires_inline_qos(pub_id));
    } else {
      element->data_dropped(true /*dropped_by_transport*/);
      return 0;
    }

  } else if (tse) {  // Basic data message
    // {DataSampleHeader} -> {Data Payload}
    data = msg->cont()->duplicate();
    const DataSampleElement* dsle = tse->sample();
    // Create RTPS Submessage(s) in place of the OpenDDS DataSampleHeader
    RtpsSampleHeader::populate_data_sample_submessages(
              subm, *dsle, this->requires_inline_qos(pub_id));
    durable = dsle->get_header().historic_sample_;

  } else if (tce) {  // Customized data message
    // {DataSampleHeader} -> {Content Filtering GUIDs} -> {Data Payload}
    data = msg->cont()->cont()->duplicate();
    const DataSampleElement* dsle = tce->original_send_element()->sample();
    // Create RTPS Submessage(s) in place of the OpenDDS DataSampleHeader
    RtpsSampleHeader::populate_data_sample_submessages(
              subm, *dsle, this->requires_inline_qos(pub_id));
    durable = dsle->get_header().historic_sample_;

  } else {
    return element;
  }

  ACE_Message_Block* hdr = submsgs_to_msgblock(subm);
  hdr->cont(data);
  RtpsCustomizedElement* rtps =
    RtpsCustomizedElement::alloc(element, hdr,
      &this->rtps_customized_element_allocator_);

  // Handle durability resends
  if (durable && rw != writers_.end()) {
    const RepoId sub = element->subscription_id();
    if (sub != GUID_UNKNOWN) {
      ReaderInfoMap::iterator ri = rw->second.remote_readers_.find(sub);
      if (ri != rw->second.remote_readers_.end()) {
        ri->second.durable_data_[rtps->sequence()] = rtps;
        ri->second.durable_timestamp_ = ACE_OS::gettimeofday();
        if (Transport_debug_level > 3) {
          const GuidConverter conv(pub_id);
          ACE_DEBUG((LM_DEBUG,
            "(%P|%t) RtpsUdpDataLink::customize_queue_element() - "
            "storing durable data for local %C\n", std::string(conv).c_str()));
        }
        return 0;
      }
    }
  } else if (durable && Transport_debug_level) {
    const GuidConverter conv(pub_id);
    ACE_DEBUG((LM_ERROR,
      "(%P|%t) RtpsUdpDataLink::customize_queue_element() - "
      "WARNING: no RtpsWriter to store durable data for local %C\n",
      std::string(conv).c_str()));
  }

  return rtps;
}

bool
RtpsUdpDataLink::requires_inline_qos(const PublicationId& pub_id)
{
  if (this->force_inline_qos_) {
    // Force true for testing purposes
    return true;
  } else {
    const GUIDSeq_var peers = peer_ids(pub_id);
    if (!peers.ptr()) {
      return false;
    }
    typedef std::map<RepoId, RemoteInfo, GUID_tKeyLessThan>::iterator iter_t;
    for (CORBA::ULong i = 0; i < peers->length(); ++i) {
      const iter_t iter = locators_.find(peers[i]);
      if (iter != locators_.end() && iter->second.requires_inline_qos_) {
        return true;
      }
    }
    return false;
  }
}

bool RtpsUdpDataLink::force_inline_qos_ = false;

void
RtpsUdpDataLink::add_gap_submsg(RTPS::SubmessageSeq& msg,
                                const TransportQueueElement& tqe)
{
  // These are the GAP submessages that we'll send directly in-line with the
  // DATA when we notice that the DataWriter has deliberately skipped seq #s.
  // There are other GAP submessages generated in response to reader ACKNACKS,
  // see send_nack_replies().
  using namespace OpenDDS::RTPS;

  const SequenceNumber seq = tqe.sequence();
  const RepoId pub = tqe.publication_id();
  if (seq == SequenceNumber::SEQUENCENUMBER_UNKNOWN() || pub == GUID_UNKNOWN
      || tqe.subscription_id() != GUID_UNKNOWN) {
    return;
  }

  const RtpsWriterMap::iterator wi = writers_.find(pub);
  if (wi == writers_.end()) {
    return; // not a reliable writer, does not send GAPs
  }

  RtpsWriter& rw = wi->second;

  if (seq != rw.expected_) {
    SequenceNumber firstMissing = rw.expected_;

    // RTPS v2.1 8.3.7.4: the Gap sequence numbers are those in the range
    // [gapStart, gapListBase) and those in the SNSet.
    const SequenceNumber_t gapStart = {firstMissing.getHigh(),
                                       firstMissing.getLow()},
                           gapListBase = {seq.getHigh(),
                                          seq.getLow()};

    // We are not going to enable any bits in the "bitmap" of the SNSet,
    // but the "numBits" and the bitmap.length must both be > 0.
    LongSeq8 bitmap;
    bitmap.length(1);
    bitmap[0] = 0;

    GapSubmessage gap = {
      {GAP, 1 /*FLAG_E*/, 0 /*length determined below*/},
      ENTITYID_UNKNOWN, // readerId: applies to all matched readers
      pub.entityId,
      gapStart,
      {gapListBase, 1, bitmap}
    };

    size_t size = 0, padding = 0;
    gen_find_size(gap, size, padding);
    gap.smHeader.submessageLength =
      static_cast<CORBA::UShort>(size + padding) - SMHDR_SZ;

    const CORBA::ULong i = msg.length();
    msg.length(i + 1);
    msg[i].gap_sm(gap);
  }
}


// DataReader's side of Reliability

void
RtpsUdpDataLink::received(const RTPS::DataSubmessage& data,
                          const GuidPrefix_t& src_prefix)
{
  datareader_dispatch(data, src_prefix, &RtpsUdpDataLink::process_data_i);
}

bool
RtpsUdpDataLink::process_data_i(const RTPS::DataSubmessage& data,
                                const RepoId& src,
                                RtpsReaderMap::value_type& rr)
{
  const WriterInfoMap::iterator wi = rr.second.remote_writers_.find(src);
  if (wi != rr.second.remote_writers_.end()) {
    WriterInfo& info = wi->second;
    SequenceNumber seq;
    seq.setValue(data.writerSN.high, data.writerSN.low);
    info.frags_.erase(seq);
    const RepoId& readerId = rr.first;
    if (info.recvd_.contains(seq)) {
      recv_strategy_->withhold_data_from(readerId);
    } else if (info.recvd_.disjoint() ||
        (!info.recvd_.empty() && info.recvd_.cumulative_ack() != seq.previous())
        || (rr.second.durable_ && !info.recvd_.empty() && info.recvd_.low() > 1)
        || (rr.second.durable_ && info.recvd_.empty() && seq > 1)) {
      const ReceivedDataSample* sample =
        recv_strategy_->withhold_data_from(readerId);
      info.held_.insert(std::make_pair(seq, *sample));
    }
    info.recvd_.insert(seq);
    deliver_held_data(readerId, info, rr.second.durable_);
  }
  return false;
}

void
RtpsUdpDataLink::deliver_held_data(const RepoId& readerId, WriterInfo& info,
                                   bool durable)
{
  if (durable && (info.recvd_.empty() || info.recvd_.low() > 1)) return;
  const SequenceNumber ca = info.recvd_.cumulative_ack();
  typedef std::map<SequenceNumber, ReceivedDataSample>::iterator iter;
  const iter end = info.held_.upper_bound(ca);
  for (iter it = info.held_.begin(); it != end; /*increment in loop body*/) {
    data_received(it->second, readerId);
    info.held_.erase(it++);
  }
}

void
RtpsUdpDataLink::received(const RTPS::GapSubmessage& gap,
                          const GuidPrefix_t& src_prefix)
{
  datareader_dispatch(gap, src_prefix, &RtpsUdpDataLink::process_gap_i);
}

bool
RtpsUdpDataLink::process_gap_i(const RTPS::GapSubmessage& gap,
                               const RepoId& src, RtpsReaderMap::value_type& rr)
{
  const WriterInfoMap::iterator wi = rr.second.remote_writers_.find(src);
  if (wi != rr.second.remote_writers_.end()) {
    SequenceRange sr;
    sr.first.setValue(gap.gapStart.high, gap.gapStart.low);
    SequenceNumber base;
    base.setValue(gap.gapList.bitmapBase.high, gap.gapList.bitmapBase.low);
    sr.second = base.previous();
    wi->second.recvd_.insert(sr);
    wi->second.recvd_.insert(base, gap.gapList.numBits,
                             gap.gapList.bitmap.get_buffer());
    deliver_held_data(rr.first, wi->second, rr.second.durable_);
    //FUTURE: to support wait_for_acks(), notify DCPS layer of the GAP
  }
  return false;
}

void
RtpsUdpDataLink::received(const RTPS::HeartBeatSubmessage& heartbeat,
                          const GuidPrefix_t& src_prefix)
{
  datareader_dispatch(heartbeat, src_prefix,
                      &RtpsUdpDataLink::process_heartbeat_i);
}

bool
RtpsUdpDataLink::process_heartbeat_i(const RTPS::HeartBeatSubmessage& heartbeat,
                                     const RepoId& src,
                                     RtpsReaderMap::value_type& rr)
{
  const WriterInfoMap::iterator wi = rr.second.remote_writers_.find(src);
  if (wi == rr.second.remote_writers_.end()) {
    // we may not be associated yet, even if the writer thinks we are
    return false;
  }

  WriterInfo& info = wi->second;

  if (heartbeat.count.value <= info.heartbeat_recvd_count_) {
    return false;
  }
  info.heartbeat_recvd_count_ = heartbeat.count.value;

  SequenceNumber& first = info.hb_range_.first;
  first.setValue(heartbeat.firstSN.high, heartbeat.firstSN.low);
  SequenceNumber& last = info.hb_range_.second;
  last.setValue(heartbeat.lastSN.high, heartbeat.lastSN.low);
  static const SequenceNumber starting, zero = SequenceNumber::ZERO();

  DisjointSequence& recvd = info.recvd_;
  if (!rr.second.durable_ && info.initial_hb_) {
    if (last.getValue() < starting.getValue()) {
      // this is an invalid heartbeat -- last must be positive
      return false;
    }
    // For the non-durable reader, the first received HB or DATA establishes
    // a baseline of the lowest sequence number we'd ever need to NACK.
    if (recvd.empty() || recvd.low() >= last) {
      recvd.insert(SequenceRange(zero,
                                 (last > starting) ? last.previous() : zero));
    } else {
      recvd.insert(SequenceRange(zero, recvd.low()));
    }
  } else if (!recvd.empty()) {
    // All sequence numbers below 'first' should not be NACKed.
    // The value of 'first' may not decrease with subsequent HBs.
    recvd.insert(SequenceRange(zero,
                               (first > starting) ? first.previous() : zero));
  }

  deliver_held_data(rr.first, info, rr.second.durable_);

  //FUTURE: to support wait_for_acks(), notify DCPS layer of the sequence
  //        numbers we no longer expect to receive due to HEARTBEAT

  info.initial_hb_ = false;

  const bool final = heartbeat.smHeader.flags & 2 /* FLAG_F */,
    liveliness = heartbeat.smHeader.flags & 4 /* FLAG_L */;

  if (!final || (!liveliness && (info.should_nack() ||
      (rr.second.durable_ && recvd.empty()) ||
      recv_strategy_->has_fragments(info.hb_range_, wi->first)))) {
    info.ack_pending_ = true;
    return true; // timer will invoke send_heartbeat_replies()
  }

  //FUTURE: support assertion of liveliness for MANUAL_BY_TOPIC
  return false;
}

bool
RtpsUdpDataLink::WriterInfo::should_nack() const
{
  if (recvd_.disjoint() && recvd_.cumulative_ack() < hb_range_.second) {
    return true;
  }
  if (!recvd_.empty()) {
    return recvd_.high() < hb_range_.second;
  }
  return false;
}

void
RtpsUdpDataLink::send_heartbeat_replies() // from DR to DW
{
  using namespace OpenDDS::RTPS;
  ACE_GUARD(ACE_Thread_Mutex, g, lock_);
  for (RtpsReaderMap::iterator rr = readers_.begin(); rr != readers_.end();
       ++rr) {
    WriterInfoMap& writers = rr->second.remote_writers_;
    for (WriterInfoMap::iterator wi = writers.begin(); wi != writers.end();
         ++wi) {

      // if we have some negative acknowledgements, we'll ask for a reply
      DisjointSequence& recvd = wi->second.recvd_;
      const bool nack = wi->second.should_nack() ||
                        (rr->second.durable_ && recvd.empty());
      bool final = !nack;

      if (wi->second.ack_pending_ || nack) {
        const bool prev_empty =
          rr->second.durable_ && wi->second.ack_pending_ && !nack;
        wi->second.ack_pending_ = false;

        SequenceNumber ack;
        CORBA::ULong num_bits = 1;
        LongSeq8 bitmap;
        bitmap.length(1);
        bitmap[0] = 0;

        const SequenceNumber& hb_low = wi->second.hb_range_.first;
        const SequenceNumber& hb_high = wi->second.hb_range_.second;
        const SequenceNumber::Value hb_low_val = hb_low.getValue(),
          hb_high_val = hb_high.getValue();

        if (recvd.empty()) {
          // Nack the entire heartbeat range.  Only reached when durable.
          ack = hb_low;
          bitmap.length(bitmap_num_longs(ack, hb_high));
          const CORBA::ULong idx = (hb_high_val > hb_low_val + 255)
                                    ? 255
                                    : CORBA::ULong(hb_high_val - hb_low_val);
          DisjointSequence::fill_bitmap_range(0, idx,
                                              bitmap.get_buffer(),
                                              bitmap.length(), num_bits);
        } else if (prev_empty && recvd.low() > hb_low) {
          // Nack the range between the heartbeat low and the recvd low.
          ack = hb_low;
          const SequenceNumber& rec_low = recvd.low();
          const SequenceNumber::Value rec_low_val = rec_low.getValue();
          bitmap.length(bitmap_num_longs(ack, rec_low));
          const CORBA::ULong idx = (rec_low_val > hb_low_val + 255)
                                   ? 255
                                   : CORBA::ULong(rec_low_val - hb_low_val);
          DisjointSequence::fill_bitmap_range(0, idx,
                                              bitmap.get_buffer(),
                                              bitmap.length(), num_bits);

        } else {
          ack = ++SequenceNumber(recvd.cumulative_ack());
          if (recvd.low().getValue() > 1) {
            // since the "ack" really is cumulative, we need to make
            // sure that a lower discontinuity is not possible later
            recvd.insert(SequenceRange(SequenceNumber::ZERO(), recvd.low()));
          }

          if (recvd.disjoint()) {
            bitmap.length(bitmap_num_longs(ack, recvd.last_ack().previous()));
            recvd.to_bitmap(bitmap.get_buffer(), bitmap.length(),
                            num_bits, true);
          }
        }

        const SequenceNumber::Value ack_val = ack.getValue();

        if (!recvd.empty() && hb_high > recvd.high()) {
          const SequenceNumber eff_high =
            (hb_high <= ack_val + 255) ? hb_high : (ack_val + 255);
          const SequenceNumber::Value eff_high_val = eff_high.getValue();
          // Nack the range between the received high and the effective high.
          const CORBA::ULong old_len = bitmap.length(),
            new_len = bitmap_num_longs(ack, eff_high);
          if (new_len > old_len) {
            bitmap.length(new_len);
            for (CORBA::ULong i = old_len; i < new_len; ++i) {
              bitmap[i] = 0;
            }
          }
          const CORBA::ULong idx_hb_high = CORBA::ULong(eff_high_val - ack_val),
            idx_recv_high = recvd.disjoint() ?
                            CORBA::ULong(recvd.high().getValue() - ack_val) : 0;
          DisjointSequence::fill_bitmap_range(idx_recv_high, idx_hb_high,
                                              bitmap.get_buffer(), new_len,
                                              num_bits);
        }

        // If the receive strategy is holding any fragments, those should
        // not be "nacked" in the ACKNACK reply.  They will be accounted for
        // in the NACK_FRAG(s) instead.
        bool frags_modified =
          recv_strategy_->remove_frags_from_bitmap(bitmap.get_buffer(),
                                                   num_bits, ack, wi->first);
        if (frags_modified && !final) { // change to final if bitmap is empty
          final = true;
          for (CORBA::ULong i = 0; i < bitmap.length(); ++i) {
            if ((i + 1) * 32 <= num_bits) {
              if (bitmap[i]) {
                final = false;
                break;
              }
            } else {
              if ((0xffffffff << (32 - (num_bits % 32))) & bitmap[i]) {
                final = false;
                break;
              }
            }
          }
        }

        AckNackSubmessage acknack = {
          {ACKNACK,
           CORBA::Octet(1 /*FLAG_E*/ | (final ? 2 /*FLAG_F*/ : 0)),
           0 /*length*/},
          rr->first.entityId,
          wi->first.entityId,
          { // SequenceNumberSet: acking bitmapBase - 1
            {ack.getHigh(), ack.getLow()},
            num_bits, bitmap
          },
          {++wi->second.acknack_count_}
        };

        size_t size = 0, padding = 0;
        gen_find_size(acknack, size, padding);
        acknack.smHeader.submessageLength =
          static_cast<CORBA::UShort>(size + padding) - SMHDR_SZ;
        InfoDestinationSubmessage info_dst = {
          {INFO_DST, 1 /*FLAG_E*/, INFO_DST_SZ},
          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
        };
        gen_find_size(info_dst, size, padding);

        std::vector<NackFragSubmessage> nack_frags;
        size += generate_nack_frags(nack_frags, wi->second, wi->first);

        ACE_Message_Block mb_acknack(size + padding); //FUTURE: allocators?
        // byte swapping is handled in the operator<<() implementation
        Serializer ser(&mb_acknack, false, Serializer::ALIGN_CDR);
        std::memcpy(info_dst.guidPrefix, wi->first.guidPrefix,
                    sizeof(GuidPrefix_t));
        ser << info_dst;
        // Interoperability note: we used to insert "info_reply_" here, but
        // testing indicated that other DDS implementations didn't accept it.
        ser << acknack;
        for (size_t i = 0; i < nack_frags.size(); ++i) {
          nack_frags[i].readerId = rr->first.entityId;
          nack_frags[i].writerId = wi->first.entityId;
          ser << nack_frags[i]; // always 4-byte aligned
        }

        if (!locators_.count(wi->first)) {
          if (Transport_debug_level) {
            const GuidConverter conv(wi->first);
            ACE_DEBUG((LM_ERROR,
              "(%P|%t) RtpsUdpDataLink::send_heartbeat_replies() - "
              "no locator for remote %C\n", std::string(conv).c_str()));
          }
        } else {
          send_strategy_->send_rtps_control(mb_acknack,
                                            locators_[wi->first].addr_);
        }
      }
    }
  }
}

size_t
RtpsUdpDataLink::generate_nack_frags(std::vector<RTPS::NackFragSubmessage>& nf,
                                     WriterInfo& wi, const RepoId& pub_id)
{
  typedef std::map<SequenceNumber, RTPS::FragmentNumber_t>::iterator iter_t;
  typedef RtpsUdpReceiveStrategy::FragmentInfo::value_type Frag_t;
  RtpsUdpReceiveStrategy::FragmentInfo frag_info;

  // Populate frag_info with two possible sources of NackFrags:
  // 1. sequence #s in the reception gaps that we have partially received
  std::vector<SequenceRange> missing = wi.recvd_.missing_sequence_ranges();
  for (size_t i = 0; i < missing.size(); ++i) {
    recv_strategy_->has_fragments(missing[i], pub_id, &frag_info);
  }
  // 1b. larger than the last received seq# but less than the heartbeat.lastSN
  if (!wi.recvd_.empty()) {
    const SequenceRange range(wi.recvd_.high(), wi.hb_range_.second);
    recv_strategy_->has_fragments(range, pub_id, &frag_info);
  }
  for (size_t i = 0; i < frag_info.size(); ++i) {
    // If we've received a HeartbeatFrag, we know the last (available) frag #
    const iter_t heartbeat_frag = wi.frags_.find(frag_info[i].first);
    if (heartbeat_frag != wi.frags_.end()) {
      extend_bitmap_range(frag_info[i].second, heartbeat_frag->second.value);
    }
  }

  // 2. sequence #s outside the recvd_ gaps for which we have a HeartbeatFrag
  const iter_t low = wi.frags_.lower_bound(wi.recvd_.cumulative_ack()),
              high = wi.frags_.upper_bound(wi.recvd_.last_ack()),
               end = wi.frags_.end();
  for (iter_t iter = wi.frags_.begin(); iter != end; ++iter) {
    if (iter == low) {
      // skip over the range covered by step #1 above
      if (high == end) {
        break;
      }
      iter = high;
    }

    const SequenceRange range(iter->first, iter->first);
    if (recv_strategy_->has_fragments(range, pub_id, &frag_info)) {
      extend_bitmap_range(frag_info.back().second, iter->second.value);
    } else {
      // it was not in the recv strategy, so the entire range is "missing"
      frag_info.push_back(Frag_t(iter->first, RTPS::FragmentNumberSet()));
      RTPS::FragmentNumberSet& fnSet = frag_info.back().second;
      fnSet.bitmapBase.value = 1;
      fnSet.numBits = std::min(CORBA::ULong(256), iter->second.value);
      fnSet.bitmap.length((fnSet.numBits + 31) / 32);
      for (CORBA::ULong i = 0; i < fnSet.bitmap.length(); ++i) {
        fnSet.bitmap[i] = 0xFFFFFFFF;
      }
    }
  }

  if (frag_info.empty()) {
    return 0;
  }

  const RTPS::NackFragSubmessage nackfrag_prototype = {
    {RTPS::NACK_FRAG, 1 /*FLAG_E*/, 0 /* length set below */},
    ENTITYID_UNKNOWN, // readerId will be filled-in by send_heartbeat_replies()
    ENTITYID_UNKNOWN, // writerId will be filled-in by send_heartbeat_replies()
    {0, 0}, // writerSN set below
    RTPS::FragmentNumberSet(), // fragmentNumberState set below
    {0} // count set below
  };

  size_t size = 0, padding = 0;
  for (size_t i = 0; i < frag_info.size(); ++i) {
    nf.push_back(nackfrag_prototype);
    RTPS::NackFragSubmessage& nackfrag = nf.back();
    nackfrag.writerSN.low = frag_info[i].first.getLow();
    nackfrag.writerSN.high = frag_info[i].first.getHigh();
    nackfrag.fragmentNumberState = frag_info[i].second;
    nackfrag.count.value = ++wi.nackfrag_count_;
    const size_t before_size = size;
    gen_find_size(nackfrag, size, padding);
    nackfrag.smHeader.submessageLength =
      static_cast<CORBA::UShort>(size - before_size) - RTPS::SMHDR_SZ;
  }
  return size;
}

void
RtpsUdpDataLink::extend_bitmap_range(RTPS::FragmentNumberSet& fnSet,
                                     CORBA::ULong extent)
{
  if (extent < fnSet.bitmapBase.value) {
    return; // can't extend to some number under the base
  }
  // calculate the index to the extent to determine the new_num_bits
  const CORBA::ULong new_num_bits = std::min(CORBA::ULong(255),
                                             extent - fnSet.bitmapBase.value + 1),
                     len = (new_num_bits + 31) / 32;
  if (new_num_bits < fnSet.numBits) {
    return; // bitmap already extends past "extent"
  }
  fnSet.bitmap.length(len);
  // We are missing from one past old bitmap end to the new end
  DisjointSequence::fill_bitmap_range(fnSet.numBits + 1, new_num_bits,
                                      fnSet.bitmap.get_buffer(), len,
                                      fnSet.numBits);
}

void
RtpsUdpDataLink::received(const RTPS::HeartBeatFragSubmessage& hb_frag,
                          const GuidPrefix_t& src_prefix)
{
  datareader_dispatch(hb_frag, src_prefix, &RtpsUdpDataLink::process_hb_frag_i);
}

bool
RtpsUdpDataLink::process_hb_frag_i(const RTPS::HeartBeatFragSubmessage& hb_frag,
                                   const RepoId& src,
                                   RtpsReaderMap::value_type& rr)
{
  WriterInfoMap::iterator wi = rr.second.remote_writers_.find(src);
  if (wi == rr.second.remote_writers_.end()) {
    // we may not be associated yet, even if the writer thinks we are
    return false;
  }

  if (hb_frag.count.value <= wi->second.hb_frag_recvd_count_) {
    return false;
  }

  wi->second.hb_frag_recvd_count_ = hb_frag.count.value;

  SequenceNumber seq;
  seq.setValue(hb_frag.writerSN.high, hb_frag.writerSN.low);

  // If seq is outside the heartbeat range or we haven't completely received
  // it yet, send a NackFrag along with the AckNack.  The heartbeat range needs
  // to be checked first because recvd_ contains the numbers below the
  // heartbeat range (so that we don't NACK those).
  if (seq < wi->second.hb_range_.first || seq > wi->second.hb_range_.second
      || !wi->second.recvd_.contains(seq)) {
    wi->second.frags_[seq] = hb_frag.lastFragmentNum;
    wi->second.ack_pending_ = true;
    return true; // timer will invoke send_heartbeat_replies()
  }
  return false;
}


// DataWriter's side of Reliability

void
RtpsUdpDataLink::received(const RTPS::AckNackSubmessage& acknack,
                          const GuidPrefix_t& src_prefix)
{
  // local side is DW
  RepoId local;
  std::memcpy(local.guidPrefix, local_prefix_, sizeof(GuidPrefix_t));
  local.entityId = acknack.writerId; // can't be ENTITYID_UNKNOWN

  ACE_GUARD(ACE_Thread_Mutex, g, lock_);
  const RtpsWriterMap::iterator rw = writers_.find(local);
  if (rw == writers_.end()) {
    if (Transport_debug_level > 5) {
      GuidConverter local_conv(local);
      ACE_DEBUG((LM_WARNING, "(%P|%t) RtpsUdpDataLink::received(ACKNACK) "
        "WARNING local %C no RtpsWriter\n", std::string(local_conv).c_str()));
    }
    return;
  }

  RepoId remote;
  std::memcpy(remote.guidPrefix, src_prefix, sizeof(GuidPrefix_t));
  remote.entityId = acknack.readerId;

  if (Transport_debug_level > 5) {
    GuidConverter local_conv(local), remote_conv(remote);
    ACE_DEBUG((LM_DEBUG, "(%P|%t) RtpsUdpDataLink::received(ACKNACK) "
      "local %C remote %C\n", std::string(local_conv).c_str(),
      std::string(remote_conv).c_str()));
  }

  const ReaderInfoMap::iterator ri = rw->second.remote_readers_.find(remote);
  if (ri == rw->second.remote_readers_.end()) {
    VDBG((LM_WARNING, "(%P|%t) RtpsUdpDataLink::received(ACKNACK) "
      "WARNING ReaderInfo not found\n"));
    return;
  }

  if (acknack.count.value <= ri->second.acknack_recvd_count_) {
    VDBG((LM_WARNING, "(%P|%t) RtpsUdpDataLink::received(ACKNACK) "
      "WARNING Count indicates duplicate, dropping\n"));
    return;
  }

  ri->second.acknack_recvd_count_ = acknack.count.value;

  if (!ri->second.handshake_done_) {
    ri->second.handshake_done_ = true;
    handshake_condition_.broadcast();
  }

  std::map<SequenceNumber, TransportQueueElement*> pendingCallbacks;
  const bool final = acknack.smHeader.flags & 2 /* FLAG_F */;

  if (!ri->second.durable_data_.empty()) {
    SequenceNumber ack;
    ack.setValue(acknack.readerSNState.bitmapBase.high,
                 acknack.readerSNState.bitmapBase.low);
    if (ack > ri->second.durable_data_.rbegin()->first) {
      // Reader acknowledges durable data, we no longer need to store it
      ri->second.durable_data_.swap(pendingCallbacks);
    } else {
      DisjointSequence requests;
      if (!requests.insert(ack, acknack.readerSNState.numBits,
                           acknack.readerSNState.bitmap.get_buffer())
          && !final && ack == rw->second.heartbeat_high(ri->second)) {
        // This is a non-final AckNack with no bits in the bitmap.
        // Attempt to reply to a request for the "base" value which
        // is neither Acked nor Nacked, only when it's the HB high.
        if (ri->second.durable_data_.count(ack)) requests.insert(ack);
      }
      // Attempt to reply to nacks for durable data
      bool sent_some = false;
      typedef std::map<SequenceNumber, TransportQueueElement*>::iterator iter_t;
      iter_t it = ri->second.durable_data_.begin();
      const std::vector<SequenceRange> psr = requests.present_sequence_ranges();
      SequenceNumber::Value lastSent = 0;
      if (!requests.empty()) {
        lastSent = requests.low().getValue() - 1;
      }
      DisjointSequence gaps;
      for (size_t i = 0; i < psr.size(); ++i) {
        for (; it != ri->second.durable_data_.end()
             && it->first < psr[i].first; ++it) ; // empty for-loop
        for (; it != ri->second.durable_data_.end()
             && it->first <= psr[i].second; ++it) {
          durability_resend(it->second);
          //FUTURE: combine multiple resends into one RTPS Message?
          sent_some = true;
          if (it->first.getValue() > lastSent + 1) {
            gaps.insert(SequenceRange(lastSent + 1, it->first.previous()));
          }
          lastSent = it->first.getValue();
        }
      }
      if (!gaps.empty()) {
        send_durability_gaps(local, remote, gaps);
      }
      if (sent_some) {
        return;
      }
      if (gaps.empty() && !requests.empty() &&
          requests.high() < ri->second.durable_data_.begin()->first) {
        // All nacks were below the start of the durable data.
        send_durability_gaps(local, remote, requests);
      }
    }
  }

  // If this ACKNACK was final, the DR doesn't expect a reply, and therefore
  // we don't need to do anything further.
  if (!final) {
    ri->second.requested_changes_.push_back(acknack.readerSNState);
  }

  g.release();
  if (!final) {
    nack_reply_.schedule(); // timer will invoke send_nack_replies()
  }
  typedef std::map<SequenceNumber, TransportQueueElement*>::iterator iter_t;
  for (iter_t it = pendingCallbacks.begin();
       it != pendingCallbacks.end(); ++it) {
    it->second->data_delivered();
  }
}

void
RtpsUdpDataLink::received(const RTPS::NackFragSubmessage& nackfrag,
                          const GuidPrefix_t& src_prefix)
{
  // local side is DW
  RepoId local;
  std::memcpy(local.guidPrefix, local_prefix_, sizeof(GuidPrefix_t));
  local.entityId = nackfrag.writerId; // can't be ENTITYID_UNKNOWN

  ACE_GUARD(ACE_Thread_Mutex, g, lock_);
  const RtpsWriterMap::iterator rw = writers_.find(local);
  if (rw == writers_.end()) {
    if (Transport_debug_level > 5) {
      GuidConverter local_conv(local);
      ACE_DEBUG((LM_WARNING, "(%P|%t) RtpsUdpDataLink::received(NACK_FRAG) "
        "WARNING local %C no RtpsWriter\n", std::string(local_conv).c_str()));
    }
    return;
  }

  RepoId remote;
  std::memcpy(remote.guidPrefix, src_prefix, sizeof(GuidPrefix_t));
  remote.entityId = nackfrag.readerId;

  if (Transport_debug_level > 5) {
    GuidConverter local_conv(local), remote_conv(remote);
    ACE_DEBUG((LM_DEBUG, "(%P|%t) RtpsUdpDataLink::received(NACK_FRAG) "
      "local %C remote %C\n", std::string(local_conv).c_str(),
      std::string(remote_conv).c_str()));
  }

  const ReaderInfoMap::iterator ri = rw->second.remote_readers_.find(remote);
  if (ri == rw->second.remote_readers_.end()) {
    VDBG((LM_WARNING, "(%P|%t) RtpsUdpDataLink::received(NACK_FRAG) "
      "WARNING ReaderInfo not found\n"));
    return;
  }

  if (nackfrag.count.value <= ri->second.nackfrag_recvd_count_) {
    VDBG((LM_WARNING, "(%P|%t) RtpsUdpDataLink::received(NACK_FRAG) "
      "WARNING Count indicates duplicate, dropping\n"));
    return;
  }

  ri->second.nackfrag_recvd_count_ = nackfrag.count.value;

  SequenceNumber seq;
  seq.setValue(nackfrag.writerSN.high, nackfrag.writerSN.low);
  ri->second.requested_frags_[seq] = nackfrag.fragmentNumberState;
  g.release();
  nack_reply_.schedule(); // timer will invoke send_nack_replies()
}

void
RtpsUdpDataLink::send_nack_replies()
{
  ACE_GUARD(ACE_Thread_Mutex, g, lock_);
  // Reply from local DW to remote DR: GAP or DATA
  using namespace OpenDDS::RTPS;
  typedef RtpsWriterMap::iterator rw_iter;
  for (rw_iter rw = writers_.begin(); rw != writers_.end(); ++rw) {

    // consolidate requests from N readers
    std::set<ACE_INET_Addr> recipients;
    DisjointSequence requests;
    RtpsWriter& writer = rw->second;

    typedef ReaderInfoMap::iterator ri_iter;
    const ri_iter end = writer.remote_readers_.end();
    for (ri_iter ri = writer.remote_readers_.begin(); ri != end; ++ri) {

      for (size_t i = 0; i < ri->second.requested_changes_.size(); ++i) {
        const SequenceNumberSet& sn_state = ri->second.requested_changes_[i];
        SequenceNumber base;
        base.setValue(sn_state.bitmapBase.high, sn_state.bitmapBase.low);
        if (sn_state.numBits == 1 && !(sn_state.bitmap[0] & 1)
            && base == writer.heartbeat_high(ri->second)) {
          // Since there is an entry in requested_changes_, the DR must have
          // sent a non-final AckNack.  If the base value is the high end of
          // the heartbeat range, treat it as a request for that seq#.
          if (!writer.send_buff_.is_nil() && writer.send_buff_->contains(base)) {
            requests.insert(base);
          }
        } else {
          requests.insert(base, sn_state.numBits, sn_state.bitmap.get_buffer());
        }
      }

      if (ri->second.requested_changes_.size()) {
        if (locators_.count(ri->first)) {
          recipients.insert(locators_[ri->first].addr_);
        }
        ri->second.requested_changes_.clear();
      }
    }

    DisjointSequence gaps;
    if (!requests.empty()) {
      if (writer.send_buff_.is_nil() || writer.send_buff_->empty()) {
        gaps = requests;
      } else {
        std::vector<SequenceRange> ranges = requests.present_sequence_ranges();
        SingleSendBuffer& sb = *writer.send_buff_;
        ACE_GUARD(TransportSendBuffer::LockType, guard, sb.strategy_lock());
        const RtpsUdpSendStrategy::OverrideToken ot =
          send_strategy_->override_destinations(recipients);
        for (size_t i = 0; i < ranges.size(); ++i) {
          sb.resend_i(ranges[i], &gaps);
        }
      }
    }

    send_nackfrag_replies(writer, gaps, recipients);

    if (!gaps.empty()) {
      ACE_Message_Block* mb_gap =
        marshal_gaps(rw->first, GUID_UNKNOWN, gaps, writer.durable_);
      if (mb_gap) {
        send_strategy_->send_rtps_control(*mb_gap, recipients);
        mb_gap->release();
      }
    }
  }
}

void
RtpsUdpDataLink::send_nackfrag_replies(RtpsWriter& writer,
                                       DisjointSequence& gaps,
                                       std::set<ACE_INET_Addr>& gap_recipients)
{
  typedef std::map<SequenceNumber, DisjointSequence> FragmentInfo;
  std::map<ACE_INET_Addr, FragmentInfo> requests;

  typedef ReaderInfoMap::iterator ri_iter;
  const ri_iter end = writer.remote_readers_.end();
  for (ri_iter ri = writer.remote_readers_.begin(); ri != end; ++ri) {

    if (ri->second.requested_frags_.empty() || !locators_.count(ri->first)) {
      continue;
    }

    const ACE_INET_Addr& remote_addr = locators_[ri->first].addr_;

    typedef std::map<SequenceNumber, RTPS::FragmentNumberSet>::iterator rf_iter;
    const rf_iter rf_end = ri->second.requested_frags_.end();
    for (rf_iter rf = ri->second.requested_frags_.begin(); rf != rf_end; ++rf) {

      const SequenceNumber& seq = rf->first;
      if (writer.send_buff_->contains(seq)) {
        FragmentInfo& fi = requests[remote_addr];
        fi[seq].insert(rf->second.bitmapBase.value, rf->second.numBits,
                       rf->second.bitmap.get_buffer());
      } else {
        gaps.insert(seq);
        gap_recipients.insert(remote_addr);
      }
    }
    ri->second.requested_frags_.clear();
  }

  typedef std::map<ACE_INET_Addr, FragmentInfo>::iterator req_iter;
  for (req_iter req = requests.begin(); req != requests.end(); ++req) {
    const FragmentInfo& fi = req->second;

    ACE_GUARD(TransportSendBuffer::LockType, guard,
      writer.send_buff_->strategy_lock());
    const RtpsUdpSendStrategy::OverrideToken ot =
      send_strategy_->override_destinations(req->first);

    for (FragmentInfo::const_iterator sn_iter = fi.begin();
         sn_iter != fi.end(); ++sn_iter) {
      const SequenceNumber& seq = sn_iter->first;
      writer.send_buff_->resend_fragments_i(seq, sn_iter->second);
    }
  }
}

ACE_Message_Block*
RtpsUdpDataLink::marshal_gaps(const RepoId& writer, const RepoId& reader,
                              const DisjointSequence& gaps, bool durable)
{
  using namespace RTPS;
  // RTPS v2.1 8.3.7.4: the Gap sequence numbers are those in the range
  // [gapStart, gapListBase) and those in the SNSet.
  const SequenceNumber firstMissing = gaps.low(),
                       base = ++SequenceNumber(gaps.cumulative_ack());
  const SequenceNumber_t gapStart = {firstMissing.getHigh(),
                                     firstMissing.getLow()},
                         gapListBase = {base.getHigh(), base.getLow()};
  CORBA::ULong num_bits = 0;
  LongSeq8 bitmap;

  if (gaps.disjoint()) {
    bitmap.length(bitmap_num_longs(base, gaps.high()));
    gaps.to_bitmap(bitmap.get_buffer(), bitmap.length(), num_bits);

  } else {
    bitmap.length(1);
    bitmap[0] = 0;
    num_bits = 1;
  }

  GapSubmessage gap = {
    {GAP, 1 /*FLAG_E*/, 0 /*length determined below*/},
    reader.entityId,
    writer.entityId,
    gapStart,
    {gapListBase, num_bits, bitmap}
  };

  size_t gap_size = 0, padding = 0;
  gen_find_size(gap, gap_size, padding);
  gap.smHeader.submessageLength =
    static_cast<CORBA::UShort>(gap_size + padding) - SMHDR_SZ;

  // For durable writers, change a non-directed Gap into multiple directed gaps.
  std::vector<RepoId> readers;
  if (durable && reader.entityId == ENTITYID_UNKNOWN) {
    const RtpsWriterMap::iterator iter = writers_.find(writer);
    RtpsWriter& rw = iter->second;
    for (ReaderInfoMap::iterator ri = rw.remote_readers_.begin();
         ri != rw.remote_readers_.end(); ++ri) {
      if (!ri->second.expecting_durable_data()) {
        readers.push_back(ri->first);
      }
    }
    if (readers.empty()) return 0;
  }

  const size_t size_per_idst = INFO_DST_SZ + SMHDR_SZ,
    prefix_sz = sizeof(reader.guidPrefix);
  // no additional padding needed for INFO_DST
  const size_t total_sz = readers.empty() ? (gap_size + padding) :
    (readers.size() * (gap_size + padding + size_per_idst));

  ACE_Message_Block* mb_gap = new ACE_Message_Block(total_sz);
  //FUTURE: allocators?
  // byte swapping is handled in the operator<<() implementation
  Serializer ser(mb_gap, false, Serializer::ALIGN_CDR);
  if (readers.empty()) {
    ser << gap;
  } else {
    InfoDestinationSubmessage idst = {
      {INFO_DST, 1 /*FLAG_E*/, INFO_DST_SZ},
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    };
    for (size_t i = 0; i < readers.size(); ++i) {
      std::memcpy(idst.guidPrefix, readers[i].guidPrefix, prefix_sz);
      gap.readerId = readers[i].entityId;
      ser << idst;
      ser << gap;
    }
  }
  return mb_gap;
}

void
RtpsUdpDataLink::durability_resend(TransportQueueElement* element)
{
  ACE_Message_Block* msg = const_cast<ACE_Message_Block*>(element->msg());
  send_strategy_->send_rtps_control(*msg,
                                    get_locator(element->subscription_id()));
}

void
RtpsUdpDataLink::send_durability_gaps(const RepoId& writer,
                                      const RepoId& reader,
                                      const DisjointSequence& gaps)
{
  ACE_Message_Block mb(RTPS::INFO_DST_SZ + RTPS::SMHDR_SZ);
  Serializer ser(&mb, false, Serializer::ALIGN_CDR);
  RTPS::InfoDestinationSubmessage info_dst = {
    {RTPS::INFO_DST, 1 /*FLAG_E*/, RTPS::INFO_DST_SZ},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
  };
  std::memcpy(info_dst.guidPrefix, reader.guidPrefix, sizeof(GuidPrefix_t));
  ser << info_dst;
  mb.cont(marshal_gaps(writer, reader, gaps));
  send_strategy_->send_rtps_control(mb, get_locator(reader));
  mb.cont()->release();
}

void
RtpsUdpDataLink::send_heartbeats()
{
  ACE_GUARD(ACE_Thread_Mutex, g, lock_);

  if (writers_.empty()) {
    heartbeat_.disable();
  }

  using namespace OpenDDS::RTPS;
  std::vector<HeartBeatSubmessage> subm;
  std::set<ACE_INET_Addr> recipients;
  std::vector<TransportQueueElement*> pendingCallbacks;
  const ACE_Time_Value now = ACE_OS::gettimeofday();

  typedef RtpsWriterMap::iterator rw_iter;
  for (rw_iter rw = writers_.begin(); rw != writers_.end(); ++rw) {

    const bool has_data = !rw->second.send_buff_.is_nil()
                          && !rw->second.send_buff_->empty();
    bool final = true, has_durable_data = false;
    SequenceNumber durable_max;

    typedef ReaderInfoMap::iterator ri_iter;
    const ri_iter end = rw->second.remote_readers_.end();
    for (ri_iter ri = rw->second.remote_readers_.begin(); ri != end; ++ri) {
      if ((has_data || !ri->second.handshake_done_)
          && locators_.count(ri->first)) {
        recipients.insert(locators_[ri->first].addr_);
        if (final && !ri->second.handshake_done_) {
          final = false;
        }
      }
      if (!ri->second.durable_data_.empty()) {
        const ACE_Time_Value expiration =
          ri->second.durable_timestamp_ + config_->durable_data_timeout_;
        if (now > expiration) {
          typedef std::map<SequenceNumber, TransportQueueElement*>::iterator
            dd_iter;
          for (dd_iter it = ri->second.durable_data_.begin();
               it != ri->second.durable_data_.end(); ++it) {
            pendingCallbacks.push_back(it->second);
          }
          ri->second.durable_data_.clear();
          if (Transport_debug_level > 3) {
            const GuidConverter gw(rw->first), gr(ri->first);
            VDBG_LVL((LM_INFO, "(%P|%t) RtpsUdpDataLink::send_heartbeats - "
              "removed expired durable data for %C -> %C\n",
              std::string(gw).c_str(), std::string(gr).c_str()), 3);
          }
        } else {
          has_durable_data = true;
          if (ri->second.durable_data_.rbegin()->first > durable_max) {
            durable_max = ri->second.durable_data_.rbegin()->first;
          }
          if (locators_.count(ri->first)) {
            recipients.insert(locators_[ri->first].addr_);
          }
        }
      }
    }

    if (final && !has_data && !has_durable_data) {
      continue;
    }

    const SequenceNumber firstSN = (rw->second.durable_ || !has_data)
                                   ? 1 : rw->second.send_buff_->low(),
        lastSN = std::max(durable_max,
                          has_data ? rw->second.send_buff_->high() : 1);

    const HeartBeatSubmessage hb = {
      {HEARTBEAT,
       CORBA::Octet(1 /*FLAG_E*/ | (final ? 2 /*FLAG_F*/ : 0)),
       HEARTBEAT_SZ},
      ENTITYID_UNKNOWN, // any matched reader may be interested in this
      rw->first.entityId,
      {firstSN.getHigh(), firstSN.getLow()},
      {lastSN.getHigh(), lastSN.getLow()},
      {++rw->second.heartbeat_count_}
    };
    subm.push_back(hb);
  }

  if (!subm.empty()) {
    ACE_Message_Block mb((HEARTBEAT_SZ + SMHDR_SZ) * subm.size()); //FUTURE: allocators?
    // byte swapping is handled in the operator<<() implementation
    Serializer ser(&mb, false, Serializer::ALIGN_CDR);
    bool send_ok = true;
    for (size_t i = 0; i < subm.size(); ++i) {
      if (!(ser << subm[i])) {
        ACE_DEBUG((LM_ERROR, "(%P|%t) RtpsUdpDataLink::send_heartbeats() - "
          "failed to serialize HEARTBEAT submessage %B\n", i));
        send_ok = false;
        break;
      }
    }
    if (send_ok) {
      send_strategy_->send_rtps_control(mb, recipients);
    }
  }
  g.release();
  for (size_t i = 0; i < pendingCallbacks.size(); ++i) {
    pendingCallbacks[i]->data_dropped();
  }
}

RtpsUdpDataLink::ReaderInfo::~ReaderInfo()
{
  expire_durable_data();
}

void
RtpsUdpDataLink::ReaderInfo::expire_durable_data()
{
  typedef std::map<SequenceNumber, TransportQueueElement*>::iterator iter_t;
  for (iter_t it = durable_data_.begin(); it != durable_data_.end(); ++it) {
    it->second->data_dropped();
  }
}

bool
RtpsUdpDataLink::ReaderInfo::expecting_durable_data() const
{
  return durable_ &&
    (durable_timestamp_ == ACE_Time_Value::zero // DW hasn't resent yet
     || !durable_data_.empty());                // DW resent, not sent to reader
}

SequenceNumber
RtpsUdpDataLink::RtpsWriter::heartbeat_high(const ReaderInfo& ri) const
{
  const SequenceNumber durable_max =
    ri.durable_data_.empty() ? 0 : ri.durable_data_.rbegin()->first;
  const SequenceNumber data_max =
    send_buff_.is_nil() ? 0 : (send_buff_->empty() ? 0 : send_buff_->high());
  return std::max(durable_max, data_max);
}


// Implementing TimedDelay and HeartBeat nested classes (for ACE timers)

void
RtpsUdpDataLink::TimedDelay::schedule()
{
  if (!scheduled_) {
    const long timer = outer_->get_reactor()->schedule_timer(this, 0, timeout_);

    if (timer == -1) {
      ACE_DEBUG((LM_ERROR, "(%P|%t) RtpsUdpDataLink::TimedDelay::schedule "
        "failed to schedule timer %p\n", ACE_TEXT("")));
    } else {
      scheduled_ = true;
    }
  }
}

void
RtpsUdpDataLink::TimedDelay::cancel()
{
  if (scheduled_) {
    outer_->get_reactor()->cancel_timer(this);
    scheduled_ = false;
  }
}

void
RtpsUdpDataLink::HeartBeat::enable()
{
  if (!enabled_) {
    const ACE_Time_Value& per = outer_->config_->heartbeat_period_;
    const long timer =
      outer_->get_reactor()->schedule_timer(this, 0, ACE_Time_Value::zero, per);

    if (timer == -1) {
      ACE_DEBUG((LM_ERROR, "(%P|%t) RtpsUdpDataLink::HeartBeat::enable"
        " failed to schedule timer %p\n", ACE_TEXT("")));
    } else {
      enabled_ = true;
    }
  }
}

void
RtpsUdpDataLink::HeartBeat::disable()
{
  if (enabled_) {
    outer_->get_reactor()->cancel_timer(this);
    enabled_ = false;
  }
}

} // namespace DCPS
} // namespace OpenDDS
