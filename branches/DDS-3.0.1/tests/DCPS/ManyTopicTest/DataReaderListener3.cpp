// -*- C++ -*-
//
// $Id$

#include "DataReaderListener3.h"
#include "common.h"
#include "../common/SampleInfo.h"
#include "dds/DdsDcpsSubscriptionC.h"
#include "dds/DCPS/Service_Participant.h"
#include "tests/DCPS/ManyTopicTypes/Foo3DefTypeSupportC.h"
#include "tests/DCPS/ManyTopicTypes/Foo3DefTypeSupportImpl.h"

  void DataReaderListenerImpl3::read(::DDS::DataReader_ptr reader)
  {
    ACE_UNUSED_ARG(max_samples_per_instance);
    ACE_UNUSED_ARG(history_depth);

    ::T3::Foo3DataReader_var foo_dr =
        ::T3::Foo3DataReader::_narrow(reader);

    if (CORBA::is_nil (foo_dr.in ()))
      {
        ACE_ERROR ((LM_ERROR,
               ACE_TEXT("(%P|%t) ::Mine::FooDataReader::_narrow failed.\n")));
      }

    ::T3::Foo3DataReaderImpl* dr_servant =
      dynamic_cast< ::T3::Foo3DataReaderImpl*>(foo_dr.in());

    ::T3::Foo3Seq foo(num_ops_per_thread) ;
    ::DDS::SampleInfoSeq si(num_ops_per_thread) ;

    DDS::ReturnCode_t status  ;
    status = dr_servant->read(foo, si,
                              num_ops_per_thread,
                              ::DDS::NOT_READ_SAMPLE_STATE,
                              ::DDS::ANY_VIEW_STATE,
                              ::DDS::ANY_INSTANCE_STATE);

    if (status == ::DDS::RETCODE_OK)
      {
        for (CORBA::ULong i = 0 ; i < si.length() ; i++)
        {
          num_samples_++ ;

          ACE_OS::printf (
              "foo3[%d]: c = %c,  s = %d, l = %d, text = %s, key = %d\n",
              i, foo[i].c, foo[i].s, foo[i].l, foo[i].text.in(), foo[i].key);
          PrintSampleInfo(si[i]) ;
        }
      }
      else if (status == ::DDS::RETCODE_NO_DATA)
      {
        ACE_OS::printf ("read returned ::DDS::RETCODE_NO_DATA\n") ;
      }
      else
      {
        ACE_OS::printf ("read - Error: %d\n", status) ;
      }
  }

