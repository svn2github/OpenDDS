// -*- C++ -*-
//
// $Id$
#ifndef OPENDDS_DCPS_NETWORKADDRESS_H
#define OPENDDS_DCPS_NETWORKADDRESS_H

#include "dds/DCPS/dcps_export.h"
#include "tao/Basic_Types.h"
#include "ace/INET_Addr.h"
#include "ace/CDR_Stream.h"
#include "ace/SString.h"
#include <vector>


namespace OpenDDS
{

  namespace DCPS
  {

    struct HostnameInfo
    {
      int index_;
      ACE_TString hostname_;
    };

    typedef std::vector <HostnameInfo> HostnameInfoVector;

    /**
     * @struct NetworkAddress
     *
     * @brief Defines a wrapper around address info which is used for advertise.
     *       
     *
     * This is used to send/receive an address information through transport.
     */
    struct OpenDDS_Dcps_Export NetworkAddress
    {
      /// Default Ctor
      NetworkAddress();

      ~NetworkAddress();

      /// Ctor using address string.
      explicit NetworkAddress(const ACE_TString& addr);

      void dump ();

      /// Accessor to populate the provided ACE_INET_Addr object from the
      /// address string received through transport.
      void to_addr(ACE_INET_Addr& addr) const;

      /// Reserve byte for some feature supports in the future. 
      /// e.g. version support.
      CORBA::Octet reserved_;

      /// The address in string format. e.g. ip:port, hostname:port
      ACE_TString addr_;
    };

    /// Helper function to get the fully qualified hostname.
    /// It attempts to discover the FQDN by the network interface addresses, however 
    /// the result is impacted by the network configuration, so it returns name in the
    /// order whoever is found first - FQDN, short hostname, name resolved from loopback
    /// address. In the case using short hostname or name resolved from loopback, a 
    /// warning is logged. If there is no any name discovered from network interfaces,
    /// an error is logged.
    extern OpenDDS_Dcps_Export
    ACE_TString get_fully_qualified_hostname ();

  }
}

/// Marshal into a buffer.
extern OpenDDS_Dcps_Export
ACE_CDR::Boolean
operator<< (ACE_OutputCDR& outCdr, OpenDDS::DCPS::NetworkAddress& value);

/// Demarshal from a buffer.
extern OpenDDS_Dcps_Export
ACE_CDR::Boolean
operator>> (ACE_InputCDR& inCdr, OpenDDS::DCPS::NetworkAddress& value);

#if defined (__ACE_INLINE__)
# include "NetworkAddress.inl"
#endif  /* __ACE_INLINE__ */

#endif /* OPENDDS_DCPS_NETWORKADDRESS_H */
