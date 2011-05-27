// -*- C++ -*-
// ============================================================================
/**
 *  @file   publisher.cpp
 *
 *  $Id$
 *
 *
 */
// ============================================================================

#include "MessageTypeSupportImpl.h"
#include "Writer.h"
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/PublisherImpl.h>
#include <dds/DCPS/transport/framework/TheTransportFactory.h>
#include <dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h>
#include <ace/streams.h>

const TAO::DCPS::TransportIdType TCP_IMPL_ID = 1;

const char* pub_ready_filename    = "publisher_ready.txt";
const char* pub_finished_filename = "publisher_finished.txt";
const char* sub_ready_filename    = "subscriber_ready.txt";
const char* sub_finished_filename = "subscriber_finished.txt";


int main (int argc, char *argv[]) {
  try {
    DDS::DomainParticipantFactory_var dpf =
      TheParticipantFactoryWithArgs(argc, argv);
    DDS::DomainParticipant_var participant =
      dpf->create_participant(411,
                              PARTICIPANT_QOS_DEFAULT,
                              DDS::DomainParticipantListener::_nil());
    if (CORBA::is_nil (participant.in ())) {
      cerr << "create_participant failed." << endl;
      return 1;
    }

    MessageTypeSupportImpl* servant = new MessageTypeSupportImpl();
    PortableServer::ServantBase_var safe_servant = servant;

    if (DDS::RETCODE_OK != servant->register_type(participant.in (), "")) {
      cerr << "register_type failed." << endl;
      exit(1);
    }
  
    CORBA::String_var type_name = servant->get_type_name ();

    DDS::TopicQos topic_qos;
    participant->get_default_topic_qos(topic_qos);
    DDS::Topic_var topic =
      participant->create_topic ("Movie Discussion List",
                                 type_name.in (),
                                 topic_qos,
                                 DDS::TopicListener::_nil());
    if (CORBA::is_nil (topic.in ())) {
      cerr << "create_topic failed." << endl;
      exit(1);
    }

    TAO::DCPS::TransportImpl_rch tcp_impl =
      TheTransportFactory->create_transport_impl (TCP_IMPL_ID, 
                                                  ::TAO::DCPS::AUTO_CONFIG);

    DDS::Publisher_var pub =
      participant->create_publisher(PUBLISHER_QOS_DEFAULT,
                                    DDS::PublisherListener::_nil());
    if (CORBA::is_nil (pub.in ())) {
      cerr << "create_publisher failed." << endl;
      exit(1);
    }

    // Attach the publisher to the transport.
    TAO::DCPS::PublisherImpl* pub_impl =
      ::TAO::DCPS::reference_to_servant< TAO::DCPS::PublisherImpl, 
                                         DDS::Publisher_ptr>(pub.in ());
    if (0 == pub_impl) {
      cerr << "Failed to obtain publisher servant" << endl;
      exit(1);
    }

    TAO::DCPS::AttachStatus status = pub_impl->attach_transport(tcp_impl.in());
    if (status != TAO::DCPS::ATTACH_OK) {
      std::string status_str;
      switch (status) {
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
      cerr << "Failed to attach to the transport. Status == "
           << status_str.c_str() << endl;
      exit(1);
    }

    // Create the datawriter
    DDS::DataWriterQos dw_qos;
    pub->get_default_datawriter_qos (dw_qos);
    DDS::DataWriter_var dw =
      pub->create_datawriter(topic.in (),
                             dw_qos,
                             DDS::DataWriterListener::_nil());
    if (CORBA::is_nil (dw.in ())) {
      cerr << "create_datawriter failed." << endl;
      exit(1);
    }
    Writer* writer = new Writer(dw.in());

    // Indicate that the publisher is ready
    FILE* writers_ready = ACE_OS::fopen (pub_ready_filename, "w");
    if (writers_ready == 0) {
      cerr << "ERROR Unable to create publisher ready file" << endl;
      exit(1);
    }
    ACE_OS::fclose(writers_ready);

    // Wait for the subscriber to be ready.
    FILE* readers_ready = 0;
    do {
      ACE_Time_Value small(0,250000);
      ACE_OS::sleep (small);
      readers_ready = ACE_OS::fopen (sub_ready_filename, "r");
    } while (0 == readers_ready);
    ACE_OS::fclose(readers_ready);

    // ensure the associations are fully established before writing.
    ACE_OS::sleep(3);
    writer->start ();
    while ( !writer->is_finished()) {
      ACE_Time_Value small(0,250000);
      ACE_OS::sleep (small);
    }

    // Indicate that the publisher is done
    FILE* writers_completed = ACE_OS::fopen (pub_finished_filename, "w");
    if (writers_completed == 0) {
      cerr << "ERROR Unable to i publisher completed file" << endl;
    } else {
      ACE_OS::fprintf (writers_completed, "%d\n",
                       writer->get_timeout_writes());
    }
    ACE_OS::fclose (writers_completed);

    // Wait for the subscriber to finish.
    FILE* readers_completed = 0;
    do {
      ACE_Time_Value small(0,250000);
      ACE_OS::sleep (small);
      readers_completed = ACE_OS::fopen (sub_finished_filename, "r");
    } while (0 == readers_completed);
    ACE_OS::fclose(readers_completed);

    // Cleanup
    writer->end ();
    delete writer;
    participant->delete_contained_entities();
    dpf->delete_participant(participant.in ());
    TheTransportFactory->release();
    TheServiceParticipant->shutdown ();
  } catch (CORBA::Exception& e) {
    cerr << "Exception caught in main.cpp:" << endl
         << e << endl;
    exit(1);
  }

  return 0;
}
