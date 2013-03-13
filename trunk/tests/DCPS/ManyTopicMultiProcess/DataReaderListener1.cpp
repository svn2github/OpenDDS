// -*- C++ -*-
//
// $Id$

#include "DataReaderListener1.h"
#include "common.h"
#include "tests/DCPS/common/SampleInfo.h"
#include "dds/DdsDcpsSubscriptionC.h"
#include "dds/DCPS/Service_Participant.h"
#include "Foo1DefTypeSupportC.h"
#include "Foo1DefTypeSupportImpl.h"

  void DataReaderListenerImpl1::read(::DDS::DataReader_ptr reader)
  {
    ::T1::Foo1DataReader_var foo_dr =
        ::T1::Foo1DataReader::_narrow(reader);

    if (CORBA::is_nil (foo_dr.in ()))
    {
      ACE_ERROR ((LM_ERROR,
              ACE_TEXT("(%P|%t) ::Mine::FooDataReader::_narrow failed.\n")));
    }

    ::T1::Foo1DataReaderImpl* dr_servant =
      dynamic_cast<T1::Foo1DataReaderImpl*> (foo_dr.in ());

    ::T1::Foo1Seq foo(num_ops_per_thread_);
    ::DDS::SampleInfoSeq si(num_ops_per_thread_);

    DDS::ReturnCode_t status;
    status = dr_servant->read(foo, si,
                              num_ops_per_thread_,
                              ::DDS::NOT_READ_SAMPLE_STATE,
                              ::DDS::ANY_VIEW_STATE,
                              ::DDS::ANY_INSTANCE_STATE);

    if (status == ::DDS::RETCODE_OK)
    {
      for (CORBA::ULong i = 0 ; i < si.length() ; i++)
      {
        if (si[i].valid_data)
        {
          ++num_samples_;

          ACE_OS::printf("foo1[%d]: c = %c, x = %f y = %f, key = %d\n",
                          i, foo[i].c, foo[i].x, foo[i].y, foo[i].key);
        }
        PrintSampleInfo(si[i]);
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

