#include "SubDriver.h"
#include "TestException.h"
#include "ace/Log_Msg.h"


int
main(int argc, char* argv[])
{
  // Need call the ORB_init to dynamically load the transport libs.
  CORBA::ORB_var orb = CORBA::ORB_init (argc,
                                        argv,
                                        "DDS_DCPS");

  ACE_LOG_MSG->priority_mask(LM_TRACE     |
                             LM_DEBUG     |
                             LM_INFO      |
                             LM_NOTICE    |
                             LM_WARNING   |
                             LM_ERROR     |
                             LM_CRITICAL  |
                             LM_ALERT     |
                             LM_EMERGENCY,
                             ACE_Log_Msg::PROCESS);
  SubDriver driver;

  try
  {
    driver.run(argc, argv);
    return 0;
  }
  catch (const TestException&)
  {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) SubDriver TestException.\n"));
  }
  catch (...)
  {
    ACE_ERROR((LM_ERROR,
               "(%P|%t) SubDriver unknown (...) exception.\n"));
  }

  return 1;
}
