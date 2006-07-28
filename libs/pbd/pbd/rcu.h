#ifndef __pbd_rcu_h__
#define __pbd_rcu_h__

#include "boost/shared_ptr.hpp"
#include "glibmm/thread.h"
 
#include <list> 
 
template<class T>
class RCUManager
{
public:
 
	RCUManager (T* new_rcu_value)
		: m_rcu_value(new_rcu_value)
	{
 
	}
 
	virtual ~RCUManager() { }
 
	boost::shared_ptr<T> reader () const { return m_rcu_value; }
 
	// should be private
	virtual boost::shared_ptr<T> write_copy () = 0;
 
	// should be private
	virtual void update (boost::shared_ptr<T> new_value) = 0;
 
protected:
 
	boost::shared_ptr<T> m_rcu_value;
 
 
};
 
 
template<class T>
class SerializedRCUManager : public RCUManager<T>
{
public:
 
	SerializedRCUManager(T* new_rcu_value)
		: RCUManager<T>(new_rcu_value)
	{
 
	}
 
	virtual boost::shared_ptr<T> write_copy ()
	{
		m_lock.lock();
 
		// I hope this is doing what I think it is doing :)
		boost::shared_ptr<T> new_copy(new T(*RCUManager<T>::m_rcu_value));
 
		// XXX todo remove old copies with only 1 reference from the list.
 
		return new_copy;
	}
 
	virtual void update (boost::shared_ptr<T> new_value)
	{
		// So a current reader doesn't hold the only reference to
		// the existing value when we assign it a new value which 
		// should ensure that deletion of old values doesn't
		// occur in a reader thread.
		boost::shared_ptr<T> old_copy = RCUManager<T>::m_rcu_value;
 
		// we hold the lock at this point effectively blocking
		// other writers.
		RCUManager<T>::m_rcu_value = new_value;
 
 
		// XXX add the old value to the list of old copies.
 
		m_lock.unlock();
	}
 
private:
	Glib::Mutex			m_lock;
 
	std::list<boost::shared_ptr<T> > m_old_values;
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
