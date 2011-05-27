#ifndef PUBDRIVER_H
#define PUBDRIVER_H

#include "SimplePublisher.h"
#include "dds/DCPS/Definitions.h"
#include "ace/INET_Addr.h"
#include "ace/String_Base.h"


class PubDriver
{
  public:

    PubDriver();
    virtual ~PubDriver();

    void run(int& argc, ACE_TCHAR* argv[]);


  private:

    enum TransportTypeId
    {
      SIMPLE_MCAST
    };

    enum TransportInstanceId
    {
      ALL_TRAFFIC
    };

    void parse_args(int& argc, ACE_TCHAR* argv[]);
    void init();
    void run();

    int parse_pub_arg(const ACE_TString& arg);
    int parse_sub_arg(const ACE_TString& arg);


    SimplePublisher publisher_;

    OpenDDS::DCPS::RepoId pub_id_;
    ACE_INET_Addr     pub_addr_;
    ACE_TString       pub_addr_str_;

    OpenDDS::DCPS::RepoId sub_id_;
    ACE_INET_Addr     if_addr_;

    unsigned num_msgs_;
};

#endif
