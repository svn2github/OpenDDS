
#include "ace/OS_main.h"
#include "../common/TestSupport.h"
#include "dds/DdsDcpsDomainC.h"
#include "dds/DdsDcpsInfrastructureTypeSupportImpl.h"
#include "dds/DCPS/Service_Participant.h"
#include "dds/DCPS/Marked_Default_Qos.h"
#include "dds/DCPS/BuiltInTopicUtils.h"
#include "dds/DCPS/WaitSet.h"


#include "ace/Log_Msg.h"

using namespace DDS;
using namespace OpenDDS::DCPS;

namespace {
  long key_val = 0;
  namespace Factory {
    ParticipantBuiltinTopicData bit_data() {
      ParticipantBuiltinTopicData result;
      result.key.value[0] = ++key_val;
      result.key.value[1] = 0;
      result.key.value[2] = 0;
      return result;
    }
  }
};

int
ACE_TMAIN(int argc, ACE_TCHAR* argv[])
{
  // Sends messages
  DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);
  DomainParticipant_var dp = dpf->create_participant(9, PARTICIPANT_QOS_DEFAULT,
                                                     0, DEFAULT_STATUS_MASK);
  Subscriber_var bit_sub = dp->get_builtin_subscriber();
  DataReader_var dr = bit_sub->lookup_datareader(BUILT_IN_PARTICIPANT_TOPIC);
  ParticipantBuiltinTopicDataDataReaderImpl* bit_dr =
      dynamic_cast<ParticipantBuiltinTopicDataDataReaderImpl*>(dr.in());
  // Wait for BIT data
  ReadCondition_var rc = dr->create_readcondition(ANY_SAMPLE_STATE, 
                                                  ANY_VIEW_STATE, 
                                                  ALIVE_INSTANCE_STATE);
  //sleep(60);
  WaitSet waiter;
  waiter.attach_condition(rc);
  DDS::ConditionSeq activeConditions;
  DDS::Duration_t forever = { DDS::DURATION_INFINITE_SEC,
                              DDS::DURATION_INFINITE_NSEC };

  ReturnCode_t result = waiter.wait(activeConditions, forever);
  TEST_ASSERT(result == RETCODE_OK);

  { // Should be able to read synthetic data
    ParticipantBuiltinTopicData part_data_in = Factory::bit_data();
    bit_dr->store_synthetic_data(part_data_in, NEW_VIEW_STATE);
    ParticipantBuiltinTopicDataSeq part_data_out;
    SampleInfoSeq si;
    result = bit_dr->read(part_data_out, si, LENGTH_UNLIMITED,
                          ANY_SAMPLE_STATE, ANY_VIEW_STATE, 
                          ALIVE_INSTANCE_STATE);
    TEST_ASSERT(result == RETCODE_OK);

    TEST_ASSERT(part_data_out.length() >= 1);
    // Of all the data, one should match our data
    bool matched = false;
    for (CORBA::ULong i = 0; i < part_data_out.length(); ++i) {
      if (part_data_out[i].key.value[0] == part_data_in.key.value[0]) {
        matched = true;
      }
    }
    TEST_ASSERT(matched);
  }
  { // ALIVE_INSTANCE_STATE should not match disposed synthetic data
    ParticipantBuiltinTopicData part_data_in = Factory::bit_data();
    bit_dr->store_synthetic_data(part_data_in, NEW_VIEW_STATE);
    ParticipantBuiltinTopicDataSeq part_data_out;
    SampleInfoSeq si;
    result = bit_dr->read(part_data_out, si, LENGTH_UNLIMITED,
                          ANY_SAMPLE_STATE, ANY_VIEW_STATE, 
                          ALIVE_INSTANCE_STATE);
    TEST_ASSERT(result == RETCODE_OK);
    TEST_ASSERT(part_data_out.length() >= 1);
    // Of all the data, one should match our data
    bool matched = false;
    for (CORBA::ULong i = 0; i < part_data_out.length(); ++i) {
      if (part_data_out[i].key.value[0] == part_data_in.key.value[0]) {
        matched = true;
        InstanceHandle_t handle = si[i].instance_handle;
        bit_dr->set_instance_state(handle, NOT_ALIVE_DISPOSED_INSTANCE_STATE);
      }
    }
    TEST_ASSERT(matched);
    // Read again should no longer retrieve disposed data
    matched = false;
    ParticipantBuiltinTopicDataSeq part_data_out2;
    SampleInfoSeq si2;
    result = bit_dr->read(part_data_out2, si2, LENGTH_UNLIMITED,
                          ANY_SAMPLE_STATE, ANY_VIEW_STATE, 
                          ALIVE_INSTANCE_STATE);
    TEST_ASSERT(result == RETCODE_OK);
    TEST_ASSERT(part_data_out2.length() == part_data_out.length() - 1);
    for (CORBA::ULong i = 0; i < part_data_out2.length(); ++i) {
      if (part_data_out2[i].key.value[0] == part_data_in.key.value[0]) {
        matched = true;
      }
    }
    TEST_ASSERT(!matched);
  }
  { // disposed synthetic data should not be valid data
    ParticipantBuiltinTopicData part_data_in = Factory::bit_data();
    bit_dr->store_synthetic_data(part_data_in, NEW_VIEW_STATE);
    ParticipantBuiltinTopicDataSeq part_data_out;
    SampleInfoSeq si;
    result = bit_dr->read(part_data_out, si, LENGTH_UNLIMITED,
                          ANY_SAMPLE_STATE, ANY_VIEW_STATE, 
                          ALIVE_INSTANCE_STATE);
    TEST_ASSERT(result == RETCODE_OK);
    TEST_ASSERT(part_data_out.length() >= 1);
    // Of all the data, one should match our data
    bool matched = false;
    for (CORBA::ULong i = 0; i < part_data_out.length(); ++i) {
      if (part_data_out[i].key.value[0] == part_data_in.key.value[0]) {
        matched = true;
        InstanceHandle_t handle = si[i].instance_handle;
        bit_dr->set_instance_state(handle, NOT_ALIVE_DISPOSED_INSTANCE_STATE);
      }
    }
    TEST_ASSERT(matched);
    // Read again should no longer retrieve disposed data
    CORBA::ULong invalid_matched = 0;
    ParticipantBuiltinTopicDataSeq part_data_out2;
    SampleInfoSeq si2;
    result = bit_dr->read(part_data_out2, si2, LENGTH_UNLIMITED,
                          ANY_SAMPLE_STATE, ANY_VIEW_STATE, 
                          ANY_INSTANCE_STATE);
    TEST_ASSERT(result == RETCODE_OK);
    TEST_ASSERT(part_data_out2.length() > part_data_out.length());
    for (CORBA::ULong i = 0; i < part_data_out2.length(); ++i) {
      if (!si2[i].valid_data) {
        ++invalid_matched;
      }
    }
    TEST_ASSERT(invalid_matched > 0);
  }
  { // Should be able to read disposed and restored synthetic data
    ParticipantBuiltinTopicData part_data_in = Factory::bit_data();
    bit_dr->store_synthetic_data(part_data_in, NEW_VIEW_STATE);
    ParticipantBuiltinTopicDataSeq part_data_out;
    SampleInfoSeq si;
    InstanceHandle_t handle = 0;
    result = bit_dr->read(part_data_out, si, LENGTH_UNLIMITED,
                          ANY_SAMPLE_STATE, ANY_VIEW_STATE, 
                          ALIVE_INSTANCE_STATE);
    TEST_ASSERT(result == RETCODE_OK);
    TEST_ASSERT(part_data_out.length() >= 1);
    // Of all the data, one should match our data
    bool matched = false;
    for (CORBA::ULong i = 0; i < part_data_out.length(); ++i) {
      if (part_data_out[i].key.value[0] == part_data_in.key.value[0]) {
        matched = true;
        handle = si[i].instance_handle;
        // Dispose of the data
        bit_dr->set_instance_state(handle, NOT_ALIVE_DISPOSED_INSTANCE_STATE);
      }
    }
    TEST_ASSERT(matched);
    // Restore the disposed data
    bit_dr->store_synthetic_data(part_data_in, NOT_NEW_VIEW_STATE);
    // Read again should still retrieve disposed/restored data
    matched = false;
    ParticipantBuiltinTopicDataSeq part_data_out2;
    SampleInfoSeq si2;
    result = bit_dr->read(part_data_out2, si2, LENGTH_UNLIMITED,
                          ANY_SAMPLE_STATE, ANY_VIEW_STATE, 
                          ALIVE_INSTANCE_STATE);
    TEST_ASSERT(result == RETCODE_OK);
    for (CORBA::ULong i = 0; i < part_data_out2.length(); ++i) {
      if (si2[i].valid_data) {
        if (part_data_out2[i].key.value[0] == part_data_in.key.value[0]) {
          matched = true;
        }
      }
    }
    TEST_ASSERT(matched);
  }
  sleep(5);
  TheServiceParticipant->shutdown();
}
