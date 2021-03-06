
#include "TestSystem.h"
#include "DataWriterListenerImpl.h"
#include "TestException.h"
#include "tests/DCPS/FooType5/FooNoKeyTypeSupportC.h"
#include "tests/DCPS/FooType5/FooNoKeyTypeSupportImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "dds/DCPS/PublisherImpl.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/transport/framework/TheTransportFactory.h"
#include "dds/DCPS/transport/framework/TransportImpl.h"
#include "dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h"
#include "ace/SString.h"

/**
 * @brief Construct a test system from the command line.
 */
TestSystem::TestSystem( int argc, char** argv, char** envp)
 : config_( argc, argv, envp)
{
  // Grab a local reference to the factory to ensure that we perform the
  // operations - they should not get optimized away!  Note that this
  // passes the arguments to the ORB initialization first, then strips
  // the DCPS arguments.
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: initializing the service.\n")));
  ::DDS::DomainParticipantFactory_var factory = TheParticipantFactoryWithArgs( argc, argv);

  // We only need to do loading and binding if we receive InfoRepo IOR
  // values on the command line - otherwise this is done during
  // configuration file processing.
  if( this->config_.infoRepoIorSize() > 0) {

    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("%T (%P|%t) INFO: loading repository %d.\n"),
      OpenDDS::DCPS::Service_Participant::DEFAULT_REPO
    ));
    TheServiceParticipant->set_repo_ior( this->config_.infoRepoIor().c_str());

    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("%T (%P|%t) INFO: binding subscriber domain to repository %d.\n"),
      OpenDDS::DCPS::Service_Participant::DEFAULT_REPO
    ));
    TheServiceParticipant->set_repo_domain(
      this->config_.subscriberDomain(),
      OpenDDS::DCPS::Service_Participant::DEFAULT_REPO
    );

    ACE_DEBUG((LM_DEBUG,
      ACE_TEXT("%T (%P|%t) INFO: binding publisher domain to repository %d.\n"),
      OpenDDS::DCPS::Service_Participant::DEFAULT_REPO
    ));
    TheServiceParticipant->set_repo_domain(
      this->config_.publisherDomain(),
      OpenDDS::DCPS::Service_Participant::DEFAULT_REPO
    );
  }

  //
  // Establish a DomainParticipant for the subscription domain.
  //

  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("%T (%P|%t) INFO: creating subscriber participant in domain %d.\n"),
    this->config_.subscriberDomain()
  ));
  this->subscriberParticipant_ =
    factory->create_participant(
      this->config_.subscriberDomain(),
      PARTICIPANT_QOS_DEFAULT,
      ::DDS::DomainParticipantListener::_nil()
    );
  if (CORBA::is_nil (this->subscriberParticipant_.in ()))
    {
      ACE_ERROR ((LM_ERROR,
                ACE_TEXT("%T (%P|%t) ERROR: create_participant failed for subscriber.\n")));
      throw BadParticipantException ();
    }

  //
  // Establish a DomainParticipant for the publication domain.
  //

  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("%T (%P|%t) INFO: creating publisher participant in domain %d.\n"),
    this->config_.publisherDomain()
  ));
  if( this->config_.publisherDomain() == this->config_.subscriberDomain()) {
    //
    // Just reference the same participant for both activities if they
    // are in the same domain.
    //
    this->publisherParticipant_
      = ::DDS::DomainParticipant::_duplicate(this->subscriberParticipant_.in());

  } else {
    this->publisherParticipant_ =
      factory->create_participant(
        this->config_.publisherDomain(),
        PARTICIPANT_QOS_DEFAULT,
        ::DDS::DomainParticipantListener::_nil()
      );
    if (CORBA::is_nil (this->publisherParticipant_.in ()))
      {
        ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("%T (%P|%t) ERROR: create_participant failed for publisher.\n")));
        throw BadParticipantException ();
      }

  }

  //
  // Grab and install the transport implementation.
  // NOTE: Try to use a single transport first.  This is something that
  //       needs to be verified and documented.
  //

  // Establish debug level.
  if( this->config_.verbose()) {
    TURN_ON_VERBOSE_DEBUG;
  }

  if( TheTransportFactory->obtain( TestConfig::SubscriberTransport).is_nil()) {
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: creating a SimpleTCP transport.\n")));
    this->transport_
      = TheTransportFactory->create_transport_impl(
          TestConfig::SubscriberTransport,
          "SimpleTcp",
          OpenDDS::DCPS::DONT_AUTO_CONFIG
        );

    OpenDDS::DCPS::TransportConfiguration_rch reader_config
      = TheTransportFactory->create_configuration(
          TestConfig::SubscriberTransport,
          ACE_TString("SimpleTcp")
        );

    OpenDDS::DCPS::SimpleTcpConfiguration* reader_tcp_config
      = static_cast <OpenDDS::DCPS::SimpleTcpConfiguration*>( reader_config.in() );

    if( this->config_.transportAddressName().length() > 0)
    {
      ACE_INET_Addr reader_address( this->config_.transportAddressName().c_str());
      reader_tcp_config->local_address_ = reader_address;
      reader_tcp_config->local_address_str_ = this->config_.transportAddressName();
    }
    // else use default address - OS assigned.

    if( this->transport_->configure( reader_config.in()) != 0)
    {
      ACE_ERROR((LM_ERROR,
        ACE_TEXT("%T (%P|%t) ERROR: subscriber TCP ")
        ACE_TEXT("failed to configure the transport.\n")));
      throw BadTransportException ();
    }
  }

  //
  // Establish a listener to install into the DataReader.
  //

  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: creating data reader listener.\n")));
  this->listener_ = new ForwardingListenerImpl( 0);
  ForwardingListenerImpl* forwarder_servant =
    dynamic_cast<ForwardingListenerImpl*>(listener_.in());

  if (CORBA::is_nil (this->listener_.in ()))
    {
      ACE_ERROR ((LM_ERROR, ACE_TEXT ("%T (%P|%t) ERROR: listener is nil.\n")));
      throw BadReaderListenerException ();
    }

  //
  // Establish the Type Support for the data.
  // MJM: Do we need to have two distinct instantiations here?  Or can we
  //      share the servant across domains?
  //

  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("%T (%P|%t) INFO: creating subscription type support for type %s.\n"),
    this->config_.typeName().c_str()
  ));
  ::Xyz::FooNoKeyTypeSupportImpl* subscriber_data = new ::Xyz::FooNoKeyTypeSupportImpl();
  if(::DDS::RETCODE_OK != subscriber_data->register_type(
                            this->subscriberParticipant_.in (),
                            this->config_.typeName().c_str()
                          )
    ) {
    ACE_ERROR ((LM_ERROR,
      ACE_TEXT ("%T (%P|%t) ERROR: Failed to register the subscriber FooNoKeyTypeSupport.\n")));
    throw TestException ();
  }

  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("%T (%P|%t) INFO: creating publication type support for type %s.\n"),
    this->config_.typeName().c_str()
  ));
  ::Xyz::FooNoKeyTypeSupportImpl* publisher_data = new ::Xyz::FooNoKeyTypeSupportImpl();
  if(::DDS::RETCODE_OK != publisher_data->register_type(
                            this->publisherParticipant_.in (),
                            this->config_.typeName().c_str()
                          )
    ) {
    ACE_ERROR ((LM_ERROR,
      ACE_TEXT ("%T (%P|%t) ERROR: Failed to register the publisher FooNoKeyTypeSupport.\n")));
    throw TestException ();
  }

  //
  // Establish the Subscriber and Publisher Topics.
  //

  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("%T (%P|%t) INFO: creating subscription topic: %s.\n"),
    this->config_.readerTopicName().c_str()
  ));
  this->readerTopic_
    = this->subscriberParticipant_->create_topic(
        this->config_.readerTopicName().c_str(),
        this->config_.typeName().c_str(),
        TOPIC_QOS_DEFAULT,
        ::DDS::TopicListener::_nil()
      );
  if( CORBA::is_nil( this->readerTopic_.in()) )
    {
      ACE_ERROR ((LM_ERROR,
        ACE_TEXT ("%T (%P|%t) ERROR: Failed to create_topic for subscriber.\n")));
      throw BadTopicException ();
    }

  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("%T (%P|%t) INFO: creating publication topic: %s.\n"),
    this->config_.writerTopicName().c_str()
  ));
  this->writerTopic_
    = this->publisherParticipant_->create_topic(
        this->config_.writerTopicName().c_str(),
        this->config_.typeName().c_str(),
        TOPIC_QOS_DEFAULT,
        ::DDS::TopicListener::_nil()
      );
  if( CORBA::is_nil( this->readerTopic_.in()) )
    {
      ACE_ERROR ((LM_ERROR,
        ACE_TEXT ("%T (%P|%t) ERROR: Failed to create_topic for publisher.\n")));
      throw BadTopicException ();
    }

  //
  // Establish the Subscriber.
  //

  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: creating subscriber.\n")));
  this->subscriber_ = this->subscriberParticipant_->create_subscriber(
                        SUBSCRIBER_QOS_DEFAULT,
                        ::DDS::SubscriberListener::_nil());
  if (CORBA::is_nil (this->subscriber_.in ()))
    {
      ACE_ERROR ((LM_ERROR,
        ACE_TEXT ("%T (%P|%t) ERROR: Failed to create_subscriber.\n")));
      throw BadSubscriberException ();
    }

  // Attach the subscriber to the transport.
  OpenDDS::DCPS::SubscriberImpl* sub_impl
    = dynamic_cast<OpenDDS::DCPS::SubscriberImpl*>(this->subscriber_.in ());

  if (0 == sub_impl)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("%T (%P|%t) ERROR: Failed to obtain subscriber servant\n")));
      throw BadSubscriberException ();
    }

  OpenDDS::DCPS::AttachStatus attach_status;

  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: attaching subscriber to transport \n")));
  attach_status = sub_impl->attach_transport( this->transport_.in());

  if (attach_status != OpenDDS::DCPS::ATTACH_OK)
    {
      // We failed to attach to the transport for some reason.
      ACE_TString status_str;

      switch (attach_status)
        {
          case OpenDDS::DCPS::ATTACH_BAD_TRANSPORT:
            status_str = "ATTACH_BAD_TRANSPORT";
            break;
          case OpenDDS::DCPS::ATTACH_ERROR:
            status_str = "ATTACH_ERROR";
            break;
          case OpenDDS::DCPS::ATTACH_INCOMPATIBLE_QOS:
            status_str = "ATTACH_INCOMPATIBLE_QOS";
            break;
          default:
            status_str = "Unknown Status";
            break;
        }

      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("%T (%P|%t) ERROR: Failed to attach to the transport. ")
                  ACE_TEXT("AttachStatus == %s\n"),
                  status_str.c_str()));
      throw BadTransportException ();
    }

  //
  // Establish the Publisher.
  //

  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: creating publisher.\n")));
  this->publisher_ = this->publisherParticipant_->create_publisher(
                       PUBLISHER_QOS_DEFAULT,
                       ::DDS::PublisherListener::_nil());
  if (CORBA::is_nil (this->publisher_.in ()))
    {
      ACE_ERROR ((LM_ERROR,
        ACE_TEXT ("%T (%P|%t) ERROR: Failed to create_publisher.\n")));
      throw BadPublisherException ();
    }

  // Attach the publisher to the transport.
  OpenDDS::DCPS::PublisherImpl* pub_impl
    = dynamic_cast<OpenDDS::DCPS::PublisherImpl*>(this->publisher_.in ());

  if (0 == pub_impl)
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("%T (%P|%t) ERROR: Failed to obtain publisher servant\n")));
      throw BadPublisherException ();
    }

  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: attaching publisher to transport \n")));
  attach_status = pub_impl->attach_transport( this->transport_.in());

  if (attach_status != OpenDDS::DCPS::ATTACH_OK)
    {
      // We failed to attach to the transport for some reason.
      ACE_TString status_str;

      switch (attach_status)
        {
          case OpenDDS::DCPS::ATTACH_BAD_TRANSPORT:
            status_str = "ATTACH_BAD_TRANSPORT";
            break;
          case OpenDDS::DCPS::ATTACH_ERROR:
            status_str = "ATTACH_ERROR";
            break;
          case OpenDDS::DCPS::ATTACH_INCOMPATIBLE_QOS:
            status_str = "ATTACH_INCOMPATIBLE_QOS";
            break;
          default:
            status_str = "Unknown Status";
            break;
        }

      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("%T (%P|%t) ERROR: Failed to attach to the transport. ")
                  ACE_TEXT("AttachStatus == %s\n"),
                  status_str.c_str()));
      throw BadTransportException ();
    }

  //
  // Establish and install the DataWriter.
  //

  ::DDS::DataWriterListener_var listener (new DataWriterListenerImpl( 0));

  //
  // Keep all data samples to allow us to establish connections in an
  // arbitrary order, with samples being buffered at the first writer
  // that has not yet had a subscription match.
  //
  ::DDS::DataWriterQos writerQos;
  this->publisher_->get_default_datawriter_qos( writerQos);

  writerQos.durability.kind                          = ::DDS::TRANSIENT_LOCAL_DURABILITY_QOS;
  writerQos.history.kind                             = ::DDS::KEEP_ALL_HISTORY_QOS;
  writerQos.resource_limits.max_samples_per_instance = ::DDS::LENGTH_UNLIMITED;
  writerQos.reliability.kind                         = ::DDS::RELIABLE_RELIABILITY_QOS;
  writerQos.reliability.max_blocking_time.sec        = 0;
  writerQos.reliability.max_blocking_time.nanosec    = 0;

  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: creating data writer.\n")));
  this->dataWriter_ = this->publisher_->create_datawriter(
                        this->writerTopic_.in(),
                        writerQos,
                        listener.in()
                      );
  if( CORBA::is_nil( this->dataWriter_.in()) )
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("%T (%P|%t) ERROR: create_datawriter failed.\n")));
      throw BadWriterException ();
    }

  // Pass the writer into the forwarder.
  forwarder_servant->dataWriter( this->dataWriter_.in());

  //
  // Establish and install the DataReader.  This needs to be done after
  // the writer has been created and attached since it is possible (I
  // know, I've seen it happen!) that messages can be received and
  // forwarded between the time the reader is created and the writer is
  // created and attached to the forwarder if we do it the other way
  // round.
  //

  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: looking up subscription topic.\n")));
  ::DDS::TopicDescription_var description
    = this->subscriberParticipant_->lookup_topicdescription(
        this->config_.readerTopicName().c_str()
      );

  ACE_DEBUG((LM_DEBUG,
    ACE_TEXT("%T (%P|%t) INFO: creating data reader for topic: %s, type: %s.\n"),
    description->get_name(), description->get_type_name()
  ));
  this->dataReader_ = this->subscriber_->create_datareader(
                        description.in(),
                        DATAREADER_QOS_DEFAULT,
                        this->listener_.in ()
                      );
  if( CORBA::is_nil( this->dataReader_.in()) )
    {
      ACE_ERROR ((LM_ERROR,
                  ACE_TEXT("%T (%P|%t) ERROR: create_datareader failed.\n")));
      throw BadReaderException ();
    }

}

TestSystem::~TestSystem()
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: finalizing the publication apparatus.\n")));

  // Tear down the sending apparatus.
  if( ::DDS::RETCODE_PRECONDITION_NOT_MET
      == this->publisherParticipant_->delete_contained_entities()
    ) {
    ACE_ERROR ((LM_ERROR,
      ACE_TEXT("%T (%P|%t) ERROR: Unable to release the publication.\n")));

  } else {
    // Release publisher participant.
    if( ::DDS::RETCODE_PRECONDITION_NOT_MET
        == TheParticipantFactory->delete_participant( this->publisherParticipant_)
      ) {
      ACE_ERROR ((LM_ERROR,
        ACE_TEXT("%T (%P|%t) ERROR: Unable to release the publication participant.\n")));
    }
  }

  if( this->subscriberParticipant_.in() != this->publisherParticipant_.in()) {
    ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: finalizing separate subscription apparatus.\n")));

    // Tear down the receiving apparatus.
    if( ::DDS::RETCODE_PRECONDITION_NOT_MET
        == this->subscriberParticipant_->delete_contained_entities()
      ) {
      ACE_ERROR ((LM_ERROR,
        ACE_TEXT("%T (%P|%t) ERROR: Unable to release the subscription.\n")));

    } else {
      // Release subscriber participant.
      if( ::DDS::RETCODE_PRECONDITION_NOT_MET
          == TheParticipantFactory->delete_participant( this->subscriberParticipant_)
        ) {
        ACE_ERROR ((LM_ERROR,
          ACE_TEXT("%T (%P|%t) ERROR: Unable to release the subscription participant.\n")));
      }
    }
  }

  // Release any remaining resources held for the service.
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: finalizing DCPS service.\n")));
  TheServiceParticipant->shutdown ();

  // Release all the transport resources.
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: finalizing transport.\n")));
  TheTransportFactory->release();
}

void
TestSystem::run()
{
  ForwardingListenerImpl* forwarder
    = dynamic_cast< ForwardingListenerImpl*>(this->listener_.in ());
  forwarder->waitForCompletion();
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("%T (%P|%t) INFO: processing complete.\n")));
}

