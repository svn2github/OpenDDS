// -*- C++ -*-
//
// $Id$
#ifndef TAO_DDS_RCOBJECT_T_H
#define TAO_DDS_RCOBJECT_T_H

#include  "ace/Atomic_Op.h"
#include  "ace/Malloc_Base.h"


namespace TAO
{
  namespace DCPS
  {

    /// Templated reference counting mix-in.
    /// A non-DDS specific helper class.
    /// The T type is an ace lock type
    /// (eg, ACE_SYNCH_MUTEX, ACE_NULL_MUTEX, etc...)
    template <typename T>
    class RcObject
    {
      public:

        virtual ~RcObject()
          {
          }

        virtual void _add_ref()
          {
            ++this->ref_count_;
          }

        virtual void _remove_ref()
          {
            long new_count = --this->ref_count_;

            if (new_count == 0)
              {
                // No need to protect the allocator with a lock since this
                // is the last reference to this object, and thus only one
                // thread will be doing this final _remove_ref().
                ACE_Allocator* allocator = this->allocator_;
                this->allocator_ = 0;

                if (allocator)
                  {
                    allocator->free(this);
                  }
                else
                  {
                    delete this;
                  }
              }
          }


      protected:

        RcObject(ACE_Allocator* allocator = 0)
          : ref_count_(1), allocator_(allocator)
          {
          }


      private:

        ACE_Atomic_Op<T, long> ref_count_;
        ACE_Allocator*         allocator_;

        // Turning these off.  I don't think they should be used for
        // objects that would be reference counted.  Maybe a copy_from()
        // method instead (if needed).
        RcObject(const RcObject&);
        RcObject& operator=(const RcObject&);
    };

  }
}

#endif  /*TAO_DDS_RCOBJECT_T_H */
