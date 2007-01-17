#ifndef __ardour_configuration_variable_h__
#define __ardour_configuration_variable_h__

#include <sstream>
#include <ostream>

#include <pbd/xml++.h>

namespace ARDOUR {

class ConfigVariableBase {
  public:
	enum Owner {
		Default = 0x1,
		System = 0x2,
		Config = 0x4,
		Session = 0x8,
		Interface = 0x10
	};

	ConfigVariableBase (std::string str) : _name (str), _owner (Default) {}
	virtual ~ConfigVariableBase() {}

	std::string name() const { return _name; }
	Owner owner() const { return _owner; }

	virtual void add_to_node (XMLNode& node) = 0;
	virtual bool set_from_node (const XMLNode& node, Owner owner) = 0;


  protected:
	std::string _name;
	Owner _owner;

	void notify ();
	void miss ();
};

template<class T>
class ConfigVariable : public ConfigVariableBase
{
  public:
	ConfigVariable (std::string str) : ConfigVariableBase (str) {}
	ConfigVariable (std::string str, T val) : ConfigVariableBase (str), value (val) {}

	virtual bool set (T val, Owner owner) {
		if (val == value) {
			miss ();
			return false;
		}
		value = val;
		_owner = (ConfigVariableBase::Owner)(_owner |owner);
		notify ();
		return true;
	}

	T get() const {
		return value;
	}

	void add_to_node (XMLNode& node) {
		std::stringstream ss;
		ss << value;
		XMLNode* child = new XMLNode ("Option");
		child->add_property ("name", _name);
		child->add_property ("value", ss.str());
		node.add_child_nocopy (*child);
	}

	bool set_from_node (const XMLNode& node, Owner owner) {

		if (node.name() == "Config") {

			/* ardour.rc */

			const XMLProperty* prop;
			XMLNodeList nlist;
			XMLNodeConstIterator niter;
			XMLNode* child;
			
			nlist = node.children();
			
			for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
				
				child = *niter;
				
				if (child->name() == "Option") {
					if ((prop = child->property ("name")) != 0) {
						if (prop->value() == _name) {
							if ((prop = child->property ("value")) != 0) {
								std::stringstream ss;
								ss << prop->value();
								ss >> value;
								_owner = (ConfigVariableBase::Owner)(_owner |owner);
								return true;
							}
						}
					}
				}
			}
			
		} else if (node.name() == "Options") {

			/* session file */

			XMLNodeList olist;
			XMLNodeConstIterator oiter;
			XMLNode* option;
			const XMLProperty* opt_prop;
			
			olist = node.children();
			
			for (oiter = olist.begin(); oiter != olist.end(); ++oiter) {
				
				option = *oiter;
				
				if (option->name() == _name) {
					if ((opt_prop = option->property ("val")) != 0) {
						std::stringstream ss;
						ss << opt_prop->value();
						ss >> value;
						_owner = (ConfigVariableBase::Owner)(_owner |owner);
						return true;
					}
				}
			}
		}

		return false;
	}

  protected:
	virtual T get_for_save() { return value; }
	T value;
};

template<class T>
class ConfigVariableWithMutation : public ConfigVariable<T>
{
  public:
	ConfigVariableWithMutation (std::string name, T val, T (*m)(T)) 
		: ConfigVariable<T> (name, val), mutator (m) {}

	bool set (T val, ConfigVariableBase::Owner owner) {
		if (unmutated_value != val) {
			unmutated_value = val;
			return ConfigVariable<T>::set (mutator (val), owner);
		} 
		return false;
	}

  protected:
	virtual T get_for_save() { return unmutated_value; }
	T unmutated_value;
	T (*mutator)(T);
};

}

#endif /* __ardour_configuration_variable_h__ */
