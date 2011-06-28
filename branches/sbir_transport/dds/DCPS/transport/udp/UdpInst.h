/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef DCPS_UDPINST_H
#define DCPS_UDPINST_H

#include "Udp_Export.h"

#include "ace/INET_Addr.h"

#include "dds/DCPS/transport/framework/TransportInst.h"

namespace OpenDDS {
namespace DCPS {

class OpenDDS_Udp_Export UdpInst
  : public TransportInst {
public:
  /// The address from which to send/receive data.
  /// The default value is: none.
  ACE_INET_Addr local_address_;

  UdpInst();

  virtual int load(const TransportIdType& id,
                   ACE_Configuration_Heap& config);

  /// Diagnostic aid.
  virtual void dump(std::ostream& os);
};

} // namespace DCPS
} // namespace OpenDDS

#endif  /* DCPS_UDPINST_H */
