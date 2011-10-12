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

#include "dds/DCPS/transport/framework/NetworkAddress.h"
#include "dds/DCPS/transport/framework/TransportCustomizedElement.h"
#include "dds/DCPS/transport/framework/TransportSendElement.h"
#include "dds/DCPS/transport/framework/TransportSendControlElement.h"

#include "dds/DCPS/RTPS/RtpsMessageTypesTypeSupportImpl.h"

#include "ace/Default_Constants.h"
#include "ace/Log_Msg.h"
#include "ace/Message_Block.h"

#include <cstring>

#ifndef __ACE_INLINE__
# include "RtpsUdpDataLink.inl"
#endif  /* __ACE_INLINE__ */


namespace OpenDDS {
namespace DCPS {

RtpsUdpDataLink::RtpsUdpDataLink(RtpsUdpTransport* transport,
                                 const GuidPrefix_t& local_prefix)
  : DataLink(transport, // 3 data link "attributes", below, are unused
             0,         // priority
             false,     // is_loopback
             false),    // is_active
    config_(0),
    reactor_task_(0),
    transport_customized_element_allocator_(40, sizeof(TransportCustomizedElement))
{
  std::memcpy(local_prefix_, local_prefix, sizeof(GuidPrefix_t));
}

bool
RtpsUdpDataLink::open()
{
  if (unicast_socket_.open(config_->local_address_) != 0) {
    ACE_ERROR_RETURN((LM_ERROR,
                      ACE_TEXT("(%P|%t) ERROR: ")
                      ACE_TEXT("RtpsUdpDataLink::open: socket open: %m\n")),
                     false);
  }

  if (config_->use_multicast_) {
    if (multicast_socket_.join(config_->multicast_group_address_) != 0) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("(%P|%t) ERROR: ")
                        ACE_TEXT("RtpsUdpDataLink::open: ")
                        ACE_TEXT("ACE_SOCK_Dgram_Mcast::join failed: %m\n")),
                       false);
    }
  }

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

void
RtpsUdpDataLink::control_received(ReceivedDataSample& sample,
                                  const ACE_INET_Addr& remote_address)
{
}

void
RtpsUdpDataLink::add_locator(const RepoId& remote_id,
                             const ACE_INET_Addr& address)
{
  locators_[remote_id] = address;
}

void
RtpsUdpDataLink::get_locators(const RepoId& local_id,
                              std::set<ACE_INET_Addr>& addrs) const
{
  using std::map;
  typedef map<RepoId, ACE_INET_Addr, GUID_tKeyLessThan>::const_iterator iter_t;

  if (local_id == GUID_UNKNOWN) {
    for (iter_t iter = locators_.begin(); iter != locators_.end(); ++iter) {
      addrs.insert(iter->second);
    }
    return;
  }

  const GUIDSeq_var peers = peer_ids(local_id);
  if (!peers.ptr()) {
    return;
  }
  for (CORBA::ULong i = 0; i < peers->length(); ++i) {
    const iter_t iter = locators_.find(peers[i]);
    if (iter == locators_.end()) {
      // log error...
    } else {
      addrs.insert(iter->second);
    }
  }
}

void
RtpsUdpDataLink::stop_i()
{
}

TransportQueueElement*
RtpsUdpDataLink::customize_queue_element(TransportQueueElement* element)
{
  const ACE_Message_Block* msg = element->msg();
  if (!msg) {
    return element;
  }

  TransportSendElement* tse = dynamic_cast<TransportSendElement*>(element);
  TransportCustomizedElement* tce =
    dynamic_cast<TransportCustomizedElement*>(element);
  TransportSendControlElement* tsce = dynamic_cast<TransportSendControlElement*>(element);

  ACE_Message_Block* data = 0;
  OpenDDS::RTPS::SubmessageSeq subm;

  // Based on the type of 'element', find and duplicate the data payload
  // continuation block.
  if (tsce) {        // Control message
    data = msg->cont()->duplicate();
    // Create RTPS Submessage(s) in place of the OpenDDS DataSampleHeader
    RtpsSampleHeader::populate_control_submessages(subm, *tsce);
  } else if (tse) {  // Basic data message
    // {DataSampleHeader} -> {Data Payload}
    data = msg->cont()->duplicate();
    const DataSampleListElement* dsle = tse->sample();
    // Create RTPS Submessage(s) in place of the OpenDDS DataSampleHeader
    RtpsSampleHeader::populate_submessages(subm, *dsle,
                                           this->requires_inline_qos(dsle->publication_id_));
  } else if (tce) {  // Customized data message
    // {DataSampleHeader} -> {Content Filtering GUIDs} -> {Data Payload}
    data = msg->cont()->cont()->duplicate();
    const DataSampleListElement* dsle = tce->original_send_element()->sample();
    // Create RTPS Submessage(s) in place of the OpenDDS DataSampleHeader
    RtpsSampleHeader::populate_submessages(subm, *dsle,
                                           this->requires_inline_qos(dsle->publication_id_));
  } else {
    //TODO: handle other types?
    return element;
  }

  size_t size = 0, padding = 0;
  for (CORBA::ULong i = 0; i < subm.length(); ++i) {
    if ((size + padding) % 4) {
      padding += 4 - ((size + padding) % 4);
    }
    gen_find_size(subm[i], size, padding);
  }

  ACE_Message_Block* hdr =
    new ACE_Message_Block(size + padding, ACE_Message_Block::MB_DATA, data);

  for (CORBA::ULong i = 0; i < subm.length(); ++i) {
    // byte swapping is handled in the operator<<() implementation
    Serializer ser(hdr, false, Serializer::ALIGN_CDR);
    ser << subm[i];
    const size_t len = hdr->length();
    if (len % 4) {
      hdr->wr_ptr(4 - (len % 4));
    }
  }

  // TODO: Decide what to do with this allocator.  Currently, we use a locally
  // created allocator.  Other options, pass a null reference (use the heap) or
  // somehow get the allocator from the WriteDataContainer.
  TransportCustomizedElement* rtps =
    TransportCustomizedElement::alloc(element, false,
                                      &this->transport_customized_element_allocator_);
  rtps->set_msg(hdr);

  // Let the framework know each TransportCustomizedElement must be in its own
  // Transport packet (i.e. have its own RTPS Message Header).
  rtps->set_requires_exclusive();
  return rtps;
}

bool
RtpsUdpDataLink::requires_inline_qos(const PublicationId& pub_id)
{
  return false;
}

} // namespace DCPS
} // namespace OpenDDS
