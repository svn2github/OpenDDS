// -*- C++ -*-
// ============================================================================
/**
 *  @file   subscriber.cpp
 *
 *  $Id$
 *
 *
 */
// ============================================================================


#include "DataReaderListener.h"
#include "MessageTypeSupportImpl.h"
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/SubscriberImpl.h>
#include <dds/DCPS/Qos_Helper.h>
#include <dds/DCPS/transport/framework/TheTransportFactory.h>
#include <dds/DCPS/transport/simpleTCP/SimpleTcpConfiguration.h>

#ifdef ACE_AS_STATIC_LIBS
#include <dds/DCPS/transport/simpleTCP/SimpleTcp.h>
#endif

#include <ace/streams.h>
#include <ace/Time_Value.h>

#include <cassert>

OpenDDS::DCPS::TransportIdType transport_impl_id = 1;

int ACE_TMAIN (int argc, ACE_TCHAR *argv[])
{
  try
    {
      DDS::DomainParticipantFactory_var dpf;
      DDS::DomainParticipant_var participant;

      dpf = TheParticipantFactoryWithArgs(argc, argv);
      participant =
        dpf->create_participant(411,
                                PARTICIPANT_QOS_DEFAULT,
                                DDS::DomainParticipantListener::_nil());
      if (CORBA::is_nil (participant.in ())) {
        cerr << "create_participant failed." << endl;
        return 1 ;
      }

      Messenger::MessageTypeSupportImpl* mts_servant =
        new Messenger::MessageTypeSupportImpl;

      if (DDS::RETCODE_OK != mts_servant->register_type(participant.in (),
                                                        ""))
      {
        cerr << "Failed to register the MessageTypeTypeSupport." << endl;
        exit(1);
      }

      CORBA::String_var type_name = mts_servant->get_type_name ();

      DDS::TopicQos topic_qos;
      participant->get_default_topic_qos(topic_qos);
      DDS::Topic_var topic =
        participant->create_topic("Movie Discussion List",
                                  type_name.in (),
                                  topic_qos,
                                  DDS::TopicListener::_nil());
      if (CORBA::is_nil (topic.in ())) {
        cerr << "Failed to create_topic." << endl;
        exit(1);
      }

      // Initialize the transport
      OpenDDS::DCPS::TransportImpl_rch tcp_impl = 
        TheTransportFactory->create_transport_impl (
          transport_impl_id, 
          ::OpenDDS::DCPS::AUTO_CONFIG);

      // Create the subscriber and attach to the corresponding
      // transport.
      DDS::Subscriber_var sub =
        participant->create_subscriber (SUBSCRIBER_QOS_DEFAULT,
                                        DDS::SubscriberListener::_nil());
      if (CORBA::is_nil (sub.in ())) {
        cerr << "Failed to create_subscriber." << endl;
        exit(1);
      }

      // Attach the subscriber to the transport.
      OpenDDS::DCPS::SubscriberImpl* sub_impl =
        dynamic_cast<OpenDDS::DCPS::SubscriberImpl*> (sub.in ());
      if (0 == sub_impl) {
        cerr << "Failed to obtain subscriber servant\n" << endl;
        exit(1);
      }

      OpenDDS::DCPS::AttachStatus const status =
        sub_impl->attach_transport(tcp_impl.in());
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
      {
        // Attempt to create a DataReader with intentionally
        // incompatible QoS.
        DDS::DataReaderQos bogus_qos;
        sub->get_default_datareader_qos (bogus_qos);
      
        // Set up a 1 second recurring deadline.  DataReader creation
        // should fail with this QoS since the requested deadline period
        // will be less than the test configured offered deadline
        // period.
        bogus_qos.deadline.period.sec = 2;
        bogus_qos.deadline.period.nanosec = 0;

        DDS::DataReader_var tmp_dr =
          sub->create_datareader (topic.in (),
                                  bogus_qos,
                                  DDS::DataReaderListener::_nil ());

        if (CORBA::is_nil (tmp_dr.in ()))
        {
          cerr << "ERROR: DataReader creation with bogus QoS failed."
               << endl;
          exit (1);
        }

        ACE_OS::sleep (2);

        // Check if the incompatible deadline was correctly flagged.
        DDS::RequestedIncompatibleQosStatus_var incompatible_status =
          tmp_dr->get_requested_incompatible_qos_status ();

        DDS::QosPolicyCountSeq const & policies =
          incompatible_status->policies;

        bool incompatible_deadline = false;
        CORBA::ULong const len = policies.length ();
        for (CORBA::ULong i = 0; i < len; ++i)
        {
          if (policies[i].policy_id == DDS::DEADLINE_QOS_POLICY_ID)
          {
            incompatible_deadline = true;
            break;
          }
        }

        if (!incompatible_deadline)
        {
          cerr << "ERROR: A DataReader/Writer association was created " << endl
               << "       despite use of deliberately incompatible deadline "
               << "QoS." << endl;
          exit (1);
        }
      }


      // ----------------------------------------------

      // Create the listener.
      DDS::DataReaderListener_var listener (new DataReaderListenerImpl);
      if (CORBA::is_nil (listener.in ()))
      {
        cerr << "ERROR: listener is nil." << endl;
        exit(1);
      }


      DDS::DataReaderQos dr_qos; // Good QoS.
      sub->get_default_datareader_qos (dr_qos);

      // Set up a 5 second recurring deadline.
      static DDS::Duration_t const DEADLINE_PERIOD =
        {
          5,  // seconds
          0   // nanoseconds
        };

      assert (DEADLINE_PERIOD.sec > 1); // Requirement for the test.

      // Time to sleep waiting for deadline periods to expire
      long const NUM_EXPIRATIONS = 2;
      ACE_Time_Value const SLEEP_DURATION (
        OpenDDS::DCPS::duration_to_time_value (DEADLINE_PERIOD)
        * 2
        + ACE_Time_Value (1));

      dr_qos.deadline.period.sec     = DEADLINE_PERIOD.sec;
      dr_qos.deadline.period.nanosec = DEADLINE_PERIOD.nanosec;

      // First data reader will have a listener to test listener
      // callback on deadline expiration.
      DDS::DataReader_var dr1 =
        sub->create_datareader (topic.in (),
                                dr_qos,
                                listener.in ());

      // Second data reader will not have a listener to test proper
      // handling of a nil listener in the deadline handling code.
      DDS::DataReader_var dr2 =
        sub->create_datareader (topic.in (),
                                dr_qos,
                                DDS::DataReaderListener::_nil ());

      if (CORBA::is_nil (dr1.in ()) || CORBA::is_nil (dr2.in ()))
      {
        cerr << "ERROR: create_datareader failed." << endl;
        exit(1);
      }

      // ----------------------------------------------

      // Wait for deadline periods to expire.
      ACE_OS::sleep (SLEEP_DURATION);

      DDS::RequestedDeadlineMissedStatus deadline_status1 =
        dr1->get_requested_deadline_missed_status();

      DDS::RequestedDeadlineMissedStatus deadline_status2 =
        dr2->get_requested_deadline_missed_status();

      if (deadline_status1.total_count != NUM_EXPIRATIONS
          || deadline_status2.total_count != NUM_EXPIRATIONS)
      {
        cerr << "ERROR: Expected number of missed requested "
             << "deadlines (" << NUM_EXPIRATIONS << ") " << "did " << endl
             << "       not occur ("
             << deadline_status1.total_count << " and/or "
             << deadline_status2.total_count << ")." << endl;

        exit (1);
      }

      if (deadline_status1.total_count_change != NUM_EXPIRATIONS
          || deadline_status2.total_count_change != NUM_EXPIRATIONS)
      {
        cerr << "ERROR: Incorrect missed requested "
             << "deadline count change" << endl
             << "       ("
             << deadline_status1.total_count_change
             << " and/or "
             << deadline_status2.total_count_change
             << " instead of " << NUM_EXPIRATIONS << ")."
             << endl;

        exit (1);
      }

      // Wait for another set of deadline periods to expire.
      ACE_OS::sleep (SLEEP_DURATION);

      deadline_status1 = dr1->get_requested_deadline_missed_status();
      deadline_status2 = dr2->get_requested_deadline_missed_status();

      if (deadline_status1.total_count != NUM_EXPIRATIONS * 2
          || deadline_status2.total_count != NUM_EXPIRATIONS * 2)
      {
        cerr << "ERROR: Another expected number of missed requested "
             << "deadlines (" << NUM_EXPIRATIONS * 2 << ")" << endl
             << "       did not occur ("
             << deadline_status1.total_count << " and/or "
             << deadline_status2.total_count << ")." << endl;

        exit (1);
      }

      if (deadline_status1.total_count_change != NUM_EXPIRATIONS
          || deadline_status2.total_count_change != NUM_EXPIRATIONS)
      {
        cerr << "ERROR: Incorrect missed requested "
             << "deadline count" << endl
             << "       change ("
             << deadline_status1.total_count_change
             << "and/or "
             << deadline_status2.total_count_change
             << " instead of " << NUM_EXPIRATIONS << ")." << endl;

        exit (1);
      }


      // Create 3rd data reader to trigger the data writers to write.
      DDS::DataReader_var dr3 =
      sub->create_datareader (topic.in (),
                              dr_qos,
                              DDS::DataReaderListener::_nil ());

      DataReaderListenerImpl* listener_servant =
        dynamic_cast<DataReaderListenerImpl*>(listener.in());
      int expected = 10;
      while ( listener_servant->num_reads() < expected) {
        ACE_OS::sleep (1);
      }



      // @todo We still need a check proper updating of the
      //       @c DDS::RequestedDeadlineMissedStatus::last_instance_handle
      //       field.

      if (!CORBA::is_nil (participant.in ())) {
        participant->delete_contained_entities();
      }
      if (!CORBA::is_nil (dpf.in ())) {
        dpf->delete_participant(participant.in ());
      }

      ACE_OS::sleep(2);

      TheTransportFactory->release();
      TheServiceParticipant->shutdown ();
    }
  catch (CORBA::Exception& e)
    {
      cerr << "SUB: Exception caught in main ():" << endl << e << endl;
      return 1;
    }

  return 0;
}
