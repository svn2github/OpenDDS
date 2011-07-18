// -*- C++ -*-
//
// $Id$
#include "SimplePublisher.h"
#include "dds/DCPS/transport/framework/TransportImpl.h"
#include "dds/DCPS/transport/framework/TransportRegistry.h"
#include "TestException.h"
#include <string>


SimplePublisher::SimplePublisher()
{
}


SimplePublisher::~SimplePublisher()
{
}


void
SimplePublisher::init(OpenDDS::DCPS::TransportIdType               transport_id,
                      OpenDDS::DCPS::RepoId                   pub_id,
                      ssize_t                             num_subscriptions,
                      const OpenDDS::DCPS::AssociationData*   subscriptions)
{
  // Obtain the transport.
  OpenDDS::DCPS::TransportImpl_rch transport =
                                    TheTransportFactory->obtain(transport_id);

  if (transport.is_nil())
    {
      // Failed to obtain the transport.
      ACE_ERROR((LM_ERROR,
                 "(%P|%t) Failed to obtain TransportImpl (id %d) from "
                 "TheTransportFactory.\n", transport_id));
      throw TestException();
    }

  // Attempt to attach the transport to ourselves.
  OpenDDS::DCPS::AttachStatus status = this->attach_transport(transport.in());

  if (status != OpenDDS::DCPS::ATTACH_OK)
    {
      // We failed to attach to the transport for some reason.
      ACE_TString status_str;

      switch (status)
        {
          case OpenDDS::DCPS::ATTACH_BAD_TRANSPORT:
            status_str = ACE_TEXT("ATTACH_BAD_TRANSPORT");
            break;
          case OpenDDS::DCPS::ATTACH_ERROR:
            status_str = ACE_TEXT("ATTACH_ERROR");
            break;
          case OpenDDS::DCPS::ATTACH_INCOMPATIBLE_QOS:
            status_str = ACE_TEXT("ATTACH_INCOMPATIBLE_QOS");
            break;
          default:
            status_str = ACE_TEXT("Unknown Status");
            break;
        }

      ACE_ERROR((LM_ERROR,
                 "(%P|%t) Failed to attach to the transport. "
                 "AttachStatus == %s\n", status_str.c_str()));
      throw TestException();
    }

  // Good!  We are now attached to the transport.

  // Initialize our DataWriter.
  this->writer_.init(pub_id);

  OpenDDS::DCPS::AssociationInfo info;
  info.num_associations_ = num_subscriptions;
  info.association_data_ =
    const_cast<OpenDDS::DCPS::AssociationData*>(subscriptions);

  // Add the association between the local pub_id and the remote sub_id
  // to the transport via the TransportInterface.
  int result = this->add_subscriptions (pub_id,
                                        info,
                                        0,   /* priority */
                                        0);

  if (result != 0)
    {
      ACE_ERROR((LM_ERROR,
                 "(%P|%t) Failed to add subscriptions to the "
                 "TransportInterface.\n"));
      throw TestException();
    }
}


int
SimplePublisher::run(unsigned num_messages)
{
  size_t sent =  this->writer_.run(this, num_messages);

  ACE_DEBUG((LM_DEBUG,
             "(%P|%t) Sent %d of %d samples.\n",
             sent, num_messages));

  return num_messages - sent ;
}


void
SimplePublisher::send_samples(const OpenDDS::DCPS::DataSampleList& samples)
{
  this->send(samples);
}


void
SimplePublisher::transport_detached_i()
{
  ACE_DEBUG((LM_DEBUG,
             "(%P|%t) Transport has detached from SimplePublisher.\n"));
  this->writer_.transport_lost();
}


int
SimplePublisher::delivered_test_message()
{
  return this->writer_.delivered_test_message();
}

void
SimplePublisher::remove_associations(ssize_t size,
                                     const OpenDDS::DCPS::RepoId* remote_ids,
                                     const OpenDDS::DCPS::RepoId pub_id)
{
  TransportInterface::remove_associations(size, remote_ids, pub_id, true);
}
