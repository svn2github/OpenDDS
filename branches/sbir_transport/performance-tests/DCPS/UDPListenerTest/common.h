// -*- C++ -*-
// ============================================================================
/**
 *  @file   common.h
 *
 *  $Id$
 *
 *
 */
// ============================================================================


#include "dds/DCPS/transport/udp/UdpInst.h"

const long  TEST_DOMAIN   = 411;
const char* TEST_TOPIC    = "UDPListenerPerfTest";
const char* TEST_TYPE     = "UDPListenerPerfTestType";
const ACE_TCHAR* reader_address_str = ACE_TEXT("default");
const ACE_TCHAR* writer_address_str = ACE_TEXT("default");

const ACE_Time_Value max_blocking_time(::DDS::DURATION_INFINITE_SEC);

int NUM_SAMPLES = 128;
int DATA_SIZE = 128;
int RECVS_BTWN_READS = 10;
CORBA::ULong max_mili_sec_blocking = 5000;

int num_datawriters = 1;
int num_datareaders = 1;
unsigned throttle_factor = 0;
unsigned id = 0;
// largest positive value of a long is 2147483647
CORBA::Long MAX_SAMPLES_PER_INSTANCE = ::DDS::LENGTH_UNLIMITED;
CORBA::Long MAX_SAMPLES = ::DDS::LENGTH_UNLIMITED;
CORBA::Long MAX_INSTANCES = ::DDS::LENGTH_UNLIMITED;

OpenDDS::DCPS::TransportImpl_rch reader_transport_impl;
OpenDDS::DCPS::TransportImpl_rch writer_transport_impl;

enum TransportTypeId
{
  SIMPLE_UDP
};

enum TransportInstanceId
{
  SUB_TRAFFIC,
  PUB_TRAFFIC
};



int init_reader_tranport ()
{
  int status = 0;

  reader_transport_impl =
      TheTransportFactory->create_transport_impl (SUB_TRAFFIC, ACE_TEXT("udp"), OpenDDS::DCPS::DONT_AUTO_CONFIG);

  OpenDDS::DCPS::TransportInst_rch reader_config
    = TheTransportFactory->create_configuration (SUB_TRAFFIC, ACE_TEXT("udp"));

  OpenDDS::DCPS::UdpInst* reader_udp_config
    = static_cast <OpenDDS::DCPS::UdpInst*> (reader_config.in ());

  ACE_INET_Addr reader_address (reader_address_str);
  reader_udp_config->local_address_ = reader_address;

  if (reader_transport_impl->configure(reader_config.in()) != 0)
    {
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ::init_reader_tranport: ")
                 ACE_TEXT("Failed to configure the transport.\n")));
      status = 1;
    }

  return status;
}



int init_writer_tranport ()
{
  int status = 0;

  writer_transport_impl =
      TheTransportFactory->create_transport_impl(PUB_TRAFFIC,
                                                 ACE_TEXT("udp"),
                                                 OpenDDS::DCPS::DONT_AUTO_CONFIG);

  OpenDDS::DCPS::TransportInst_rch writer_config
    = TheTransportFactory->create_configuration (PUB_TRAFFIC, ACE_TEXT("udp"));

  OpenDDS::DCPS::UdpInst* writer_udp_config
    = static_cast <OpenDDS::DCPS::UdpInst*> (writer_config.in ());

  if (0 != ACE_OS::strcmp(ACE_TEXT("default"), writer_address_str) )
    {
      ACE_INET_Addr writer_address (writer_address_str);
      writer_udp_config->local_address_ = writer_address;
    }

  if (writer_transport_impl->configure(writer_config.in()) != 0)
    {
      ACE_ERROR((LM_ERROR,
                 ACE_TEXT("(%P|%t) ::init_writer_tranport: ")
                 ACE_TEXT("Failed to configure the transport.\n")));
      status = 1;
    }

  return status;
}

