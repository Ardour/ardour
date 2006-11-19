#ifndef __pbd_rcu_h__
#define __pbd_rcu_h__

#include "boost/shared_ptr.hpp"
#include "glibmm/thread.h"
 
#include <list> 

template<class T>
class RCUManager
{
  public:
 
	RCUManager (T* new_rcu_value) {
		m_rcu_value = new boost::shared_ptr<T> (new_rcu_value);
	}
 
	virtual ~RCUManager() { delete m_rcu_value; }
 
        boost::shared_ptr<T> reader () const { return *((boost::shared_ptr<T> *) g_atomic_pointer_get (the_pointer())); }
 
	virtual boost::shared_ptr<T> write_copy () = 0;
	virtual bool update (boost::shared_ptr<T> new_value) = 0;

  protected:
	boost::shared_ptr<T>* m_rcu_value;

	// this monstrosity is needed because of some wierd behavior by g++

	gpointer * the_pointer() const { return (gpointer *) &m_rcu_value; }
};
 
 
template<class T>
class SerializedRCUManager : public RCUManager<T>
{
public:
 
	SerializedRCUManager(T* new_rcu_value)
		: RCUManager<T>(new_rcu_value)
	{
 
	}
 
	boost::shared_ptr<T> write_copy ()
	{
		m_lock.lock();

		// clean out any dead wood

		typename std::list<boost::shared_ptr<T> >::iterator i;

		for (i = m_dead_wood.begin(); i != m_dead_wood.end(); ) {
			if ((*i).use_count() == 1) {
				i = m_dead_wood.erase (i);
			} else {
				++i;
			}
		}

		// store the current 

		current_write_old = RCUManager<T>::m_rcu_value;
		
		boost::shared_ptr<T> new_copy (new T(**current_write_old));

		return new_copy;
	}
 
	bool update (boost::shared_ptr<T> new_value)
	{
		// we hold the lock at this point effectively blocking
		// other writers.

		boost::shared_ptr<T>* new_spp = new boost::shared_ptr<T> (new_value);

		// update, checking that nobody beat us to it

		bool ret = g_atomic_pointer_compare_and_exchange (RCUManager<T>::the_pointer(),
								  (gpointer) current_write_old,
								  (gpointer) new_spp);
		
		if (ret) {

			// successful update : put the old value into dead_wood,

			m_dead_wood.push_back (*current_write_old);

			// now delete it - this gets rid of the shared_ptr<T> but
			// because dead_wood contains another shared_ptr<T> that
			// references the same T, the underlying object lives on

			delete current_write_old;
		}

		m_lock.unlock();

		return ret;
	}

	void flush () {
		Glib::Mutex::Lock lm (m_lock);
		m_dead_wood.clear ();
	}
 
private:
	Glib::Mutex			 m_lock;
	boost::shared_ptr<T>*            current_write_old;
	std::list<boost::shared_ptr<T> > m_dead_wood;
};
 
template<class T>
class RCUWriter
{
public:
 
	RCUWriter(RCUManager<T>& manager)
		: m_manager(manager)
	{
		m_copy = m_manager.write_copy();	
	}
 
	~RCUWriter()
	{
		// we can check here that the refcount of m_copy is 1
 
		if(m_copy.use_count() == 1) {
			m_manager.update(m_copy);
		} else {
 
			// critical error.
		}
 
	}
 
	// or operator boost::shared_ptr<T> ();
	boost::shared_ptr<T> get_copy() { return m_copy; }
 
private:
 
	RCUManager<T>& m_manager;
 
	// preferably this holds a pointer to T
	boost::shared_ptr<T> m_copy;
};

#endif /* __pbd_rcu_h__ */
