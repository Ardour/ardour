/*
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2009 Hans Baier <hansfbaier@googlemail.com>
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

#include <cassert>
#include <stdint.h>
#include <sigc++/sigc++.h>
#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include "temporal/beats.h"
#include "evoral/SMF.h"
#include "SequenceTest.h"

using namespace Evoral;

class TestSMF : public SMF {
public:
	std::string path() const { return _path; }

	int open(const std::string& path) {
		_path = path;
		return SMF::open(path);
	}

	void close() {
		return SMF::close();
	}

	int read_event(uint32_t* delta_t, uint32_t* size, uint8_t** buf) const {
		event_id_t id;
		return SMF::read_event(delta_t, size, buf, &id);
	}

private:
	std::string  _path;
};

class SMFTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(SMFTest);
	CPPUNIT_TEST(createNewFileTest);
	CPPUNIT_TEST(takeFiveTest);
	CPPUNIT_TEST(writeTest);
	CPPUNIT_TEST_SUITE_END();

public:
	typedef Temporal::Beats Time;

	void setUp() {
		type_map = new DummyTypeMap();
		assert(type_map);
		seq = new MySequence<Time>(*type_map);
		assert(seq);
	}

	void tearDown() {
		delete seq;
		delete type_map;
	}

	void createNewFileTest();
	void takeFiveTest();
	void writeTest();

private:
	DummyTypeMap*     type_map;
	MySequence<Time>* seq;
};

