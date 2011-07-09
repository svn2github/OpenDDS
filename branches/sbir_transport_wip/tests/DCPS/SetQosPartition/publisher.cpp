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

#include "MessengerTypeSupportImpl.h"
#include "Writer.h"
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/PublisherImpl.h>
#include <dds/DCPS/Qos_Helper.h>
#include <dds/DCPS/transport/framework/TheTransportFactory.h>
#include <dds/DCPS/transport/tcp/TcpInst.h>

#ifdef ACE_AS_STATIC_LIBS
#include <dds/DCPS/transport/tcp/Tcp.h>
#endif

#include <ace/streams.h>
#include "ace/Get_Opt.h"

#include <memory>

using namespace Messenger;

const char PARTITION_A[] = "ZiggieStardust";
const char PARTITION_B[] = "Amadeus";


OpenDDS::DCPS::TransportIdType transport_impl_id = 1;


int ACE_TMAIN (int argc, ACE_TCHAR *argv[]){
  try
    {
      DDS::DomainParticipantFactory_var dpf =
        TheParticipantFactoryWithArgs(argc, argv);

      DDS::DomainParticipant_var participant =
        dpf->create_participant(411,
                                PARTICIPANT_QOS_DEFAULT,
                                DDS::DomainParticipantListener::_nil(),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (participant.in ())) {
        cerr << "create_participant failed." << endl;
        return 1;
      }

      MessageTypeSupportImpl* servant = new MessageTypeSupportImpl();

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
                                   DDS::TopicListener::_nil(),
                                   ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (topic.in ())) {
        cerr << "create_topic failed." << endl;
        exit(1);
      }

      OpenDDS::DCPS::TransportImpl_rch tcp_impl =
        TheTransportFactory->create_transport_impl (transport_impl_id,
                                                    ::OpenDDS::DCPS::AUTO_CONFIG);

      DDS::PublisherQos pub_qos;
      participant->get_default_publisher_qos (pub_qos);

      pub_qos.partition.name.length (1);
      pub_qos.partition.name[0] = PARTITION_A;

      DDS::Publisher_var pub =
        participant->create_publisher(pub_qos, DDS::PublisherListener::_nil(),
                                      ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (pub.in ())) {
        cerr << "create_publisher failed." << endl;
        exit(1);
      }

      // Attach the publisher to the transport.
      OpenDDS::DCPS::PublisherImpl* const pub_impl =
        dynamic_cast<OpenDDS::DCPS::PublisherImpl*> (pub.in());
      if (pub_impl == 0) {
        cerr << "Failed to obtain publisher servant" << endl;
        exit(1);
      }

      OpenDDS::DCPS::AttachStatus const status =
        pub_impl->attach_transport(tcp_impl.in());
      if (status != OpenDDS::DCPS::ATTACH_OK) {
        std::string status_str;
        switch (status) {
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
        cerr << "Failed to attach to the transport. Status == "
          << status_str.c_str() << endl;
        exit(1);
      }

      // ----------------------------------------------
      // Create DataWriter which is belongs to PARTITION_A
      DDS::DataWriter_var dw =
        pub->create_datawriter (topic.in (),
                                DATAWRITER_QOS_DEFAULT,
                                DDS::DataWriterListener::_nil (),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

      int const max_attempts = 15;
      int attempts = 1;

      // ----------------------------------------------
      // Wait for first DataReader that belongs to PARTITION_A too,
      // then write samples.

      // cache handle for first reader.
      ::DDS::InstanceHandle_t handle = -1;
      {
        std::auto_ptr<Writer> writer (new Writer (dw.in ()));

        while (attempts != max_attempts)
        {
          ::DDS::InstanceHandleSeq handles;
          dw->get_matched_subscriptions(handles);
          if (handles.length() == 1)
          {
            handle = handles[0];
            break;
          }
          else
            ACE_OS::sleep(1);
          ++attempts;
        }

        if (attempts == max_attempts)
        {
          cerr << "ERROR: failed to wait for first DataReader." << endl;
          exit (1);
        }

        writer->start ();
        writer->end ();
      }

      // ----------------------------------------------
      // Switch from PARTITION A to B, now the first DataReader belong to
      // PARTITION A should be disconnected and the second DataReader belong to
      // PARTITION B should be connected.

      pub_qos.partition.name[0] = PARTITION_B;
      if (pub->set_qos (pub_qos)!= ::DDS::RETCODE_OK)
      {
        cerr << "ERROR: DataWriter changed partition which should be compatible "
          << "but should disconnect with DataReaders" << endl;

        exit (1);
      }


      // ----------------------------------------------
      // Now DataWriter is in PARTITION B, the second DataReader in PARTITION B
      // should receive the messages.
      {
        std::auto_ptr<Writer> writer (new Writer (dw.in ()));

        attempts = 1;
        while (attempts != max_attempts)
        {
          ::DDS::InstanceHandleSeq handles;
          dw->get_matched_subscriptions(handles);
          if (handles.length() == 1 && handles[0] != handle)
            break;
          else
            ACE_OS::sleep(1);
          ++attempts;
        }

        if (attempts == max_attempts)
        {
          cerr << "ERROR: subscriptions failed to match." << endl;
          exit (1);
        }

        writer->start ();
        writer->end ();
      }

      // ----------------------------------------------
      // Wait for first reader to switch from PARTITION A to B so
      // both two readers will receive the messages.
      {
        std::auto_ptr<Writer> writer (new Writer (dw.in ()));

        attempts = 1;
        while (attempts != max_attempts)
        {
          ::DDS::InstanceHandleSeq handles;

          dw->get_matched_subscriptions(handles);
          if (handles.length() == 2)
            break;
          else
            ACE_OS::sleep(1);

          ++ attempts;
        }

        if (attempts == max_attempts)
        {
          cerr << "ERROR: failed to wait for DataReader exit." << endl;
          exit (1);
        }

        writer->start ();
        writer->end ();
      }

      // ----------------------------------------------
      // Now wait for subscriber exit.
      {
        attempts = 1;
        while (attempts != max_attempts)
        {
          ::DDS::InstanceHandleSeq handles;

          dw->get_matched_subscriptions(handles);
          if (handles.length() == 0)
            break;
          else
            ACE_OS::sleep(1);

          ++ attempts;
        }
      }

      participant->delete_contained_entities();
      dpf->delete_participant(participant.in ());
      TheTransportFactory->release();
      TheServiceParticipant->shutdown ();
  }
  catch (CORBA::Exception& e)
  {
    cerr << "PUB: Exception caught in main.cpp:" << endl
         << e << endl;
    exit(1);
  }

  return 0;
}
