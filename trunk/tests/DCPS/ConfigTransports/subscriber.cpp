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


#include "common.h"
#include "Options.h"
#include "Factory.h"

#include "Puller.h"
#include "DataReaderListener.h"

#include "dds/DCPS/Service_Participant.h"

#include "dds/DCPS/StaticIncludes.h"
#ifdef ACE_AS_STATIC_LIBS
#include <dds/DCPS/RTPS/RtpsDiscovery.h>
#include <dds/DCPS/transport/rtps_udp/RtpsUdp.h>
#endif

#include "../common/TestSupport.h"
#include "../FooType4/FooDefTypeSupportImpl.h"

int
ACE_TMAIN(int argc, ACE_TCHAR *argv[])
{
  try
    {
      DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);

      OpenDDS::DCPS::TypeSupport_var typsup = new Xyz::FooTypeSupportImpl;

      Options configopt(argc, argv);
      Factory fconfig(configopt, typsup);

      Options plainopt;
      Factory fplain(plainopt, typsup);

      DDS::DataReaderListener_var drl1(new DataReaderListenerImpl(configopt));
      DDS::DataReaderListener_var drl2(new DataReaderListenerImpl(plainopt));

      if (configopt.collocation_str == "none")
        {
          DDS::DomainParticipant_var participant(fconfig.participant(dpf));
          Puller r(fconfig, dpf, participant, drl1);
          TEST_ASSERT(assert_supported(configopt, r.reader_.in()));
          r.pull(ACE_Time_Value(1));

        }
      else if (configopt.collocation_str == "process")
        {
          DDS::DomainParticipant_var participant1(fconfig.participant(dpf));
          Puller r1(fconfig, dpf, participant1, drl1);

          DDS::DomainParticipant_var participant2(fplain.participant(dpf));
          Puller r2(fplain, dpf, participant2, drl2);

          TEST_ASSERT (participant1.in() != participant2.in());

          TEST_ASSERT(assert_supported(configopt, r1.reader_));
          if (configopt.entity_str == "none")
            {
              TEST_ASSERT(assert_supported(configopt, r1.reader_));
            }
          else
            {
              TEST_ASSERT(!assert_supported(configopt, r2.reader_));
            }

          r1.pull(ACE_Time_Value(1));
        }
      else if (configopt.collocation_str == "participant")
        {
          DDS::DomainParticipant_var participant1(fconfig.participant(dpf));
          DDS::DomainParticipant_var participant2(participant1);

          Puller r1(fconfig, dpf, participant1, drl1);

          Puller r2(fplain, dpf, participant2, drl2);

          TEST_ASSERT(assert_supported(configopt, r1.reader_));
          TEST_ASSERT(assert_supported(configopt, r2.reader_));

          r1.pull(ACE_Time_Value(1));
        }

      else if (configopt.collocation_str == "pubsub")
        {
          DDS::DomainParticipant_var participant1(fconfig.participant(dpf));
          DDS::DomainParticipant_var participant2(participant1);

          DDS::Subscriber_var subscriber1(fconfig.subscriber(participant1));
          Puller r1(fconfig, dpf, participant1, subscriber1, drl1);

          DDS::Subscriber_var subscriber2(fplain.subscriber(participant2));
          Puller r2(fplain, dpf, participant2, subscriber2, drl1);

          TEST_ASSERT(assert_supported(configopt, r1.reader_));
          TEST_ASSERT(assert_supported(configopt, r2.reader_));

          r1.pull(ACE_Time_Value(1));
        }

      ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) Shutting subscriber down ...\n")));
      TheServiceParticipant->shutdown();
      ACE_DEBUG((LM_INFO, ACE_TEXT("(%P|%t) Subscriber shutdown complete\n")));

      if (configopt.collocation_str == "none")
        {
          TEST_ASSERT(assert_subscription_matched(configopt, drl1));
        }
      else if (configopt.collocation_str == "process")
        {
          TEST_ASSERT(assert_subscription_matched(configopt, drl1)
                      && assert_subscription_matched(configopt, drl2));
        }
      else if (configopt.collocation_str == "participant")
        {
          TEST_ASSERT(assert_subscription_matched(configopt, drl1)
                      && assert_subscription_matched(configopt, drl2));
        }
      else if (configopt.collocation_str == "pubsub")
        {
          TEST_ASSERT(assert_subscription_matched(configopt, drl1)
                      && assert_subscription_matched(configopt, drl2));
        }
    }
  catch (char const *ex)
    {
      ACE_ERROR_RETURN((LM_ERROR,
                    ACE_TEXT("(%P|%t) Assertion failed.\n"), ex), -1);
    }
  catch (const CORBA::Exception &ex)
    {
      ex._tao_print_exception("Exception caught in main.cpp:");
      return 1;
    }

  ACE_ERROR_RETURN((LM_INFO,
                    ACE_TEXT("(%P|%t) done.\n")), 0);
}
