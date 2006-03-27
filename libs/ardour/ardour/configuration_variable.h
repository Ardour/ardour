#ifndef __ardour_configuration_variable_h__
#define __ardour_configuration_variable_h__

#include <sstream>
#include <ostream>

#include <pbd/xml++.h>

namespace ARDOUR {

class ConfigVariableBase {
  public:
	ConfigVariableBase (std::string str) : _name (str), _is_user (false) {}
	virtual ~ConfigVariableBase() {}

	std::string name() const { return _name; }
	bool is_user() const { return _is_user; }
	void set_is_user (bool yn) { _is_user = yn; }
	
	virtual void add_to_node (XMLNode& node) = 0;
	virtual bool set_from_node (const XMLNode& node) = 0;

  protected:
	std::string _name;
	bool _is_user;
};

template<class T>
class ConfigVariable : public ConfigVariableBase
{
  public:
	ConfigVariable (std::string str) : ConfigVariableBase (str) {}
	ConfigVariable (std::string str, T val) : ConfigVariableBase (str), value (val) {}

	virtual void set (T val) {
		value = val;
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

	bool set_from_node (const XMLNode& node) {
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
							return true;
						}
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

	void set (T val) {
		unmutated_value = val;
		ConfigVariable<T>::set (mutator (val));
	}

  protected:
	virtual T get_for_save() { return unmutated_value; }
	T unmutated_value;
	T (*mutator)(T);
};

}

#endif /* __ardour_configuration_variable_h__ */
