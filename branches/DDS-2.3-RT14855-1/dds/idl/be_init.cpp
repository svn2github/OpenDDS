/*
 * $Id$
 *
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "../Version.h"

#include "tao/Version.h"
#include "global_extern.h"
#include "be_extern.h"
#include "drv_extern.h"

#include "ace/OS_NS_stdlib.h"

void
BE_version()
{
  ACE_DEBUG((LM_DEBUG, ACE_TEXT("OpenDDS version ") ACE_TEXT(DDS_VERSION)
             ACE_TEXT("\n")));
}

int
BE_init(int&, ACE_TCHAR*[])
{
  ACE_NEW_RETURN(be_global, BE_GlobalData, -1);
  return 0;
}

void
BE_post_init(char*[], long)
{
  const char* env = ACE_OS::getenv("DDS_ROOT");
  if (env && env[0]) {
    std::string dds_root = env;
    if (dds_root.find(' ') != std::string::npos && dds_root[0] != '"') {
      dds_root.insert(dds_root.begin(), '"');
      dds_root.insert(dds_root.end(), '"');
    }
    be_global->add_inc_path(dds_root.c_str());
    ACE_CString included;
    DRV_add_include_path(included, dds_root.c_str(), 0
#if TAO_MAJOR_VERSION > 1 || (TAO_MAJOR_VERSION == 1 && TAO_MINOR_VERSION > 6)
, true);
#else
);
#endif
  }
}
