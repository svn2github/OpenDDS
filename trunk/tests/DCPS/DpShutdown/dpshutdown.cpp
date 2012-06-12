// -*- C++ -*-
// ============================================================================
/**
 *  @file   dpshutdown.cpp
 *
 *  $Id$
 */
// ============================================================================

#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/PublisherImpl.h>
#include <dds/DCPS/Qos_Helper.h>
#include <dds/DCPS/transport/tcp/TcpInst.h>
#include <dds/DCPS/transport/framework/TransportType_rch.h>
#include <dds/DCPS/transport/rtps_udp/RtpsUdpInst_rch.h>
#include <dds/DCPS/transport/rtps_udp/RtpsUdpInst.h>
#include <dds/DCPS/transport/framework/TransportRegistry.h>
#include <dds/DCPS/transport/framework/TransportConfig_rch.h>
#ifndef DDS_HAS_MINIMUM_BIT
#include <dds/DCPS/RTPS/RtpsDiscovery.h>
#endif

#include "dds/DCPS/StaticIncludes.h"
#include "MessengerTypeSupportImpl.h"

#include <ace/streams.h>
#include "ace/Get_Opt.h"

#include <memory>
#include <assert.h>

int ACE_TMAIN (int argc, ACE_TCHAR *argv[]){
  try
    {
      DDS::DomainParticipantFactory_var dpf =
        TheParticipantFactoryWithArgs(argc, argv);

      OpenDDS::DCPS::TransportConfig_rch config =
        OpenDDS::DCPS::TransportRegistry::instance()->get_config("dds4ccm_rtps");

      if (config.is_nil())
        {
          config =
            OpenDDS::DCPS::TransportRegistry::instance()->create_config("dds4ccm_rtps");
        }

      OpenDDS::DCPS::TransportInst_rch inst =
        OpenDDS::DCPS::TransportRegistry::instance()->get_inst("the_rtps_transport");

      if (inst.is_nil())
        {
          inst =
            OpenDDS::DCPS::TransportRegistry::instance()->create_inst("the_rtps_transport",
                                                                "rtps_udp");
          OpenDDS::DCPS::RtpsUdpInst_rch rui =
            OpenDDS::DCPS::static_rchandle_cast<OpenDDS::DCPS::RtpsUdpInst>(inst);

          config->instances_.push_back(inst);

          OpenDDS::DCPS::TransportRegistry::instance()->global_config(config);
        }

#ifndef DDS_HAS_MINIMUM_BIT
      OpenDDS::RTPS::RtpsDiscovery_rch disc =
        new OpenDDS::RTPS::RtpsDiscovery(OpenDDS::DCPS::Discovery::DEFAULT_RTPS);

      TheServiceParticipant->add_discovery(OpenDDS::DCPS::static_rchandle_cast<OpenDDS::DCPS::Discovery>(disc));
      TheServiceParticipant->set_repo_domain(11, disc->key());
#endif
      TheServiceParticipant->set_default_discovery (OpenDDS::DCPS::Discovery::DEFAULT_RTPS);

      DDS::DomainParticipant_var participant =
        dpf->create_participant(11,
                                PARTICIPANT_QOS_DEFAULT,
                                DDS::DomainParticipantListener::_nil(),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (participant.in ())) {
        cerr << "create_participant failed." << endl;
        return 1;
      }
      else
      {
        ACE_DEBUG ((LM_DEBUG, "Created participant 1 with instance handle %d\n",
                    participant->get_instance_handle ()));
      }

      DDS::DomainParticipant_var participant2 =
        dpf->create_participant(11,
                                PARTICIPANT_QOS_DEFAULT,
                                DDS::DomainParticipantListener::_nil(),
                                ::OpenDDS::DCPS::DEFAULT_STATUS_MASK);
      if (CORBA::is_nil (participant2.in ())) {
        cerr << "create_participant failed." << endl;
        return 1;
      }
      else
      {
        ACE_DEBUG ((LM_DEBUG, "Created participant 2 with instance handle %d\n",
                    participant2->get_instance_handle ()));
      }

      // Register TypeSupport (Messenger::Message)
      Messenger::MessageTypeSupport_var mts =
        new Messenger::MessageTypeSupportImpl();

      if (mts->register_type(participant.in(), "") != DDS::RETCODE_OK) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: register_type failed!\n")),
                        -1);
      }

      // Create Topic
      CORBA::String_var type_name = mts->get_type_name();
      DDS::Topic_var topic =
        participant->create_topic("Movie Discussion List",
                                  type_name.in(),
                                  TOPIC_QOS_DEFAULT,
                                  DDS::TopicListener::_nil(),
                                  OpenDDS::DCPS::DEFAULT_STATUS_MASK);

      if (CORBA::is_nil(topic.in())) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: create_topic failed!\n")),
                        -1);
      }

      // Create Publisher
      DDS::Publisher_var pub =
        participant->create_publisher(PUBLISHER_QOS_DEFAULT,
                                      DDS::PublisherListener::_nil(),
                                      OpenDDS::DCPS::DEFAULT_STATUS_MASK);

      if (CORBA::is_nil(pub.in())) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: create_publisher failed!\n")),
                        -1);
      }

      // Create DataWriter
      DDS::DataWriter_var dw =
        pub->create_datawriter(topic.in(),
                              DATAWRITER_QOS_DEFAULT,
                              DDS::DataWriterListener::_nil(),
                              OpenDDS::DCPS::DEFAULT_STATUS_MASK);

      if (CORBA::is_nil(dw.in())) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: create_datawriter failed!\n")),
                        -1);
      }

      DDS::ReturnCode_t retcode2 = pub->delete_datawriter (dw.in ());
      if (retcode2 != DDS::RETCODE_OK) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: should be able to delete datawriter\n")),
                        -1);
      }

      DDS::ReturnCode_t retcode5 = dpf->delete_participant(participant.in ());
      if (retcode5 != DDS::RETCODE_PRECONDITION_NOT_MET) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: should not be able to delete participant\n")),
                        -1);
      }

      DDS::ReturnCode_t retcode3 = participant->delete_publisher (pub.in ());
      if (retcode3 != DDS::RETCODE_OK) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: should be able to delete publisher\n")),
                        -1);
      }

      DDS::ReturnCode_t retcode4 = participant->delete_topic (topic.in ());
      if (retcode4 != DDS::RETCODE_OK) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: should be able to delete topic\n")),
                        -1);
      }

      DDS::ReturnCode_t retcode6 = dpf->delete_participant(participant.in ());
      if (retcode6 != DDS::RETCODE_OK) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l: main()")
                          ACE_TEXT(" ERROR: should be able to delete participant\n")),
                        -1);
      }

//       DDS::ReturnCode_t retcode7 = dpf->delete_participant(participant2.in ());
//       if (retcode7 != DDS::RETCODE_OK) {
//         ACE_ERROR_RETURN((LM_ERROR,
//                           ACE_TEXT("%N:%l: main()")
//                           ACE_TEXT(" ERROR: should be able to delete participant2\n")),
//                         -1);
//       }

      ACE_DEBUG ((LM_DEBUG, "Shuting down tsp with one participant still registered\n"));
      TheServiceParticipant->shutdown ();
  }
  catch (CORBA::Exception& e)
  {
    cerr << "dp: Exception caught in main.cpp:" << endl
         << e << endl;
    exit(1);
  }

  return 0;
}
