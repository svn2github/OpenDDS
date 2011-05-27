
#include "TestConfig.h"
#include "TestException.h"
#include "ace/Arg_Shifter.h"
#include "ace/OS_NS_stdlib.h"
// #include "ace/Log_Msg.h"

namespace { // anonymous namespace for file scope.
  //
  // Default values.
  //
  const char* DEFAULT_TYPENAME   = "Foo";
  const char* DEFAULT_TOPICNAME  = "Foo";
  const bool  TC_DEFAULT_VERBOSE = false;
  enum {      TC_DEFAULT_SAMPLES = 10};
  enum {      DEFAULT_DOMAIN     = 311 };

} // end of anonymous namespace.

TestConfig::~TestConfig()
{
}

TestConfig::TestConfig( int argc, char** argv, char** /* envp */)
 : verbose_(   TC_DEFAULT_VERBOSE),
   samples_(   TC_DEFAULT_SAMPLES),
   domain_(    DEFAULT_DOMAIN),
   typeName_(  DEFAULT_TYPENAME),
   topicName_( DEFAULT_TOPICNAME)
{
  ACE_Arg_Shifter parser( argc, argv);
  while( parser.is_anything_left()) {
    const char* currentArg = 0;
    if( 0 != (currentArg = parser.get_the_parameter("-Samples"))) {
      this->samples_ = ACE_OS::atoi( currentArg);
      parser.consume_arg();

    } else if( 0 != (currentArg = parser.get_the_parameter("-v"))) {
      this->verbose_ = true;

    } else {
      parser.ignore_arg();
    }
  }
}

bool&
TestConfig::verbose()
{
  return this->verbose_;
}

bool
TestConfig::verbose() const
{
  return this->verbose_;
}

int&
TestConfig::samples()
{
  return this->samples_;
}

int
TestConfig::samples() const
{
  return this->samples_;
}

int&
TestConfig::domain()
{
  return this->domain_;
}

int
TestConfig::domain() const
{
  return this->domain_;
}

std::string&
TestConfig::typeName()
{
  return this->typeName_;
}

std::string
TestConfig::typeName() const
{
  return this->typeName_;
}

std::string&
TestConfig::topicName()
{
  return this->topicName_;
}

std::string
TestConfig::topicName() const
{
  return this->topicName_;
}

