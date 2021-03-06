// -*- C++ -*-
//
// $Id$

#include "Pusher.h"
#include "Factory.h"
#include "../common/TestSupport.h"
#include "../FooType4/FooDefTypeSupportImpl.h"


Pusher::Pusher(const Factory& f,
               const DDS::DomainParticipantFactory_var& factory,
               const DDS::DomainParticipant_var& participant,
               const DDS::DataWriterListener_var& listener) :
        dpf(factory),
        dp(participant),
        pub(f.publisher(dp)),
        topic(f.topic(dp)),
        writer_(f.writer(pub, topic, listener))
{
}

Pusher::Pusher(const Factory& f,
               const DDS::DomainParticipantFactory_var& factory,
               const DDS::DomainParticipant_var& participant,
               const DDS::Publisher_var& publisher,
               const DDS::DataWriterListener_var& listener) :
        dpf(factory),
        dp(participant),
        pub(publisher),
        topic(f.topic(dp)),
        writer_(f.writer(pub, topic, listener))
{
}


Pusher::~Pusher()
{
  // Clean up and shut down DDS objects
  pub->delete_contained_entities();
  dp->delete_contained_entities();
  dpf->delete_participant(dp.in());
}


const int default_key = 101010;

int
Pusher::push (const ACE_Time_Value& duration)
{
  ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("(%P|%t) Writer::run_test begins.\n")));


  ACE_Time_Value started = ACE_OS::gettimeofday ();
  unsigned int pass = 0;

  while(ACE_OS::gettimeofday() < started + duration)
  {
    ++pass;
    try
    {
      ::Xyz::Foo foo;
      //foo.key set below.
      foo.x = -1;
      foo.y = -1;

      foo.key = default_key;

      ::Xyz::FooDataWriter_var foo_dw
        = ::Xyz::FooDataWriter::_narrow(writer_.in ());
      TEST_ASSERT (! CORBA::is_nil (foo_dw.in ()));

      ACE_DEBUG((LM_DEBUG,
                ACE_TEXT("(%P|%t) %T Writer::run_test starting to write pass %d\n"),
                pass));

      ::DDS::InstanceHandle_t handle
          = foo_dw->register_instance(foo);

      foo.x = 5.0;
      foo.y = (float)(pass) ;

      foo_dw->write(foo,
                    handle);

      ACE_DEBUG((LM_DEBUG,
                ACE_TEXT("(%P|%t) %T Writer::run_test done writing.\n")));

      ACE_OS::sleep(1);
    }
    catch (const CORBA::Exception& ex)
    {
      ex._tao_print_exception ("Exception caught in run_test:");
    }
  }
  ACE_DEBUG((LM_DEBUG,
              ACE_TEXT("(%P|%t) Writer::run_test finished.\n")));
  return 0;
}
