/*
 * $Id$
 *
 * Copyright 2010 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#ifndef OPENDDS_DCPS_TRANSPORTGDCONTROLELEMENT_H
#define OPENDDS_DCPS_TRANSPORTGDCONTROLELEMENT_H

#include "dds/DCPS/dcps_export.h"
#include "dds/DCPS/GuidUtils.h"
#include "TransportDefs.h"
#include "TransportQueueElement.h"

class ACE_Message_Block ;

namespace OpenDDS {
namespace DCPS {

class OpenDDS_Dcps_Export TransportControlElement : public TransportQueueElement {
public:

  /**
   * msg_block - chain of ACE_Message_Blocks containing the control
   *             sample held by this queue element, if any.
   * pub_id    - publication Id value of the originating publication, if
   *             any.
   * owner     - indicates that this element has been obtained from the
   *             heap and can be discarded to it.  If an object of this
   *             type is created on the stack, this *must* be set to
   *             false.
   */
  TransportControlElement( const ACE_Message_Block* msg_block,
                           const RepoId& pub_id = GUID_UNKNOWN,
                           bool  owner = true);

  virtual ~TransportControlElement();

protected:

  virtual bool requires_exclusive_packet() const;

  virtual RepoId publication_id() const;

  virtual const ACE_Message_Block* msg() const;

  virtual void release_element(bool dropped_by_transport);

  virtual void data_delivered();

private:

  /// The control message.
  ACE_Message_Block* msg_;

  /// Publication Id of the originating publication.
  RepoId pub_id_;

  /// Ownership flag.
  bool owner_;
};

} // namespace DCPS
} // namespace OpenDDS

#endif  /* OPENDDS_DCPS_TRANSPORTGDCONTROLELEMENT_H */
