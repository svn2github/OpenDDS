// -*- C++ -*-
// $Id$

#include "Publisher.h"

#include "Test.h"
#include "Options.h"
#include "PublicationProfile.h"
#include "Writer.h"

#include "TestTypeSupportImpl.h"

#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/PublisherImpl.h"
#include "dds/DCPS/transport/framework/TheTransportFactory.h"
#include "dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h"
#include "dds/DCPS/transport/simpleUnreliableDgram/SimpleUdpConfiguration.h"
#include "dds/DCPS/transport/simpleUnreliableDgram/SimpleMcastConfiguration.h"
#include "dds/DCPS/transport/ReliableMulticast/ReliableMulticastTransportConfiguration.h"

#ifdef ACE_AS_STATIC_LIBS
#include "dds/DCPS/transport/simpleTCP/SimpleTcp.h"
#include "dds/DCPS/transport/simpleUnreliableDgram/SimpleUnreliableDgram.h"
#include "dds/DCPS/transport/ReliableMulticast/ReliableMulticast.h"
#endif

// #include "ace/Null_Mutex.h"
#include "ace/Condition_T.h"

#include <sstream>

namespace Test {

Publisher::~Publisher()
{
  this->waiter_->detach_condition( this->status_.in());

  if( ! CORBA::is_nil( this->participant_.in())) {
    this->participant_->delete_contained_entities();
    TheParticipantFactory->delete_participant( this->participant_.in());
  }
  TheTransportFactory->release();
  TheServiceParticipant->shutdown();
}

Publisher::Publisher( const Options& options)
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
      ACE_TEXT("(%P|%t) ERROR: Publisher::Publisher() - ")
      ACE_TEXT("failed to create a participant.\n")
    ));
    throw BadParticipantException();

  } else if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Publisher::Publisher() - ")
      ACE_TEXT("created participant in domain %d.\n"),
      this->options_.domain()
    ));
  }

  // Create the transport.
  this->transport_
    = TheTransportFactory->create_transport_impl(
        this->options_.transportKey(),
        OpenDDS::DCPS::AUTO_CONFIG
      );
  if( this->transport_.is_nil()) {
    std::stringstream buffer;
    buffer << this->options_.transportType();
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Publisher::Publisher() - ")
      ACE_TEXT("failed to create %s transport.\n"),
      buffer.str().c_str()
    ));
    throw BadTransportException();

  } else if( this->options_.verbose()) {
    std::stringstream buffer;
    buffer << this->options_.transportType();
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Publisher::Publisher() - ")
      ACE_TEXT("created %s transport.\n"),
      buffer.str().c_str()
    ));
  }

  // Create and register the type support.
  DataTypeSupportImpl* testData = new DataTypeSupportImpl();
  if( ::DDS::RETCODE_OK
   != testData->register_type( this->participant_.in(), 0)) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Publisher::Publisher() - ")
      ACE_TEXT("unable to install type %s support.\n"),
      testData->get_type_name()
    ));
    throw BadTypeSupportException ();

  } else if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Publisher::Publisher() - ")
      ACE_TEXT("created type %s support.\n"),
      testData->get_type_name()
    ));
  }

  // Create the topic.
  this->topic_ = this->participant_->create_topic(
                   this->options_.topicName().c_str(),
                   testData->get_type_name(),
                   TOPIC_QOS_DEFAULT,
                   ::DDS::TopicListener::_nil()
                 );
  if( CORBA::is_nil( this->topic_.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Publisher::Publisher() - ")
      ACE_TEXT("failed to create topic %s.\n"),
      this->options_.topicName().c_str()
    ));
    throw BadTopicException();

  } else if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Publisher::Publisher() - ")
      ACE_TEXT("created topic %s.\n"),
      this->options_.topicName().c_str()
    ));
  }

  // Create the publisher.
  this->publisher_ = this->participant_->create_publisher(
                       PUBLISHER_QOS_DEFAULT,
                       ::DDS::PublisherListener::_nil()
                     );
  if( CORBA::is_nil( this->publisher_.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Publisher::Publisher() - ")
      ACE_TEXT("failed to create publisher.\n")
    ));
    throw BadPublisherException();

  } else if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Publisher::Publisher() - ")
      ACE_TEXT("created publisher.\n")
    ));
  }

  // Attach the transport to the publisher.
  ::OpenDDS::DCPS::PublisherImpl* servant
    = dynamic_cast< ::OpenDDS::DCPS::PublisherImpl*>( this->publisher_.in());
  if( 0 == servant) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Publisher::Publisher() - ")
      ACE_TEXT("failed to narrow publisher servant.\n")
    ));
    throw BadServantException();
  }

  if( ::OpenDDS::DCPS::ATTACH_OK
   != servant->attach_transport( this->transport_.in())) {
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: Publisher::Publisher() - ")
      ACE_TEXT("failed to attach transport to publisher.\n")
    ));
    throw BadAttachException();

  } else if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Publisher::Publisher() - ")
      ACE_TEXT("attached transport to publisher.\n")
    ));
  }

  // Writer Qos policy values.
  ::DDS::DataWriterQos writerQos;
  this->publisher_->get_default_datawriter_qos( writerQos);

  writerQos.durability.kind                          = ::DDS::TRANSIENT_LOCAL_DURABILITY_QOS;
  writerQos.history.kind                             = ::DDS::KEEP_ALL_HISTORY_QOS;
  writerQos.resource_limits.max_samples_per_instance = ::DDS::LENGTH_UNLIMITED;

  // Reliability varies with the transport implementation.
  switch( this->options_.transportType()) {
    case Options::TCP:
    case Options::RMC:
      writerQos.reliability.kind = ::DDS::RELIABLE_RELIABILITY_QOS;
      break;

    case Options::UDP:
    case Options::MC:
      writerQos.reliability.kind = ::DDS::BEST_EFFORT_RELIABILITY_QOS;
      break;

    case Options::NONE:
    default:
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: Publisher::Publisher() - ")
        ACE_TEXT("unrecognized transport when setting up Qos policies.\n")
      ));
      throw BadQosException();
  }

  if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Publisher::Publisher() - ")
      ACE_TEXT("starting to create %d publications.\n"),
      this->options_.profiles().size()
    ));
  }

  // Build as many publications as are specified.
  for( unsigned int index = 0; index < this->options_.profiles().size(); ++index) {
    // This publications priority is needed when creating the writer.
    writerQos.transport_priority.value = this->options_.profiles()[ index]->priority();

    // Create the writer.
    DDS::DataWriter_var writer
      = this->publisher_->create_datawriter(
          this->topic_.in(),
          writerQos,
          DDS::DataWriterListener::_nil()
        );
    if( CORBA::is_nil( writer.in())) {
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: Publisher::Publisher() - ")
        ACE_TEXT("failed to create writer.\n")
      ));
      throw BadWriterException();

    } else if( this->options_.verbose()) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) Publisher::Publisher() - ")
        ACE_TEXT("created writer for publication %s.\n"),
        this->options_.profiles()[ index]->name().c_str()
      ));
    }

    // Create a publication and store it.
    this->publications_[ this->options_.profiles()[ index]->name()]
      = new Writer(
              writer.in(),
              *this->options_.profiles()[ index],
              this->options_.verbose()
            );

    //
    // The first publication is fine for obtaining a status condition to
    // synchronize the beginning of publication.
    //
    if( CORBA::is_nil( this->status_)) {
      // Grab, enable and attach the status condition for test synchronization.
      this->writer_ = DDS::DataWriter::_duplicate( writer.in());
      this->status_ = writer->get_statuscondition();
      this->status_->set_enabled_statuses( DDS::PUBLICATION_MATCH_STATUS);
      this->waiter_->attach_condition( this->status_.in());

      if( this->options_.verbose()) {
        ACE_DEBUG((LM_DEBUG,
          ACE_TEXT("(%P|%t) Publisher::Publisher() - ")
          ACE_TEXT("created StatusCondition and WaitSet for test synchronization.\n")
        ));
      }
    }
  }
}

void
Publisher::run()
{
  DDS::Duration_t   timeout = { SUBSCRIPTION_WAIT_TIME, 0};
  DDS::ConditionSeq conditions;
  DDS::PublicationMatchStatus matches = { 0, 0, 0};
  do {
    if( this->options_.verbose()) {
      ACE_DEBUG((LM_DEBUG,
        ACE_TEXT("(%P|%t) Publisher::run() - ")
        ACE_TEXT("waiting for subscription to attach (total==%d, change==%d).\n"),
        matches.total_count,
        matches.total_count_change
      ));
    }
    if( DDS::RETCODE_OK != this->waiter_->wait( conditions, timeout)) {
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("(%P|%t) ERROR: Publisher::run() - ")
        ACE_TEXT("failed to synchronize at start of test.\n")
      ));
      throw BadSyncException();
    }
    matches = this->writer_->get_publication_match_status();

  } while( matches.total_count_change == 0);

  if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Publisher::run() - ")
      ACE_TEXT("starting to publish samples.\n")
    ));
  }

  for( PublicationMap::const_iterator current = this->publications_.begin();
       current != this->publications_.end();
       ++current
     ) {
    current->second->start();
  }

  // Execute test for specified duration, or block until terminated externally.
  if( this->options_.duration() > 0) {
    ACE_Time_Value duration( this->options_.duration(), 0);
    ACE_OS::sleep( duration);

  } else {
    // Block the main thread, leaving the others working.
    ACE_Thread_Manager::instance()->wait();
  }

  // Signal the writers to terminate.
  for( PublicationMap::const_iterator current = this->publications_.begin();
       current != this->publications_.end();
       ++current
     ) {
    current->second->stop();
  }

  // Separate loop so the termination messages can be handled concurrently.
  for( PublicationMap::const_iterator current = this->publications_.begin();
       current != this->publications_.end();
       ++current
     ) {
    // Join and clean up.
    delete current->second;
  }
  this->publications_.clear();

  if( this->options_.verbose()) {
    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("(%P|%t) Publisher::run() - ")
      ACE_TEXT("finished publishing samples.\n")
    ));
  }
  ACE_OS::sleep( 2); /// @TODO: Kluge around a shutdown race condition.
}

} // End of namespace Test

