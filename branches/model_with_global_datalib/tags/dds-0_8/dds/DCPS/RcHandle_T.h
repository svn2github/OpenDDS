// -*- C++ -*-
//
// $Id$
#ifndef TAO_DDS_RCHANDLE_T_H
#define TAO_DDS_RCHANDLE_T_H


namespace TAO
{

  namespace DCPS
  {

    /// Templated Reference counted handle to a pointer.
    /// A non-DDS specific helper class.
    template <typename T>
    class RcHandle
    {
      public:

        RcHandle(void)
          : ptr_(0)
          {
          }

        RcHandle(T *p, bool take_ownership = true)
          : ptr_(p)
          {
            if (!take_ownership)
              {
                this->bump_up();
              }
          }

        RcHandle(const RcHandle &b)
          : ptr_(b.ptr_)
          {
            this->bump_up();
          }

        ~RcHandle(void)
          {
            this->bump_down();
          }

        RcHandle& operator=(T* p)
          {
            if (this->ptr_ != p)
              {
                this->bump_down();
                this->ptr_ = p;
              }

            return *this;
          }


        RcHandle& operator=(const RcHandle& b)
          {
            if (this->ptr_ != b.ptr_)
              {
                this->bump_down();
                this->ptr_ = b.ptr_;
                this->bump_up();
              }

            return *this;
          }

        T* operator->() const
          {
            return this->ptr_;
          }

        unsigned is_nil() const
          {
            return this->ptr_ == 0;
          }

        T* in(void) const
          {
            return this->ptr_;
          }

        T*& inout(void)
          {
            return this->ptr_;
          }

        T*& out(void)
          {
            this->bump_down();
            return this->ptr_;
          }

        T *_retn (void)
          {
            T* retval = this->ptr_;
            this->ptr_ = 0;
            return retval;
          }


      private:

        void bump_up()
          {
            if (this->ptr_ != 0)
              {
                this->ptr_->_add_ref ();
              }
          }

        void bump_down()
          {
            if (this->ptr_ != 0)
              {
                this->ptr_->_remove_ref();
                this->ptr_ = 0;
              }
          }


        /// The actual "unsmart" pointer to the T object.
        T* ptr_;
    };


  }  /* namespace DCPS */

}  /* namespace TAO */

#endif  /* TAO_DDS_RCHANDLE_T_H */
