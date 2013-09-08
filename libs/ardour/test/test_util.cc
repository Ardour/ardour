#include <fstream>
#include <sstream>
#include "pbd/xml++.h"
#include "pbd/textreceiver.h"
#include "ardour/session.h"
#include "ardour/audioengine.h"
#include <cppunit/extensions/HelperMacros.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static void
check_nodes (XMLNode const * p, XMLNode const * q, list<string> const & ignore_properties)
{
	CPPUNIT_ASSERT_EQUAL (p->is_content(), q->is_content());
	if (!p->is_content()) {
		CPPUNIT_ASSERT_EQUAL (p->name(), q->name());
	} else {
		CPPUNIT_ASSERT_EQUAL (p->content(), q->content());
	}

	XMLPropertyList const & pp = p->properties ();
	XMLPropertyList const & qp = q->properties ();
	CPPUNIT_ASSERT_EQUAL (pp.size(), qp.size());

	XMLPropertyList::const_iterator i = pp.begin ();
	XMLPropertyList::const_iterator j = qp.begin ();
	while (i != pp.end ()) {
		CPPUNIT_ASSERT_EQUAL ((*i)->name(), (*j)->name());
		if (find (ignore_properties.begin(), ignore_properties.end(), (*i)->name ()) == ignore_properties.end ()) {
			CPPUNIT_ASSERT_EQUAL ((*i)->value(), (*j)->value());
		}
		++i;
		++j;
	}

	XMLNodeList const & pc = p->children ();
	XMLNodeList const & qc = q->children ();

	CPPUNIT_ASSERT_EQUAL (pc.size(), qc.size());
	XMLNodeList::const_iterator k = pc.begin ();
	XMLNodeList::const_iterator l = qc.begin ();
	
	while (k != pc.end ()) {
		check_nodes (*k, *l, ignore_properties);
		++k;
		++l;
	}
}

void
check_xml (XMLNode* node, string ref_file, list<string> const & ignore_properties)
{
	XMLTree ref (ref_file);

	XMLNode* p = node;
	XMLNode* q = ref.root ();

	check_nodes (p, q, ignore_properties);
}

void
write_ref (XMLNode* node, string ref_file)
{
	XMLTree ref;
	ref.set_root (node);
	ref.write (ref_file);
}

class TestReceiver : public Receiver 
{
protected:
	void receive (Transmitter::Channel chn, const char * str) {
		const char *prefix = "";
		
		switch (chn) {
		case Transmitter::Error:
			prefix = ": [ERROR]: ";
			break;
		case Transmitter::Info:
			/* ignore */
			return;
		case Transmitter::Warning:
			prefix = ": [WARNING]: ";
			break;
		case Transmitter::Fatal:
			prefix = ": [FATAL]: ";
			break;
		case Transmitter::Throw:
			/* this isn't supposed to happen */
			abort ();
		}
		
		/* note: iostreams are already thread-safe: no external
		   lock required.
		*/
		
		cout << prefix << str << endl;
		
		if (chn == Transmitter::Fatal) {
			exit (9);
		}
	}
};

TestReceiver test_receiver;

/** @param dir Session directory.
 *  @param state Session state file, without .ardour suffix.
 */
Session *
load_session (string dir, string state)
{
	SessionEvent::create_per_thread_pool ("test", 512);

	test_receiver.listen_to (error);
	test_receiver.listen_to (info);
	test_receiver.listen_to (fatal);
	test_receiver.listen_to (warning);

	/* We can't use VSTs here as we have a stub instead of the
	   required bits in gtk2_ardour.
	*/
	Config->set_use_lxvst (false);

	AudioEngine* engine = AudioEngine::create ();
	init_post_engine ();

	CPPUNIT_ASSERT (engine->start () == 0);

	Session* session = new Session (*engine, dir, state);
	engine->set_session (session);
	return session;
}
