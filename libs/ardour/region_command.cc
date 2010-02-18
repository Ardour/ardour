#include "pbd/convert.h"
#include "pbd/xml++.h"
#include "pbd/locale_guard.h"
#include "pbd/enumwriter.h"

#include "ardour/region.h"
#include "ardour/utils.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

RegionCommand::RegionCommand (boost::shared_ptr<Region> r, const XMLNode& node)
	: region (r)
{
	if (set_state (node, 0)) {
		throw failed_constructor();
	}
}

RegionCommand::RegionCommand (boost::shared_ptr<Region> r)
	: region (r)
{
}

RegionCommand::RegionCommand (boost::shared_ptr<Region> r, Property prop, const std::string& target_value)
	: region (r)
{
	LocaleGuard lg ("POSIX");
	string before;
	char buf[128];

	/* get current value as a string */
	
	switch (prop) {
	case Name:
		before = r->name();
		break;
	case PositionLockStyle:
		before = enum_2_string (r->positional_lock_style());
		break;
	case Length:
		snprintf (buf, sizeof (buf), "%" PRId32, r->length());
		before = buf;
		break;
	case Start:
		snprintf (buf, sizeof (buf), "%" PRId32, r->start());
		before = buf;
		break;
	case Position:
		snprintf (buf, sizeof (buf), "%" PRId32, r->position());
		before = buf;
		break;
	case PositionOnTop:
		snprintf (buf, sizeof (buf), "%" PRId32, r->position());
		before = buf;
		break;
	case Layer:
		snprintf (buf, sizeof (buf), "%" PRId32, r->layer());
		before = buf;
		break;
	case SyncPosition:
		snprintf (buf, sizeof (buf), "%" PRId32, r->sync_position());
		before = buf;
		break;
	case Hidden:
		before = (r->hidden() ? "yes" : "no");
		break;
	case Muted:
		before = (r->muted() ? "yes" : "no");
		break;
	case Opaque:
		before = (r->opaque() ? "yes" : "no");
		break;
	case Locked:
		before = (r->locked() ? "yes" : "no");
		break;
	case PositionLocked:
		before = (r->position_locked() ? "yes" : "no");
		break;
		
        /* audio */

	case ScaleAmplitude:
		break;
	case FadeInActive:
		break;
	case FadeInShape:
		break;
	case FadeInLength:
		break;
	case FadeIn:
		break;
	case FadeOutActive:
		break;
	case FadeOutShape:
		break;
	case FadeOutLength:
		break;
	case FadeOut:
		break;
	case EnvelopActive:
		break;
	case DefaultEnvelope:
		break;
		
	}

	add_property_change (prop, before, target_value);
}

void
RegionCommand::_add_property_change (Property prop, const std::string& before, const std::string& after)
{
	property_changes.push_back (PropertyTriple (prop, before, after));
}

void
RegionCommand::operator() ()
{
	region->freeze ();
	for (PropertyTriples::iterator i= property_changes.begin(); i != property_changes.end(); ++i) {
		do_property_change (i->property, i->after);
	}
	region->thaw ();
}

void
RegionCommand::undo ()
{
	region->freeze ();
	for (PropertyTriples::iterator i= property_changes.begin(); i != property_changes.end(); ++i) {
		do_property_change (i->property, i->before);
	}
	region->thaw ();
}

void
RegionCommand::do_property_change (Property prop, const std::string& value)
{
	Region::PositionLockStyle pls;

	switch (prop) {
	case Name:
		region->set_name (value);
		break;
	case PositionLockStyle:
		region->set_position_lock_style ((Region::PositionLockStyle) string_2_enum (value, pls));
		break;
	case Length:
		region->set_length (atoll (value), this);
		break;
	case Start:
		region->set_start (atoll (value), this);
		break;
	case Position:
		region->set_position (atoll (value), this);
		break;
	case PositionOnTop:
		region->set_position_on_top (atoll (value), this);
		break;
	case Layer:
		region->set_layer (atoi (value));
		break;
	case SyncPosition:
		region->set_sync_position (atoi (value));
		break;
	case Hidden:
		region->set_hidden (string_is_affirmative (value));
		break;
	case Muted:
		region->set_muted (string_is_affirmative (value));
		break;
	case Opaque:
		region->set_opaque (string_is_affirmative (value));
		break;
	case Locked:
		region->set_locked (string_is_affirmative (value));
		break;
	case PositionLocked:
		region->set_position_locked (string_is_affirmative (value));
		break;

        /* audio */

	case ScaleAmplitude:
		break;
	case FadeInActive:
		break;
	case FadeInShape:
		break;
	case FadeInLength:
		break;
	case FadeIn:
		break;
	case FadeOutActive:
		break;
	case FadeOutShape:
		break;
	case FadeOutLength:
		break;
	case FadeOut:
		break;
	case EnvelopActive:
		break;
	case DefaultEnvelope:
		break;
		
	}
}

XMLNode&
RegionCommand::get_state ()
{
	XMLNode* node = new XMLNode (X_("RegionCommand"));
	XMLNode* child;
	
	node->add_property (X_("region"), region->id().to_s());
	
	for (PropertyTriples::iterator i = property_changes.begin(); i != property_changes.end(); ++i) {

		child = new XMLNode (X_("Op"));

		child->add_property (X_("property"), enum_2_string (i->property));
		child->add_property (X_("before"), i->before);
		child->add_property (X_("after"), i->after);

		node->add_child_nocopy (*child);
	}

	return *node;
}

int
RegionCommand::set_state (const XMLNode& node, int /* version */)
{
	const XMLNodeList& children (node.children());
	Property property;
	string before;
	string after;
	const XMLProperty* prop;
	
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {

		if ((*i)->name() != X_("Op")) {
			continue;
		}

		if ((prop = (*i)->property (X_("property"))) == 0) {
			return -1;
		}

		property = (Property) string_2_enum (prop->value(), property);

		if ((prop = (*i)->property (X_("before"))) == 0) {
			return -1;
		}

		before = prop->value();

		if ((prop = (*i)->property (X_("after"))) == 0) {
			return -1;
		}

		after = prop->value();

		add_property_change (property, before, after);
	}

	return 0;
}
