// -*- C++ -*-
// $Id$

#include "Subscriber.h"

#include "Test.h"
#include "Options.h"

#include "TestTypeSupportImpl.h"

#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/DataReaderImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "dds/DCPS/RepoIdConverter.h"
#include "dds/DCPS/transport/framework/TheTransportFactory.h"
#include "dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h"
#include "dds/DCPS/transport/simpleUnreliableDgram/SimpleUdpConfiguration.h"
#include "dds/DCPS/transport/simpleUnreliableDgram/SimpleMcastConfiguration.h"
#include "dds/DCPS/transport/ReliableMulticast/ReliableMulticastTransportConfiguration.h"

#ifdef ACE_AS_STATIC_LIBS
#include "dds/DCPS/transport/simpleTCP/SimpleTcp.h"
#endif

#include <sstream>

namespace Test {

Subscriber::~Subscriber()
{
  this->waiter_->detach_condition( this->status_.in());

  if( ! CORBA::is_nil( this->participant_.in())) {
    this->participant_->delete_contained_entities();
    TheParticipantFactory->delete_participant( this->participant_.in());
  }
  TheTransportFactory->release();
  TheServiceParticipant->shutdown();
}

Subscriber::Subscriber( const Options& options)
 : options_( options),
   waiter_( new DDS::WaitSet)
{
  // Create the DomainParticipant
  this->participant_
    = TheParticipantFactory->create_participant(
        this->options_.domain(),
        PARTICIPANT_QOS_DEFAULT,
        DDS::DomainParticipantListener::_nil()
      );
  if( CORBA::is_nil( this->participant_.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Subscriber::Subscriber() - ")
      ACE_TEXT("failed to create a participant.\n")
    ));
    throw BadParticipantException();

  } else if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Subscriber::Subscriber() - ")
      ACE_TEXT("created participant in domain %d.\n"),
      this->options_.domain()
    ));
  }

  // Create and register the type support.
  DataTypeSupportImpl* testData = new DataTypeSupportImpl();
  if( ::DDS::RETCODE_OK
   != testData->register_type( this->participant_.in(), 0)) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Subscriber::Subscriber() - ")
      ACE_TEXT("unable to install type %C support.\n"),
      testData->get_type_name()
    ));
    throw BadTypeSupportException ();

  } else if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Subscriber::Subscriber() - ")
      ACE_TEXT("created type %C support.\n"),
      testData->get_type_name()
    ));
  }

  // Create the topic.
  DDS::Topic_var topic = this->participant_->create_topic(
                           this->options_.topicName().c_str(),
                           testData->get_type_name(),
                           TOPIC_QOS_DEFAULT,
                           ::DDS::TopicListener::_nil()
                         );
  if( CORBA::is_nil( topic.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Subscriber::Subscriber() - ")
      ACE_TEXT("failed to create topic %C.\n"),
      this->options_.topicName().c_str()
    ));
    throw BadTopicException();

  } else if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Subscriber::Subscriber() - ")
      ACE_TEXT("created topic %C.\n"),
      this->options_.topicName().c_str()
    ));
  }

  // Create the subscriber.
  DDS::Subscriber_var subscriber
    = this->participant_->create_subscriber(
        SUBSCRIBER_QOS_DEFAULT,
        ::DDS::SubscriberListener::_nil()
      );
  if( CORBA::is_nil( subscriber.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Subscriber::Subscriber() - ")
      ACE_TEXT("failed to create subscriber.\n")
    ));
    throw BadSubscriberException();

  } else if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Subscriber::Subscriber() - ")
      ACE_TEXT("created subscriber.\n")
    ));
  }

  // Create the transport.
  OpenDDS::DCPS::TransportImpl_rch transport
    = TheTransportFactory->create_transport_impl(
        this->options_.transportKey(),
        OpenDDS::DCPS::AUTO_CONFIG
      );
  if( transport.is_nil()) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Subscriber::Subscriber() - ")
      ACE_TEXT("failed to create transport.\n")
    ));
    throw BadTransportException();

  } else if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Subscriber::Subscriber() - ")
      ACE_TEXT("created transport.\n")
    ));
  }

  // Attach the transport.
  if( ::OpenDDS::DCPS::ATTACH_OK
   != transport->attach( subscriber.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Subscriber::Subscriber() - ")
      ACE_TEXT("failed to attach subscriber to transport.\n")
    ));
    throw BadAttachException();

  } else if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Subscriber::Subscriber() - ")
      ACE_TEXT("attached subscriber to transport.\n")
    ));
  }

  // Reader Qos policy values.
  ::DDS::DataReaderQos readerQos;
  subscriber->get_default_datareader_qos( readerQos);
  readerQos.reliability.kind = ::DDS::RELIABLE_RELIABILITY_QOS;

  // Create the readers.
  for( int index = 0; index < 2; ++index) {
    ::DDS::DataReader_var reader
      = subscriber->create_datareader(
          topic.in(),
          readerQos,
          DDS::DataReaderListener::_nil()
        );
    if( CORBA::is_nil( reader.in())) {
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: Subscriber::Subscriber() - ")
        ACE_TEXT("failed to create reader.\n")
      ));
      throw BadReaderException();

    } else if( this->options_.verbose()) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) Subscriber::Subscriber() - ")
        ACE_TEXT("created reader.\n")
      ));
    }

    this->reader_[ index] = ::OpenDDS::DCPS::DataReaderEx::_narrow( reader.in());
    if( CORBA::is_nil( this->reader_[ index].in())) {
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: Subscriber::Subscriber() - ")
        ACE_TEXT("failed to narrow reader to extract statistics.\n")
      ));
      throw BadReaderException();
    }

    // Grab, enable and attach the status condition for test synchronization.
    this->status_ = this->reader_[ index]->get_statuscondition();
    this->status_->set_enabled_statuses( DDS::SUBSCRIPTION_MATCH_STATUS);
    this->waiter_->attach_condition( this->status_.in());

    if( this->options_.verbose()) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) Subscriber::Subscriber() - ")
        ACE_TEXT("created StatusCondition and WaitSet for test synchronization.\n")
      ));
    }
  }
}

void
Subscriber::run()
{
  DDS::Duration_t   timeout = { DDS::DURATION_INFINITY_SEC, DDS::DURATION_INFINITY_NSEC};
  DDS::ConditionSeq conditions;
  DDS::SubscriptionMatchStatus matches = { 0, 0, 0, 0, 0};
  int current_count = 0;
  do {
    if( this->options_.verbose()) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) Subscriber::run() - ")
        ACE_TEXT("%d publications attached.\n"),
        matches.current_count
      ));
    }
    if( DDS::RETCODE_OK != this->waiter_->wait( conditions, timeout)) {
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: Subscriber::run() - ")
        ACE_TEXT("failed to synchronize at start of test.\n")
      ));
      throw BadSyncException();
    }
    matches = this->reader_[ 0]->get_subscription_match_status();
    current_count = matches.current_count;

    matches = this->reader_[ 1]->get_subscription_match_status();
    current_count += matches.current_count;

  } while( current_count > 0);

  if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Subscriber::run() - ")
      ACE_TEXT("shutting down after all publications were removed.\n")
    ));
  }
}

} // End of namespace Test

