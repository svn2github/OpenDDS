// -*- C++ -*-
//
// $Id$

#include "SimpleUnreliableDgram_pch.h"
#include "SimpleMcastTransport.h"
#include "SimpleMcastConfiguration.h"
#include "dds/DCPS/transport/framework/NetworkAddress.h"
#include <sstream>


#if !defined (__ACE_INLINE__)
#include "SimpleMcastTransport.inl"
#endif /* __ACE_INLINE__ */

OpenDDS::DCPS::SimpleMcastTransport::~SimpleMcastTransport()
{
  DBG_ENTRY_LVL("SimpleMcastTransport","~SimpleMcastTransport",6);
}



int
OpenDDS::DCPS::SimpleMcastTransport::configure_socket(TransportConfiguration* config)
{
  DBG_ENTRY_LVL("SimpleMcastTransport","configure_socket",6);

  // Downcast the config argument to a SimpleMcastConfiguration*
  SimpleMcastConfiguration* mcast_config = ACE_static_cast(SimpleMcastConfiguration*,
                                                       config);

  if (mcast_config == 0)
    {
      // The downcast failed.
      ACE_ERROR_RETURN((LM_ERROR,
                        "(%P|%t) ERROR: Failed downcast from TransportConfiguration "
                        "to SimpleMcastConfiguration.\n"),
                       -1);
    }

  // Open our socket using the parameters from our mcast_config_ object.
  if (this->socket_->open_socket (mcast_config->local_address_,
                          mcast_config->multicast_group_address_,
                          mcast_config->receiver_) != 0)
    {
      ACE_ERROR_RETURN((LM_ERROR,
                        "(%P|%t) ERROR: failed to open multicast socket %s:%d: %p\n",
                        mcast_config->multicast_group_address_.get_host_addr (),
                        mcast_config->multicast_group_address_.get_port_number (),
                        "open"),
                       -1);
    }

  // As default, the acceptor will be listening on INADDR_ANY. It's not necessary to
  // update this local_address since it's not advertised.
  if (mcast_config->local_address_.is_any())
    {
      unsigned short port = mcast_config->local_address_.get_port_number ();
      std::stringstream out;
      out << port;

      const std::string& hostname = get_fully_qualified_hostname ();

      mcast_config->local_address_.set (port, hostname.c_str());
      mcast_config->local_address_str_ = hostname;
      mcast_config->local_address_str_ += ":";
      mcast_config->local_address_str_ += out.str ();
    }

  // Do not need update multicast_group_address_ since it's default to use 
  // ACE_DEFAULT_MULTICAST_PORT:ACE_DEFAULT_MULTICAST_ADDR
  this->local_address_ = mcast_config->local_address_;
  this->multicast_group_address_ = mcast_config->multicast_group_address_;
  this->multicast_group_address_str_ = mcast_config->multicast_group_address_str_;
  this->receiver_ = mcast_config->receiver_;

  return 0;
}


int
OpenDDS::DCPS::SimpleMcastTransport::connection_info_i
                                   (TransportInterfaceInfo& local_info) const
{
  DBG_ENTRY_LVL("SimpleMcastTransport","connection_info_i",6);

  VDBG_LVL ((LM_DEBUG, "(%P|%t)SimpleMcastTransport::connection_info_i %s\n", 
    this->multicast_group_address_str_.c_str ()), 2);

  NetworkAddress network_order_address(this->multicast_group_address_str_);
  
  ACE_OutputCDR cdr;
  cdr << network_order_address;
  size_t len = cdr.total_length ();

  // Allow DCPSInfo to check compatibility of transport implemenations.
  local_info.transport_id = 3; // TBD Change magic number into a enum or constant value.
  // TBD SOON - Move the local_info.data "population" to the NetworkAddress.
  local_info.data = OpenDDS::DCPS::TransportInterfaceBLOB
    (len,
    len,
    (CORBA::Octet*)(cdr.buffer ()));

  return 0;
}



