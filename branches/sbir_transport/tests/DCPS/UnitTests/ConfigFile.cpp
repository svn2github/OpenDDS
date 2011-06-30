/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "ace/OS_main.h"
#include "ace/Configuration_Import_Export.h"
#include "tao/corba.h"

#include "dds/DCPS/transport/framework/TransportRegistry.h"
#include "dds/DCPS/transport/tcp/TcpInst.h"
#include "dds/DCPS/transport/tcp/TcpInst_rch.h"

#include "../common/TestSupport.h"

#include <iostream>

using namespace OpenDDS::DCPS;

int
ACE_TMAIN(int argc, ACE_TCHAR* argv[])
{
  CORBA::ORB_var orb = CORBA::ORB_init( argc, argv );

  ACE_TString config_fname(ACE_TEXT("test1.conf"));

  FILE* in = ACE_OS::fopen(config_fname.c_str(),
                           ACE_TEXT("r"));

  TEST_CHECK(in != 0);

  ACE_Configuration_Heap cf;
  int status = 0;
  ACE_OS::fclose(in);

  status = cf.open();
  TEST_CHECK(status == 0);

  ACE_Ini_ImpExp import(cf);
  status = import.import_config(config_fname.c_str());
  TEST_CHECK(status == 0);

  status = TransportRegistry::instance()->load_transport_configuration(cf);
  TEST_CHECK(status == 0);

  TransportInst_rch inst = TransportRegistry::instance()->get_inst("mytcp");
  TEST_CHECK(inst != 0);

  TcpInst_rch tcp_inst = dynamic_cast<TcpInst*>(inst.in());
  TEST_CHECK(tcp_inst != 0);

  tcp_inst->dump(std::cout);

  TEST_CHECK(tcp_inst->swap_bytes_ == true);
  TEST_CHECK(tcp_inst->queue_messages_per_pool_ == 9);
  TEST_CHECK(tcp_inst->queue_initial_pools_ == 2);
  TEST_CHECK(tcp_inst->max_packet_size_ == 2000000000);
  TEST_CHECK(tcp_inst->max_samples_per_packet_ == 9);
  TEST_CHECK(tcp_inst->optimum_packet_size_ == 2048);
  TEST_CHECK(tcp_inst->thread_per_connection_ == true);
  TEST_CHECK(tcp_inst->datalink_release_delay_ == 5000);
  TEST_CHECK(tcp_inst->datalink_control_chunks_ == 16);
  TEST_CHECK(tcp_inst->local_address_str_ == "localhost");
  TEST_CHECK(tcp_inst->enable_nagle_algorithm_ == true);
  TEST_CHECK(tcp_inst->conn_retry_initial_delay_ == 1000);
  TEST_CHECK(tcp_inst->conn_retry_backoff_multiplier_ == 4);
  TEST_CHECK(tcp_inst->conn_retry_attempts_ == 4);
  TEST_CHECK(tcp_inst->passive_reconnect_duration_ == 4000);
  TEST_CHECK(tcp_inst->passive_connect_duration_ == 20000);
  TEST_CHECK(tcp_inst->max_output_pause_period_ == 1000);

  return 0;
}
