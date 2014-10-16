#include "WCRefManager.h"

/// Construcotr.
WCRefManager::WCRefManager()
{
	m_RefCount = 1;
}

/// Destructor.
WCRefManager::~WCRefManager()
{
}

/// Adds a reference to class.
void WCRefManager::AddRef()
{
	m_RefCount++;
}

/// Decrements reference count and deletes the object if reference count becomes zero.
void WCRefManager::Release()
{
	m_RefCount--;
	if( m_RefCount <= 0 )
		delete this;
}