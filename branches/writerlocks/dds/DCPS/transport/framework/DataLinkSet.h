/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_DATALINKSET_H
#define OPENDDS_DCPS_DATALINKSET_H

#include "dds/DCPS/dcps_export.h"
#include "dds/DCPS/RcObject_T.h"
#include "DataLink_rch.h"
#include "TransportDefs.h"
#include "TransportSendControlElement.h"

#include "ace/Synch.h"

#include <map>

namespace OpenDDS {
namespace DCPS {

class TransportSendListener;
struct DataSampleListElement;

class OpenDDS_Dcps_Export DataLinkSet : public RcObject<ACE_SYNCH_MUTEX> {
public:

  DataLinkSet();
  virtual ~DataLinkSet();

  // Returns 0 for success, -1 for failure, and 1 for failure due
  // to duplicate entry (link is already a member of the set).
  int insert_link(DataLink* link);

  void remove_link(const DataLink_rch& link);

  /// Send to each DataLink in the set.
  void send(DataSampleListElement* sample);

  /// Send control message to each DataLink in the set.
  SendControlStatus send_control(RepoId                  pub_id,
                                 TransportSendListener*  listener,
                                 const DataSampleHeader& header,
                                 ACE_Message_Block*      msg);

  void send_response(RepoId sub_id,
                     const DataSampleHeader& header,
                     ACE_Message_Block* response);

  bool remove_sample(const DataSampleListElement* sample);

  bool remove_all_msgs(RepoId pub_id);

  /// This will do several things, including adding to the membership
  /// of the send_links_ set.  Any DataLinks added to the send_links_
  /// set will be also told about the send_start() event.  Those
  /// DataLinks (in the pub_links set) that are already in the
  /// send_links_ set will not be told about the send_start() event
  /// since they heard about it when they were inserted into the
  /// send_links_ set.
  void send_start(DataLinkSet* link_set);

  /// This will inform each DataLink in the set about the send_stop()
  /// event.  It will then clear the send_links_ set.
  void send_stop();

  DataLinkSet* select_links(const RepoId* remoteIds,
                            const CORBA::ULong num_targets);

  bool empty();

  typedef ACE_SYNCH_MUTEX     LockType;
  typedef ACE_Guard<LockType> GuardType;

  typedef std::map<DataLinkIdType, DataLink_rch> MapType;

  void send_delayed_notifications();

  //{@
  /// Accessors for external iteration
  LockType& lock() { return lock_; }
  MapType& map() { return map_; }
  //@}

private:

  /// Hash map for DataLinks.
  MapType map_;

  /// Allocator for TransportSendControlElement.
  TransportSendControlElementAllocator send_control_element_allocator_;

  /// This lock will protect critical sections of code that play a
  /// role in the sending of data.
  LockType lock_;
};

} // namespace DCPS
} // namespace OpenDDS

#if defined (__ACE_INLINE__)
#include "DataLinkSet.inl"
#endif /* __ACE_INLINE__ */

#endif /* OPENDDS_DCPS_DATALINKSET_H */
