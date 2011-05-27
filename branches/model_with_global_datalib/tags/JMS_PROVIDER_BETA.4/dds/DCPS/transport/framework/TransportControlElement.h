// -*- C++ -*-
//
// $Id$
#ifndef OPENDDS_DCPS_TRANSPORTGDCONTROLELEMENT_H
#define OPENDDS_DCPS_TRANSPORTGDCONTROLELEMENT_H

#include "dds/DCPS/dcps_export.h"
#include "dds/DCPS/GuidUtils.h"
#include "TransportDefs.h"
#include "TransportQueueElement.h"

class ACE_Message_Block ;

namespace OpenDDS
{

  namespace DCPS
  {

    class OpenDDS_Dcps_Export TransportControlElement : public TransportQueueElement
    {
      public:

        TransportControlElement(ACE_Message_Block* msg_block);

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
    };

  }
}


#endif  /* OPENDDS_DCPS_TRANSPORTGDCONTROLELEMENT_H */
