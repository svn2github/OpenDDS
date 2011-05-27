// -*- C++ -*-
//
// $Id$
#include  "SimpleSubscriber.h"
#include  "TestException.h"
#include  "dds/DCPS/transport/framework/TransportImpl.h"
#include  "dds/DCPS/transport/framework/TransportInterface.h"
#include  "dds/DCPS/transport/framework/ReceivedDataSample.h"

#include "dds/DCPS/transport/framework/EntryExit.h"


SimpleSubscriber::SimpleSubscriber()
{
  DBG_ENTRY("SimpleSubscriber","SimpleSubscriber");
}


SimpleSubscriber::~SimpleSubscriber()
{
  DBG_ENTRY("SimpleSubscriber","~SimpleSubscriber");
}


void
SimpleSubscriber::init(TAO::DCPS::TransportIdType          transport_id,
                       TAO::DCPS::RepoId                   sub_id,
                       ssize_t                             num_publications,
                       const TAO::DCPS::AssociationData*   publications)
{
  // TURN_ON_VERBOSE_DEBUG ;
  DBG_ENTRY("SimpleSubscriber","init");

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
             "Use TheTransportFactory to obtain() the TransportImpl object "
             "that it knows as transport_id (%d).\n", transport_id));

  // Obtain the transport.
  TAO::DCPS::TransportImpl_rch transport =
                                    TheTransportFactory->obtain(transport_id);

  if (transport.is_nil())
    {
      // Failed to obtain the transport.
      ACE_ERROR((LM_ERROR,
                 "(%P|%t) Failed to obtain TransportImpl (id %d) from "
                 "TheTransportFactory.\n", transport_id));
      throw TestException();
    }

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
             "Attach ourselves (SimpleSubscriber) to the TransportImpl.\n"));

  // Attempt to attach the transport to ourselves.
  TAO::DCPS::AttachStatus status = this->attach_transport(transport.in());

  if (status != TAO::DCPS::ATTACH_OK)
    {
      // We failed to attach to the transport for some reason.
      std::string status_str;

      switch (status)
        {
          case TAO::DCPS::ATTACH_BAD_TRANSPORT:
            status_str = "ATTACH_BAD_TRANSPORT";
            break;
          case TAO::DCPS::ATTACH_ERROR:
            status_str = "ATTACH_ERROR";
            break;
          case TAO::DCPS::ATTACH_INCOMPATIBLE_QOS:
            status_str = "ATTACH_INCOMPATIBLE_QOS";
            break;
          default:
            status_str = "Unknown Status";
            break;
        }

      ACE_ERROR((LM_ERROR,
                 "(%P|%t) Failed to attach to the transport. "
                 "AttachStatus == %s\n", status_str.c_str()));
      throw TestException();
    }

  // Good!  We are now attached to the transport.
  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
             "SimpleSubscriber is now attached to the TransportImpl.\n"));

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
             "Initialize our SimpleDataReader object.\n"));

  // Initialize our DataReader.
  this->reader_.init(sub_id);

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
             "Add the publications.\n"));

  // Add the association between the local sub_id and the remote pub_id
  // to the transport via the TransportInterface.
  int result = this->add_publications(sub_id,
                                      &this->reader_,
                                      0,   /* priority */
                                      num_publications,
                                      publications);

  if (result != 0)
    {
      ACE_ERROR((LM_ERROR,
                 "(%P|%t) Failed to add publications to the "
                 "TransportInterface.\n"));
      throw TestException();
    }

  VDBG((LM_DEBUG, "(%P|%t) DBG:   "
             "The publications have been added successfully.\n"));
}


void
SimpleSubscriber::transport_detached_i()
{
  DBG_ENTRY("SimpleSubscriber","transport_detached_i");

  ACE_DEBUG((LM_DEBUG,
             "(%P|%t) Transport has detached from SimpleSubscriber.\n"));

  this->reader_.transport_lost();
}


int
SimpleSubscriber::received_test_message() const
{
  return this->reader_.received_test_message();
}
