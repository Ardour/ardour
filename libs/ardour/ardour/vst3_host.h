/*
 * Copyright (C) 2019-2020 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _ardour_vst3_host_h_
#define _ardour_vst3_host_h_

#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include <glib.h>

#include <boost/shared_ptr.hpp>

#include "pbd/g_atomic_compat.h"
#include "ardour/libardour_visibility.h"
#include "vst3/vst3.h"

#define QUERY_INTERFACE_IMPL(Interface)                                       \
tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) SMTG_OVERRIDE \
{                                                                             \
  QUERY_INTERFACE (_iid, obj, FUnknown::iid, Interface)                       \
  QUERY_INTERFACE (_iid, obj, Interface::iid, Interface)                      \
  *obj = nullptr;                                                             \
  return kNoInterface;                                                        \
}

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

namespace Steinberg {

LIBARDOUR_API extern std::string tchar_to_utf8 (Vst::TChar const* s);
LIBARDOUR_API extern bool utf8_to_tchar (Vst::TChar* rv, const char* s, size_t l = 0);
LIBARDOUR_API extern bool utf8_to_tchar (Vst::TChar* rv, std::string const& s, size_t l = 0);

namespace Vst {
	/* see public.sdk/source/vst/vstpresetfile.cpp */
	typedef char       ChunkID[4];          // using ChunkID = char[4];
	static const int32 kClassIDSize   = 32; // ASCII-encoded FUID
	static const int32 kHeaderSize    = sizeof (ChunkID) + sizeof (int32) + kClassIDSize + sizeof (TSize);
	static const int32 kListOffsetPos = kHeaderSize - sizeof (TSize);
} // namespace Vst

class LIBARDOUR_API HostAttribute
{
public:
	enum Type {
		kInteger,
		kFloat,
		kString,
		kBinary
	};

	HostAttribute (int64 value)
		: _size (0)
		, _type (kInteger)
	{
		v.intValue = value;
	}

	HostAttribute (double value)
		: _size (0)
		, _type (kFloat)
	{
		v.floatValue = value;
	}

	HostAttribute (const Vst::TChar* value, uint32 size)
		: _size (size)
		, _type (kString)
	{
		v.stringValue = new Vst::TChar[_size + 1];
		memcpy (v.stringValue, value, _size * sizeof (Vst::TChar));
		v.stringValue[size] = 0;
	}

	HostAttribute (const void* value, uint32 size)
		: _size (size)
		, _type (kBinary)
	{
		v.binaryValue = new char[_size];
		memcpy (v.binaryValue, value, _size);
	}

	~HostAttribute ()
	{
		if (_size) {
			delete[] v.binaryValue;
		}
	}

	Type getType ()      const { return _type; }
	int64 intValue ()    const { return v.intValue; }
	double floatValue () const { return v.floatValue; }

	const Vst::TChar* stringValue (uint32& stringSize)
	{
		stringSize = _size;
		return v.stringValue;
	}

	const void* binaryValue (uint32& binarySize)
	{
		binarySize = _size;
		return v.binaryValue;
	}

protected:
	union v {
		int64       intValue;
		double      floatValue;
		Vst::TChar* stringValue;
		char*       binaryValue;
	} v;

	uint32 _size;
	Type   _type;

private:
	/* prevent copy construction */
	HostAttribute (HostAttribute const& other);
};

class LIBARDOUR_API RefObject : public FUnknown
{
public:
	RefObject ();
	virtual ~RefObject () {}
	uint32 PLUGIN_API addRef () SMTG_OVERRIDE;
	uint32 PLUGIN_API release () SMTG_OVERRIDE;

private:
	GATOMIC_QUAL gint _cnt; // atomic
};

class LIBARDOUR_API HostAttributeList : public Vst::IAttributeList, public RefObject
{
public:
	HostAttributeList ();
	virtual ~HostAttributeList ();

	QUERY_INTERFACE_IMPL (Vst::IAttributeList);

	uint32 PLUGIN_API addRef () SMTG_OVERRIDE
	{
		return RefObject::addRef ();
	}

	uint32 PLUGIN_API release () SMTG_OVERRIDE
	{
		return RefObject::release ();
	}

	tresult PLUGIN_API setInt (AttrID aid, int64 value) SMTG_OVERRIDE;
	tresult PLUGIN_API getInt (AttrID aid, int64& value) SMTG_OVERRIDE;
	tresult PLUGIN_API setFloat (AttrID aid, double value) SMTG_OVERRIDE;
	tresult PLUGIN_API getFloat (AttrID aid, double& value) SMTG_OVERRIDE;
	tresult PLUGIN_API setString (AttrID aid, const Vst::TChar* string) SMTG_OVERRIDE;
	tresult PLUGIN_API getString (AttrID aid, Vst::TChar* string, uint32 size) SMTG_OVERRIDE;
	tresult PLUGIN_API setBinary (AttrID aid, const void* data, uint32 size) SMTG_OVERRIDE;
	tresult PLUGIN_API getBinary (AttrID aid, const void*& data, uint32& size) SMTG_OVERRIDE;

protected:
	void removeAttrID (AttrID aid);

	std::map<std::string, HostAttribute*> list;
};

class LIBARDOUR_API HostMessage : public Vst::IMessage, public RefObject
{
public:
	HostMessage ();
	virtual ~HostMessage ();

	QUERY_INTERFACE_IMPL (Vst::IMessage);

	uint32 PLUGIN_API addRef () SMTG_OVERRIDE
	{
		return RefObject::addRef ();
	}

	uint32 PLUGIN_API release () SMTG_OVERRIDE
	{
		return RefObject::release ();
	}

	const char* PLUGIN_API          getMessageID () SMTG_OVERRIDE;
	void PLUGIN_API                 setMessageID (const char* messageID) SMTG_OVERRIDE;
	Vst::IAttributeList* PLUGIN_API getAttributes () SMTG_OVERRIDE;

protected:
	char*                                _messageId;
	boost::shared_ptr<HostAttributeList> _attribute_list;
};

class LIBARDOUR_API ConnectionProxy : public Vst::IConnectionPoint, public RefObject
{
public:
	ConnectionProxy (IConnectionPoint* src);
	~ConnectionProxy () SMTG_OVERRIDE;

	QUERY_INTERFACE_IMPL (Vst::IConnectionPoint);

	uint32 PLUGIN_API addRef () SMTG_OVERRIDE
	{
		return RefObject::addRef ();
	}

	uint32 PLUGIN_API release () SMTG_OVERRIDE
	{
		return RefObject::release ();
	}

	/* IConnectionPoint API */
	tresult PLUGIN_API connect (Vst::IConnectionPoint*) SMTG_OVERRIDE;
	tresult PLUGIN_API disconnect (Vst::IConnectionPoint*) SMTG_OVERRIDE;
	tresult PLUGIN_API notify (Vst::IMessage*) SMTG_OVERRIDE;

	bool disconnect ();

protected:
	IConnectionPoint* _src;
	IConnectionPoint* _dst;
};

class LIBARDOUR_API PlugInterfaceSupport : public Vst::IPlugInterfaceSupport
{
public:
	PlugInterfaceSupport ();
	QUERY_INTERFACE_IMPL (Vst::IPlugInterfaceSupport);

	uint32 PLUGIN_API addRef () SMTG_OVERRIDE
	{
		return 1;
	}

	uint32 PLUGIN_API release () SMTG_OVERRIDE
	{
		return 1;
	}

	tresult PLUGIN_API isPlugInterfaceSupported (const TUID) SMTG_OVERRIDE;

	void addPlugInterfaceSupported (const TUID);

private:
	std::vector<FUID> _interfaces;
};

class LIBARDOUR_API HostApplication : public Vst::IHostApplication
{
public:
	static Vst::IHostApplication* getHostContext ()
	{
		static HostApplication* app = new HostApplication;
		return app;
	}

	HostApplication ();
	virtual ~HostApplication () {}
	tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) SMTG_OVERRIDE;

	uint32 PLUGIN_API addRef () SMTG_OVERRIDE
	{
		return 1;
	}

	uint32 PLUGIN_API release () SMTG_OVERRIDE
	{
		return 1;
	}

	tresult PLUGIN_API getName (Vst::String128 name) SMTG_OVERRIDE;
	tresult PLUGIN_API createInstance (TUID cid, TUID _iid, void** obj) SMTG_OVERRIDE;

protected:
	boost::shared_ptr<PlugInterfaceSupport> _plug_interface_support;
};

class LIBARDOUR_LOCAL Vst3ParamValueQueue : public Vst::IParamValueQueue
{
public:
	QUERY_INTERFACE_IMPL (Vst::IParamValueQueue);

	uint32 PLUGIN_API addRef () SMTG_OVERRIDE
	{
		return 1;
	}

	uint32 PLUGIN_API release () SMTG_OVERRIDE
	{
		return 1;
	}

	static const int maxNumPoints = 64;

	Vst3ParamValueQueue ()
	{
		_values.reserve (maxNumPoints);
		_id = Vst::kNoParamId;
	}

	Vst::ParamID PLUGIN_API getParameterId () SMTG_OVERRIDE
	{
		return _id;
	}

	void setParameterId (Vst::ParamID id)
	{
		_values.clear ();
		_id = id;
	}

	int32 PLUGIN_API getPointCount () SMTG_OVERRIDE
	{
		return _values.size ();
	}

	tresult PLUGIN_API getPoint (int32 index, int32&, Vst::ParamValue&) SMTG_OVERRIDE;
	tresult PLUGIN_API addPoint (int32, Vst::ParamValue, int32&) SMTG_OVERRIDE;

protected:
	struct Value {
		Value (Vst::ParamValue v, int32 offset)
			: value (v)
			, sampleOffset (offset)
		{}

		Vst::ParamValue value;
		int32           sampleOffset;
	};

	std::vector<Value> _values;
	Vst::ParamID       _id;
};

class LIBARDOUR_LOCAL Vst3ParameterChanges : public Vst::IParameterChanges
{
public:
	QUERY_INTERFACE_IMPL (Vst::IParameterChanges);

	uint32 PLUGIN_API addRef () SMTG_OVERRIDE
	{
		return 1;
	}

	uint32 PLUGIN_API release () SMTG_OVERRIDE
	{
		return 1;
	}

	Vst3ParameterChanges ()
	{
		clear ();
	}

	void set_n_params (int n)
	{
		_queue.resize (n);
	}

	void clear ()
	{
		_used_queue_count = 0;
	}

	int32 PLUGIN_API getParameterCount () SMTG_OVERRIDE
	{
		return _used_queue_count;
	}

	Vst::IParamValueQueue* PLUGIN_API getParameterData (int32 index) SMTG_OVERRIDE;
	Vst::IParamValueQueue* PLUGIN_API addParameterData (Vst::ParamID const& id, int32& index) SMTG_OVERRIDE;

protected:
	std::vector<Vst3ParamValueQueue> _queue;
	int                              _used_queue_count;
};

class LIBARDOUR_LOCAL Vst3EventList : public Vst::IEventList
{
public:
	Vst3EventList ()
	{
		_events.reserve (128);
	}

	QUERY_INTERFACE_IMPL (Vst::IEventList)

	uint32 PLUGIN_API addRef () SMTG_OVERRIDE
	{
		return 1;
	}

	uint32 PLUGIN_API release () SMTG_OVERRIDE
	{
		return 1;
	}

	int32 PLUGIN_API PLUGIN_API getEventCount () SMTG_OVERRIDE
	{
		return _events.size ();
	}

	tresult PLUGIN_API getEvent (int32 index, Vst::Event& e) SMTG_OVERRIDE
	{
		if (index >= 0 && index < (int32)_events.size ()) {
			e = _events[index];
			return kResultTrue;
		} else {
			return kResultFalse;
		}
	}

	tresult PLUGIN_API addEvent (Vst::Event& e) SMTG_OVERRIDE
	{
		_events.push_back (e);
		return kResultTrue;
	}

	void clear ()
	{
		_events.clear ();
	}

protected:
	std::vector<Vst::Event> _events;
};

class LIBARDOUR_LOCAL RAMStream : public IBStream, public ISizeableStream, public Vst::IStreamAttributes
{
public:
	RAMStream ();
	RAMStream (uint8_t* data, size_t size);
	RAMStream (std::string const& fn);

	virtual ~RAMStream ();

	tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) SMTG_OVERRIDE;

	uint32 PLUGIN_API addRef () SMTG_OVERRIDE
	{
		return 1;
	}

	uint32 PLUGIN_API release () SMTG_OVERRIDE
	{
		return 1;
	}

	/* IBStream API */
	tresult PLUGIN_API read  (void* buffer, int32 numBytes, int32* numBytesRead) SMTG_OVERRIDE;
	tresult PLUGIN_API write (void* buffer, int32 numBytes, int32* numBytesWritten) SMTG_OVERRIDE;
	tresult PLUGIN_API seek  (int64 pos, int32 mode, int64* result) SMTG_OVERRIDE;
	tresult PLUGIN_API tell  (int64* pos) SMTG_OVERRIDE;

	/* ISizeableStream API */
	tresult PLUGIN_API getStreamSize (int64&) SMTG_OVERRIDE;
	tresult PLUGIN_API setStreamSize (int64) SMTG_OVERRIDE;

	/* IStreamAttributes API */
	tresult PLUGIN_API   getFileName (Vst::String128 name) SMTG_OVERRIDE;
	Vst::IAttributeList* PLUGIN_API getAttributes () SMTG_OVERRIDE;

	/* convenience API for state I/O */
	void rewind ()
	{
		_pos = 0;
	}

	bool readonly () const
	{
		return _readonly;
	}

	bool write_int32 (int32 i);
	bool write_int64 (int64 i);
	bool write_ChunkID (const Vst::ChunkID& id);
	bool write_TUID (const TUID& tuid);

	bool read_int32 (int32& i);
	bool read_int64 (int64& i);
	bool read_ChunkID (Vst::ChunkID& id);
	bool read_TUID (TUID& tuid);

	/* direct access */
	uint8_t const* data () const
	{
		return _data;
	}

	int64 size () const
	{
		return _size;
	}

#ifndef NDEBUG
	void hexdump (int64 max_len = 64) const;
#endif

private:
	bool reallocate_buffer (int64 size, bool exact);

	template <typename T>
	bool read_pod (T& t)
	{
		int32 n_read = 0;
		read ((void*)&t, sizeof (T), &n_read);
		return n_read == sizeof (T);
	}

	template <typename T>
	bool write_pod (const T& t)
	{
		int32 written = 0;
		write (const_cast<void*> ((const void*)&t), sizeof (T), &written);
		return written == sizeof (T);
	}

	uint8_t* _data;
	int64    _size;
	int64    _alloc;
	int64    _pos;
	bool     _readonly;

	HostAttributeList attribute_list;
};

class LIBARDOUR_LOCAL ROMStream : public IBStream
{
public:
	ROMStream (IBStream& src, TSize offset, TSize size);
	virtual ~ROMStream ();

	tresult PLUGIN_API queryInterface (const TUID _iid, void** obj) SMTG_OVERRIDE;

	uint32 PLUGIN_API addRef () SMTG_OVERRIDE
	{
		return 1;
	}

	uint32 PLUGIN_API release () SMTG_OVERRIDE
	{
		return 1;
	}

	/* IBStream API */
	tresult PLUGIN_API read  (void* buffer, int32 numBytes, int32* numBytesRead) SMTG_OVERRIDE;
	tresult PLUGIN_API write (void* buffer, int32 numBytes, int32* numBytesWritten) SMTG_OVERRIDE;
	tresult PLUGIN_API seek  (int64 pos, int32 mode, int64* result) SMTG_OVERRIDE;
	tresult PLUGIN_API tell  (int64* pos) SMTG_OVERRIDE;

	void rewind ()
	{
		_pos = 0;
	}

protected:
	IBStream& _stream;
	int64     _offset;
	int64     _size;
	int64     _pos;
};

#if defined(__clang__)
#pragma clang diagnostic pop
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#endif

} // namespace Steinberg
#endif
