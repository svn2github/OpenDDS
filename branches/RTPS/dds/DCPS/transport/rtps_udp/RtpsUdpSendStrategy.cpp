/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "RtpsUdpSendStrategy.h"
#include "RtpsUdpDataLink.h"
#include "RtpsUdpInst.h"

#include "dds/DCPS/transport/framework/NullSynchStrategy.h"
#include "dds/DCPS/transport/framework/TransportCustomizedElement.h"
#include "dds/DCPS/transport/framework/TransportSendElement.h"

#include "dds/DCPS/RTPS/BaseMessageTypes.h"
#include "dds/DCPS/RTPS/RtpsMessageTypesTypeSupportImpl.h"

#include "dds/DCPS/Serializer.h"

#include <cstring>

namespace OpenDDS {
namespace DCPS {

RtpsUdpSendStrategy::RtpsUdpSendStrategy(RtpsUdpDataLink* link)
  : TransportSendStrategy(TransportInst_rch(link->config(), false),
                          0,  // synch_resource
                          link->transport_priority(),
                          new NullSynchStrategy),
    link_(link)
{
  rtps_header_.prefix[0] = 'R';
  rtps_header_.prefix[1] = 'T';
  rtps_header_.prefix[2] = 'P';
  rtps_header_.prefix[3] = 'S';
  rtps_header_.version = OpenDDS::RTPS::PROTOCOLVERSION;
  rtps_header_.vendorId = OpenDDS::RTPS::VENDORID_OPENDDS;
  std::memcpy(rtps_header_.guidPrefix, link->local_prefix(),
              sizeof(GuidPrefix_t));

  // for testing only...
  remote_address_ = link->config()->remote_address_;
}

ssize_t
RtpsUdpSendStrategy::send_bytes_i(const iovec iov[], int n)
{
  //TODO: determine destination address(es)
  // 1. need to (safely) get current elems_ from base TransportSendStrategy
  //    -- also cover the case where we come in from TransportSendBuffer
  // 2. first TQE in elems_ should be good enough, since we require exclusive
  // 3. get the publication RepoId from that TQE
  // 4. link_->get_locators(pub_id, addrs)
  // 5. send to each addr in addrs

  return link_->unicast_socket().send(iov, n, remote_address_);
}

void
RtpsUdpSendStrategy::marshal_transport_header(ACE_Message_Block* mb)
{
  Serializer writer(mb);
  writer << rtps_header_;
}

void
RtpsUdpSendStrategy::stop_i()
{
}

} // namespace DCPS
} // namespace OpenDDS
