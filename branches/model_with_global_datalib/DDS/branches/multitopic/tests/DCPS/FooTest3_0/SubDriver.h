#ifndef SUBDRIVER_H
#define SUBDRIVER_H

#include "SimpleSubscriber.h"
#include "dds/DCPS/Definitions.h"
#include "ace/INET_Addr.h"
#include "ace/String_Base.h"
#include "TestC.h"
#include <string>

#include <vector>

class SubDriver
{
  public:
  
    typedef std::vector < ::OpenDDS::DCPS::PublicationId > PublicationIds;

    SubDriver();
    virtual ~SubDriver();

    void run(int& argc, ACE_TCHAR* argv[]);


  private:

    enum TransportTypeId
    {
      SIMPLE_TCP
    };

    enum TransportInstanceId
    {
      ALL_TRAFFIC
    };

    void parse_args(int& argc, ACE_TCHAR* argv[]);
    void init(int& argc, ACE_TCHAR* argv[]);
    void run();

    int parse_pub_arg(const ACE_TString& arg);
    int parse_sub_arg(const ACE_TString& arg);

    CORBA::ORB_var orb_;

    SimpleSubscriber  subscriber_;

    ACE_TString       pub_id_fname_;
    ACE_INET_Addr     pub_addr_;
    ACE_TString       pub_addr_str_;

    OpenDDS::DCPS::RepoId sub_id_;
    ACE_TString       sub_addr_;

    int               num_writes_;

    ::Test::TestPubDriver_var pub_driver_;
    ACE_CString       pub_driver_ior_;
    int               shutdown_pub_;
    int               add_new_subscription_;
    int               shutdown_delay_secs_;

    ACE_TString       sub_ready_filename_;
};

#endif
