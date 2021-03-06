/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include <ace/Argv_Type_Converter.h>
#include <ace/Get_Opt.h>
#include <ace/Log_Msg.h>
#include <ace/OS_NS_stdlib.h>

#include <dds/DdsDcpsInfrastructureC.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/SubscriberImpl.h>
#include <dds/DCPS/WaitSet.h>

#include "dds/DCPS/transport/framework/TransportRegistry.h"
#include "dds/DCPS/transport/framework/TransportInst_rch.h"
#include "dds/DCPS/transport/udp/UdpInst.h"
#include "dds/DCPS/transport/udp/UdpInst_rch.h"

#include "dds/DCPS/StaticIncludes.h"
#ifdef ACE_AS_STATIC_LIBS
#include <dds/DCPS/transport/udp/Udp.h>
#include <dds/DCPS/transport/multicast/Multicast.h>
#include <dds/DCPS/transport/shmem/Shmem.h>
#include <dds/DCPS/transport/rtps_udp/RtpsUdp.h>
#endif

#include "DataReaderListener.h"
#include "MessengerTypeSupportImpl.h"
#include <cstdlib>

namespace {

const long num_messages_expected = 40;

void parse_args(int argc, ACE_TCHAR* argv[], bool& reliable)
{
  ACE_Get_Opt getopt(argc, argv, "r:");
  for (int opt = 0; (opt = getopt()) != EOF;) {
    if (opt == 'r') {
      reliable = ACE_OS::atoi(getopt.opt_arg());
    }
  }
}

} // namespace

int
ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
  bool ok = true;
  try {
    // Initialize DomainParticipantFactory
    DDS::DomainParticipantFactory_var dpf =
      TheParticipantFactoryWithArgs(argc, argv);

    // handle test performance issue on one platform
#if defined (sun)
    const char* udpTransName = "udp";
    OpenDDS::DCPS::TransportInst_rch inst = OpenDDS::DCPS::TransportRegistry::instance()->get_inst(udpTransName);
    if (inst != 0) {
      OpenDDS::DCPS::UdpInst_rch udp_inst = OpenDDS::DCPS::dynamic_rchandle_cast<OpenDDS::DCPS::UdpInst>(inst);
      if (udp_inst == 0) {
        ACE_ERROR_RETURN((LM_ERROR,
                          ACE_TEXT("%N:%l main()")
                          ACE_TEXT(" ERROR: retrieving transport config for: %C failed!\n"),
                          udpTransName), -1);
      }
      udp_inst->rcv_buffer_size_ = 0x40000;
    }
#endif

    // Create DomainParticipant
    DDS::DomainParticipant_var participant =
      dpf->create_participant(411,
                              PARTICIPANT_QOS_DEFAULT,
                              DDS::DomainParticipantListener::_nil(),
                              OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (CORBA::is_nil(participant.in())) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l main()")
                        ACE_TEXT(" ERROR: create_participant() failed!\n")), -1);
    }

    // Register Type (Messenger::Message)
    Messenger::MessageTypeSupport_var ts =
      new Messenger::MessageTypeSupportImpl();

    if (ts->register_type(participant.in(), "") != DDS::RETCODE_OK) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l main()")
                        ACE_TEXT(" ERROR: register_type() failed!\n")), -1);
    }

    // Create Topic (Movie Discussion List)
    CORBA::String_var type_name = ts->get_type_name();
    DDS::Topic_var topic =
      participant->create_topic("Movie Discussion List",
                                type_name.in(),
                                TOPIC_QOS_DEFAULT,
                                DDS::TopicListener::_nil(),
                                OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (CORBA::is_nil(topic.in())) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l main()")
                        ACE_TEXT(" ERROR: create_topic() failed!\n")), -1);
    }

    // Create Subscriber
    DDS::Subscriber_var sub =
      participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT,
                                     DDS::SubscriberListener::_nil(),
                                     OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (CORBA::is_nil(sub.in())) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l main()")
                        ACE_TEXT(" ERROR: create_subscriber() failed!\n")), -1);
    }

    // Create DataReader
    DataReaderListenerImpl* listener_svt = new DataReaderListenerImpl;
    DDS::DataReaderListener_var listener(listener_svt);

    DDS::DataReaderQos qos;
    sub->get_default_datareader_qos(qos);
    qos.liveliness.kind = DDS::AUTOMATIC_LIVELINESS_QOS;
    qos.liveliness.lease_duration.sec = 10;
    qos.liveliness.lease_duration.nanosec = 0;

    bool reliable = true;
    parse_args(argc, argv, reliable);
    if (reliable) {
      qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;
    }

    DDS::DataReader_var reader =
      sub->create_datareader(topic.in(),
                             qos,
                             listener.in(),
                             OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (CORBA::is_nil(reader.in())) {
      ACE_ERROR_RETURN((LM_ERROR,
                        ACE_TEXT("%N:%l main()")
                        ACE_TEXT(" ERROR: create_datareader() failed!\n")), -1);
    }

    for (int delay = 0; listener_svt->num_samples() != num_messages_expected
         && delay < 60; ++delay) {
      ACE_OS::sleep(1);
    }

    const long received = listener_svt->num_samples();
    if (reliable && received < num_messages_expected) {
      std::cout << "ERROR: data loss (" << received << "/"
                << num_messages_expected << " received)\n";
      ok = false;
    }
    else {
      const unsigned int percent = ((num_messages_expected - received) * 100) / num_messages_expected;
      std::cout << "data loss == " << percent << "% (" << received << "/"
                << num_messages_expected << " received)\n";
    }

    // Clean-up!
    participant->delete_contained_entities();
    dpf->delete_participant(participant);

    TheServiceParticipant->shutdown();
    ACE_Thread_Manager::instance()->wait();

  } catch (const CORBA::Exception& e) {
    e._tao_print_exception("Exception caught in main():");
    return -1;
  }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
