/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_TRANSPORTSENDLISTENER_H
#define OPENDDS_DCPS_TRANSPORTSENDLISTENER_H

#include "dds/DCPS/dcps_export.h"
#include "dds/DCPS/RcHandle_T.h"
#include "TransportDefs.h"

class ACE_Message_Block;

namespace OpenDDS {
namespace DCPS {

struct DataSampleListElement;
struct DataSampleHeader;
typedef ACE_Message_Block DataSample;

class DataLinkSet;
typedef RcHandle<DataLinkSet> DataLinkSet_rch;

class OpenDDS_Dcps_Export TransportSendListener {
public:

  virtual ~TransportSendListener();

  virtual void data_delivered(const DataSampleListElement* sample);
  virtual void data_dropped(const DataSampleListElement* sample,
                            bool dropped_by_transport);

  virtual void control_delivered(ACE_Message_Block* sample);
  virtual void control_dropped(ACE_Message_Block* sample,
                               bool dropped_by_transport);

  /// Hook for the listener to override a normal control message with
  /// customized messages to different DataLinks.
  virtual SendControlStatus send_control_customized(
    const DataLinkSet_rch& /* links */,
    ACE_Message_Block* /* msg */,
    void* /* extra */)
  {
    return SEND_CONTROL_OK;
  }

  /// Handle reception of SAMPLE_ACK message.
  virtual void deliver_ack(
    const DataSampleHeader& /* header */,
    DataSample* /* data */)
  {}

protected:

  TransportSendListener();
};

} // namespace DCPS
} // namespace OpenDDS

#if defined (__ACE_INLINE__)
#include "TransportSendListener.inl"
#endif /* __ACE_INLINE__ */

#endif /* OPENDDS_DCPS_TRANSPORTSENDLISTENER_H */
