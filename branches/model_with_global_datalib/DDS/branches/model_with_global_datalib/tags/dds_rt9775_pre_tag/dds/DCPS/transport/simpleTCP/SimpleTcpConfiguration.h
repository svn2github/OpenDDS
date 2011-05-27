// -*- C++ -*-
//
// $Id$
#ifndef TAO_DCPS_SIMPLETCPCONFIGURATION_H
#define TAO_DCPS_SIMPLETCPCONFIGURATION_H

#include  "dds/DCPS/dcps_export.h"
#include  "dds/DCPS/transport/framework/TransportConfiguration.h"
#include  "ace/INET_Addr.h"


namespace TAO
{
  namespace DCPS
  {

    class TAO_DdsDcps_Export SimpleTcpConfiguration
                                          : public TransportConfiguration
    {
      public:

        SimpleTcpConfiguration();
        virtual ~SimpleTcpConfiguration();
        virtual int load (const TransportIdType& id,
                          ACE_Configuration_Heap& cf);

        /// Describes the local endpoint to be used to accept
        /// passive connections.
        ACE_INET_Addr local_address_;

        bool enable_nagle_algorithm_;

        /// The initial retry delay in milliseconds.
        /// The first connection retry will be when the loss of connection
        /// is detected.  The second try will be after this delay.
        /// The default is 500 miliseconds.
        int conn_retry_initial_delay_;

        /// The backoff multiplier for reconnection strategy.
        /// The third and so on reconnect will be this value * the previous delay.
        /// Hence with conn_retry_initial_delay=500 and conn_retry_backoff_multiplier=1.5
        /// the second reconnect attempt will be at 0.5 seconds after first retry connect
        /// fails; the third attempt will be 0.75 seconds after the second retry connect
        /// fails; the fourth attempt will be 1.125 seconds after the third retry connect
        /// fails.
        /// The default value is 2.0.
        double conn_retry_backoff_multiplier_;

        /// Number of attemps to reconnect before giving up and calling
        /// on_publication_lost() and on_subscription_lost() callbacks.
        /// The default is 3.
        int conn_retry_attempts_;

        /// Maximum period (in milliseconds) of not being able to send queued
        /// messages. If there are samples queued and no output for longer
        /// than this period then the connection will be closed and on_*_lost()
        /// callbacks will be called. If the value is zero, the default, then
        /// this check will not be made.
        int max_output_pause_period_;

        /// The time period in milliseconds for the acceptor side
        /// of a connection to wait for the connection to be reconnected.
        /// If not reconnected within this period then
        /// on_publication_lost() and on_subscription_lost() callbacks
        /// will be called.
        /// The default is 2 seconds (2000 millseconds).
        int passive_reconnect_duration_;

      /// The time period in milliseconds for the acceptor side
      /// of a connection to wait for the connection.
      /// The default is 0 (wait forever)
      /// This currently doesn't apply to passive reconnections.
      unsigned long passive_connect_duration_;
    };

  } /* namespace DCPS */

} /* namespace TAO */

#if defined (__ACE_INLINE__)
#include "SimpleTcpConfiguration.inl"
#endif /* __ACE_INLINE__ */


#endif  /* TAO_DCPS_SIMPLETCPCONFIGURATION_H */
