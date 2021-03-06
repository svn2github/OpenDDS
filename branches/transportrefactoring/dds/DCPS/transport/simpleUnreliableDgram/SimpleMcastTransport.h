// -*- C++ -*-
//
// $Id$
#ifndef OPENDDS_DCPS_SIMPLEMCASTTRANSPORT_H
#define OPENDDS_DCPS_SIMPLEMCASTTRANSPORT_H

#include "SimpleUnreliableDgram_export.h"
#include "SimpleUnreliableDgramTransport.h"


namespace OpenDDS
{

  namespace DCPS
  {

    class SimpleUnreliableDgram_Export SimpleMcastTransport : public SimpleUnreliableDgramTransport
    {
      public:

        SimpleMcastTransport();
        virtual ~SimpleMcastTransport();

      protected:

        virtual int configure_socket (TransportConfiguration* config);

        virtual int connection_info_i
                                 (TransportInterfaceInfo& local_info) const;

        virtual void deliver_sample(ReceivedDataSample&  sample,
                            const ACE_INET_Addr& remote_address);

      private:
  
        ACE_INET_Addr local_address_;
        ACE_INET_Addr multicast_group_address_;
        bool receiver_;
    };

  } /* namespace DCPS */

} /* namespace OpenDDS */

#if defined (__ACE_INLINE__)
#include "SimpleMcastTransport.inl"
#endif /* __ACE_INLINE__ */


#endif  /* OPENDDS_DCPS_SIMPLEMCASTTRANSPORT_H */
