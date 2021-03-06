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
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/Qos_Helper.h"
#include "dds/DCPS/TopicDescriptionImpl.h"
#include "dds/DCPS/SubscriberImpl.h"
#include "../TypeNoKeyBounded/PTDefTypeSupportImpl.h"
#include "dds/DCPS/transport/framework/EntryExit.h"

#include "ace/Arg_Shifter.h"




#include "common.h"


/// parse the command line arguments
int parse_args (int argc, ACE_TCHAR *argv[])
{
  ACE_Arg_Shifter arg_shifter (argc, argv);

  arg_shifter.ignore_arg (); // ignore the command - argv[0]
  while (arg_shifter.is_anything_left ())
  {
    // options:
    // -p  <num data writers>
    // -n  <num samples>
    // -d  <data size>
    // -a  <transport address>
    // -i  <num messagess between reads>
    // -msi <max samples per instance>
    // -mxs <max samples>
    // -mxi <max instances>
    // -z  <verbose transport debug>

    const ACE_TCHAR *currentArg = 0;

    if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-p"))) != 0)
    {
      num_datawriters = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-d"))) != 0)
    {
      int shift_bits = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
      DATA_SIZE = 1 << shift_bits;
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-n"))) != 0)
    {
      NUM_SAMPLES = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-i"))) != 0)
    {
      RECVS_BTWN_READS = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-a"))) != 0)
    {
      reader_address_str = currentArg;
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-multicast"))) != 0)
    {
      multicast_group_address_str = currentArg;
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-msi"))) != 0)
    {
      MAX_SAMPLES_PER_INSTANCE = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-mxs"))) != 0)
    {
      MAX_SAMPLES = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if ((currentArg = arg_shifter.get_the_parameter(ACE_TEXT("-mxi"))) != 0)
    {
      MAX_INSTANCES = ACE_OS::atoi (currentArg);
      arg_shifter.consume_arg ();
    }
    else if (arg_shifter.cur_arg_strncasecmp(ACE_TEXT("-z")) == 0)
    {
      TURN_ON_VERBOSE_DEBUG;
      arg_shifter.consume_arg();
    }
    else
    {
      ACE_ERROR((LM_ERROR,"(%P|%t) unexpected parameter %s\n", arg_shifter.get_current()));
      arg_shifter.ignore_arg ();
      return 3;

    }
  }

  // Indicates sucessful parsing of the command line
  return 0;
}


int ACE_TMAIN (int argc, ACE_TCHAR *argv[])
{

  int status = 0;

  try
    {
      ACE_DEBUG((LM_INFO," %P|%t %T subscriber main\n"));

      ::DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);

      // let the Service_Participant (in above line) strip out -DCPSxxx parameters
      // and then get application specific parameters.
      status = parse_args (argc, argv);
      if (status)
        return status;


      ::DDS::DomainParticipant_var dp =
        dpf->create_participant(TEST_DOMAIN,
                                PARTICIPANT_QOS_DEFAULT,
                                ::DDS::DomainParticipantListener::_nil(),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (dp.in ()))
      {
        ACE_ERROR ((LM_ERROR,
                    ACE_TEXT(" %P|%t ERROR: create_participant failed.\n")));
        return 1 ;
      }

      ::Xyz::Pt128TypeSupport_var pt128ts;
      ::Xyz::Pt512TypeSupport_var pt512ts;
      ::Xyz::Pt2048TypeSupport_var pt2048ts;
      ::Xyz::Pt8192TypeSupport_var pt8192ts;

      // Register the type supports
      switch (DATA_SIZE)
      {
      case 128:
        {
          pt128ts = new ::Xyz::Pt128TypeSupportImpl();

          if (::DDS::RETCODE_OK != pt128ts->register_type(dp.in (), TEST_TYPE))
            {
              ACE_ERROR ((LM_ERROR,
                          ACE_TEXT (" %P|%t ERROR: Failed to register the Pt128TypeSupport.")));
              return 1;
            }
        }
        break;
      case 512:
        {
          pt512ts = new ::Xyz::Pt512TypeSupportImpl();

          if (::DDS::RETCODE_OK != pt512ts->register_type(dp.in (), TEST_TYPE))
            {
              ACE_ERROR ((LM_ERROR,
                          ACE_TEXT (" %P|%t ERROR:Failed to register the Pt512TypeSupport.")));
              return 1;
            }
        }
        break;
      case 2048:
        {
          pt2048ts = new ::Xyz::Pt2048TypeSupportImpl();

          if (::DDS::RETCODE_OK != pt2048ts->register_type(dp.in (), TEST_TYPE))
            {
              ACE_ERROR ((LM_ERROR,
                          ACE_TEXT (" %P|%t ERROR: Failed to register the Pt2048TypeSupport.")));
              return 1;
            }
        }
        break;
      case 8192:
        {
          pt8192ts = new ::Xyz::Pt8192TypeSupportImpl();

          if (::DDS::RETCODE_OK != pt8192ts->register_type(dp.in (), TEST_TYPE))
            {
              ACE_ERROR ((LM_ERROR,
                          ACE_TEXT (" %P|%t ERROR: Failed to register the Pt8192TypeSupport.")));
              return 1;
            }
        }
      };


      ::DDS::TopicQos topic_qos;
      dp->get_default_topic_qos(topic_qos);

      topic_qos.resource_limits.max_samples_per_instance =
            MAX_SAMPLES_PER_INSTANCE;
      topic_qos.resource_limits.max_instances = MAX_INSTANCES;
      topic_qos.resource_limits.max_samples = MAX_SAMPLES;

      topic_qos.history.kind = ::DDS::KEEP_ALL_HISTORY_QOS;

      ::DDS::Topic_var topic =
        dp->create_topic (TEST_TOPIC,
                          TEST_TYPE,
                          topic_qos,
                          ::DDS::TopicListener::_nil(),
                          ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (topic.in ()))
      {
        return 1 ;
      }

      ::DDS::TopicDescription_var description =
        dp->lookup_topicdescription(TEST_TOPIC);
      if (CORBA::is_nil (description.in() ))
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                           ACE_TEXT(" %P|%t ERROR: lookup_topicdescription failed.\n")),
                           1);
      }


      // Create the subscriber
      ::DDS::Subscriber_var sub =
        dp->create_subscriber(SUBSCRIBER_QOS_DEFAULT,
                             ::DDS::SubscriberListener::_nil(),
                             ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (sub.in() ))
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                           ACE_TEXT(" %P|%t ERROR: create_subscriber failed.\n")),
                           1);
      }

      // Initialize the transport
      if (0 != ::init_reader_tranport() )
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                           ACE_TEXT(" %P|%t ERROR: init_transport failed!\n")),
                           1);
      }

      // Attach the subscriber to the transport.
      OpenDDS::DCPS::SubscriberImpl* sub_impl =
        dynamic_cast<OpenDDS::DCPS::SubscriberImpl*> (sub.in());

      if (0 == sub_impl)
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                          ACE_TEXT(" %P|%t ERROR: Failed to obtain servant ::OpenDDS::DCPS::SubscriberImpl\n")),
                          1);
      }

      OpenDDS::DCPS::AttachStatus attach_status =
        sub_impl->attach_transport(reader_transport_impl.in());

      if (attach_status != OpenDDS::DCPS::ATTACH_OK)
        {
          // We failed to attach to the transport for some reason.
          const ACE_TCHAR* status_str = ACE_TEXT("");

          switch (attach_status)
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

          ACE_ERROR_RETURN ((LM_ERROR,
                            ACE_TEXT(" %P|%t ERROR: Failed to attach to the transport. ")
                            ACE_TEXT("AttachStatus == %s\n"),
                            status_str),
                            1);
        }


      // Create the Datareader
      ::DDS::DataReaderQos dr_qos;
      sub->get_default_datareader_qos (dr_qos);
      sub->copy_from_topic_qos (dr_qos, topic_qos);

      ::DDS::DataReaderListener_var dr_listener (
        new DataReaderListenerImpl(num_datawriters,
                                   NUM_SAMPLES,
                                   DATA_SIZE,
                                   RECVS_BTWN_READS));
      DataReaderListenerImpl* listener_servant =
        dynamic_cast<DataReaderListenerImpl*>(dr_listener.in());

      if (CORBA::is_nil (dr_listener.in()))
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                          ACE_TEXT(" %P|%t ERROR: get listener reference failed.\n")),
                          1);
      }

      ::DDS::DataReader_var  the_dr
               = sub->create_datareader(description.in() ,
                                        dr_qos,
                                        dr_listener.in(),
                                        ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);

      if (CORBA::is_nil (the_dr.in() ))
      {
        ACE_ERROR_RETURN ((LM_ERROR,
                          ACE_TEXT(" %P|%t ERROR: create_datareader failed.\n")),
                          1);
      }

      while (! listener_servant->is_finished ())
      {
        ACE_OS::sleep(2);
      }

      if (! listener_servant->verify_result ()) {
        ACE_ERROR_RETURN ((LM_ERROR,
                          ACE_TEXT(" %P|%t ERROR: verify result failed.\n")),
                          1);
      }

      //The collocated test for multicast needs more time than udp to
      //wait for receiving all end messages.
      ACE_OS::sleep(10);

      dp->delete_contained_entities();
      dpf->delete_participant(dp.in ());

      TheTransportFactory->release();
      TheServiceParticipant->shutdown ();


    }
  catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception ("Exception caught in main.cpp:");
      return 1;
    }

  // Note: The TransportImpl reference SHOULD be deleted before exit from
  //       main if the concrete transport libraries are loaded dynamically.
  //       Otherwise cleanup after main() will encount access vilation.
  reader_transport_impl = 0;

  return status;
}
