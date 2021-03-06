// -*- C++ -*-
//
// $Id$
#ifndef OPENDDS_DCPS_THREADSYNCHRESOURCE_H
#define OPENDDS_DCPS_THREADSYNCHRESOURCE_H

#include "dds/DCPS/dcps_export.h"
#include "ace/Time_Value.h"


namespace OpenDDS
{
  namespace DCPS
  {

    class OpenDDS_Dcps_Export ThreadSynchResource
    {
      public:

        virtual ~ThreadSynchResource();

        virtual int wait_to_unclog();

     
      protected:

        virtual void notify_lost_on_backpressure_timeout () = 0;

        ThreadSynchResource(ACE_HANDLE handle);
        ACE_HANDLE handle_;
        ACE_Time_Value* timeout_;
    };

  } /* namespace DCPS */
} /* namespace OpenDDS */

#if defined (__ACE_INLINE__)
#include "ThreadSynchResource.inl"
#endif /* __ACE_INLINE__ */

#endif  /* OPENDDS_DCPS_THREADSYNCHRESOURCE_H */
