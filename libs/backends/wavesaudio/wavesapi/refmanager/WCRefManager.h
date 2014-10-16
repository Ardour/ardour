/*
    Copyright (C) 2014 Waves Audio Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef WCREFMANAGER_H
#define WCREFMANAGER_H


#define SAFE_RELEASE(p) if (p) {p->Release(); p = NULL;}


//In order to use this interface, derive the Interface class
//from WCRefManager_Interface and derive the implementation class
//from WCRefManager_Impl. Further, in the implementation class
//declaration, place the macro WCREFMANAGER_IMPL.
class WCRefManager_Interface
{
public:
	/// Constructor.
	WCRefManager_Interface() {};
	/// Destructor.
	virtual ~WCRefManager_Interface() {};
	/// Adds a reference to class.
	virtual void AddRef() = 0;
	/// Decrements reference count and deletes the object if reference count becomes zero.
	virtual void Release() = 0;
};

///! See details at WCRefManager_Interface for how to use this.
class WCRefManager_Impl
{
public:
    WCRefManager_Impl () : m_RefCount(1) {}
    virtual ~WCRefManager_Impl() {}
protected:
	/// Variable to store reference count.
	unsigned int m_RefCount;

/// Helper to put implementation in an interface derived class, don't forget to
/// derive the impl from WCRefManager_Impl
#define WCREFMAN_IMPL \
    public: \
        virtual void AddRef() {m_RefCount++;} \
        virtual void Release() {m_RefCount--; if (m_RefCount<=0) delete this;}

};


class WCRefManager
{
public:
	/// Construcotr.
	WCRefManager();
	/// Destructor.
	virtual ~WCRefManager();
	/// Adds a reference to class.
	void AddRef();
	/// Decrements reference count and deletes the object if reference count becomes zero.
	void Release();

private:
	/// Variable to store reference count.
	unsigned int m_RefCount;
};

#endif // WCREFMANAGER_H
