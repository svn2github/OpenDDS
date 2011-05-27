// -*- C++ -*-
//
// $Id$
#include "DCPS/DdsDcps_pch.h" //Only the _pch include should start with DCPS/

#include "Registered_Data_Types.h"

#include "dds/DCPS/Util.h"

#include "tao/TAO_Singleton.h"

#include "ace/SString.h"

#include <set>

namespace OpenDDS
{
  namespace DCPS
  {

    Data_Types_Register::Data_Types_Register(void)
    {
    }


    Data_Types_Register::~Data_Types_Register(void)
    {
      TypeSupportHash*  supportHash;

      if (!domains_.empty() )
        {
          std::set<OpenDDS::DCPS::TypeSupport_ptr> typeSupports;
          DomainHash::iterator domainIter = domains_.begin();
          DomainHash::iterator domainEnd = domains_.end();

          while (domainIter != domainEnd)
            {
              supportHash = domainIter->second;
              ++domainIter;

              if (!supportHash->empty() )
                {
                  TypeSupportHash::iterator supportIter = supportHash->begin();
                  TypeSupportHash::iterator supportEnd = supportHash->end();

                  while (supportIter != supportEnd)
                    {
                      // ignore the error of adding a duplicate pointer.
                      // this done to handle a pointer having been registered
                      // to multiple names.
                      typeSupports.insert( supportIter->second );
                      ++supportIter;

                    } /* while (supportIter != supportEnd) */

                } /* if (0 < supportHash->current_size() ) */

              delete supportHash;

            } /* while (domainIter != domainEnd) */

          domains_.clear();

          std::set<OpenDDS::DCPS::TypeSupport_ptr>::iterator typesIter =
            typeSupports.begin();
          std::set<OpenDDS::DCPS::TypeSupport_ptr>::iterator typesEnd =
            typeSupports.end();

          while (typesEnd != typesIter)
            {
              OpenDDS::DCPS::TypeSupport_ptr type = *typesIter;
              ++typesIter;
              // if there are no more references then it will be deleted
              type->_remove_ref();
            }

        } /* if (0 < domains_.current_size() ) */

    }

    Data_Types_Register*
    Data_Types_Register::instance (void)
    {
      // Hide the template instantiation to prevent multiple instances
      // from being created.

      return
	TAO_Singleton<Data_Types_Register, TAO_SYNCH_MUTEX>::instance ();
    }



    ::DDS::ReturnCode_t Data_Types_Register::register_type (
      ::DDS::DomainParticipant_ptr domain_participant,
      ACE_CString type_name,
      OpenDDS::DCPS::TypeSupport_ptr the_type)
    {
      ::DDS::ReturnCode_t retCode = ::DDS::RETCODE_ERROR;
      TypeSupportHash*  supportHash = NULL;
      ACE_GUARD_RETURN(ACE_SYNCH_RECURSIVE_MUTEX, guard, lock_, retCode);

      if (0 == find(domains_, reinterpret_cast <void*> (domain_participant), supportHash))
        {
          int lookup = bind(*supportHash, type_name.c_str(), the_type);

          if (0 == lookup)
            {
              the_type->_add_ref();
              retCode = ::DDS::RETCODE_OK;
            }
          else if (1 == lookup)
            {
              OpenDDS::DCPS::TypeSupport_ptr currentType = NULL;
              if ( 0 == find(*supportHash, type_name.c_str(), currentType) )
                {
                  // Allow different TypeSupport instances of the same TypeSupport
                  // type register with the same type name in the same
                  // domain pariticipant. The second (and subsequent) registrations
                  // will be ignored.
                  CORBA::String_var the_type_name = the_type->get_type_name();
                  CORBA::String_var current_type_name = currentType->get_type_name();
                  if (ACE_OS::strcmp (the_type_name.in (), current_type_name.in ()) == 0)
                    {
                      retCode = ::DDS::RETCODE_OK;
                    } /* if (the_type == currentType) */
                } /* if ( 0 == supportHash->find(type_name, currentType) ) */
            } /* else if (1 == lookup) */
        }
      else
        {
          // new domain id!
          supportHash = new TypeSupportHash;

          if (0 == bind(domains_, reinterpret_cast<void*> (domain_participant), supportHash))
            {
              if (0 == bind(*supportHash, type_name.c_str(), the_type))
                {
                  the_type->_add_ref();
                  retCode = ::DDS::RETCODE_OK;
                }
            }
          else
            {
              delete supportHash;
            } /* if (0 == domains_.bind(domain_participant, supportHash)) */
        } /* if (0 == domains_.find(domain_participant, supportHash)) */

      return retCode;
    }


    OpenDDS::DCPS::TypeSupport_ptr Data_Types_Register::lookup(
      ::DDS::DomainParticipant_ptr domain_participant,
      ACE_CString type_name)
    {
      OpenDDS::DCPS::TypeSupport_ptr typeSupport = 0;
      ACE_GUARD_RETURN(ACE_SYNCH_RECURSIVE_MUTEX, guard, lock_, typeSupport);

      TypeSupportHash*  supportHash = NULL;

      if (0 == find(domains_, reinterpret_cast<void*> (domain_participant), supportHash))
        {
          if (0 != find(*supportHash, type_name.c_str(), typeSupport))
            {
              // reassign to nil to make sure that there was no partial
              // assignment in the find.
              typeSupport = 0;
            }
        }
      return typeSupport;
    }

  }

}
