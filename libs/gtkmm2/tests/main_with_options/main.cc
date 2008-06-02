//$Id: main.cc 616 2006-05-11 11:40:25Z murrayc $ -*- c++ -*-

/* gtkmm example Copyright (C) 2002 gtkmm development team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <gtkmm.h>
#include <iomanip>
#include <iostream>


class ExampleOptionGroup : public Glib::OptionGroup
{ 
public:
  ExampleOptionGroup();

  virtual bool on_pre_parse(Glib::OptionContext& context, Glib::OptionGroup& group);
  virtual bool on_post_parse(Glib::OptionContext& context, Glib::OptionGroup& group);
  virtual void on_error(Glib::OptionContext& context, Glib::OptionGroup& group);
  
  //These int instances should live as long as the OptionGroup to which they are added, 
  //and as long as the OptionContext to which those OptionGroups are added.
  int m_arg_foo;
  std::string m_arg_filename;
  Glib::ustring m_arg_goo;
  bool m_arg_boolean;
  Glib::OptionGroup::vecustrings m_arg_list;
};

ExampleOptionGroup::ExampleOptionGroup()
: Glib::OptionGroup("example_group", "description of example group", "help description of example group"),
  m_arg_foo(0), m_arg_boolean(false)
{
  Glib::OptionEntry entry1;
  entry1.set_long_name("foo");
  entry1.set_short_name('f');
  entry1.set_description("The Foo");
  add_entry(entry1, m_arg_foo);
      
  Glib::OptionEntry entry2;
  entry2.set_long_name("file");
  entry2.set_short_name('F');
  entry2.set_description("The Filename");
  add_entry_filename(entry2, m_arg_filename);
 
  Glib::OptionEntry entry3;
  entry3.set_long_name("goo");
  entry3.set_short_name('g');
  entry3.set_description("The Goo");
  add_entry(entry3, m_arg_goo);
  
  Glib::OptionEntry entry4;
  entry4.set_long_name("activate_something");
  entry4.set_description("Activate something");
  add_entry(entry4, m_arg_boolean);
  
  Glib::OptionEntry entry5;
  entry5.set_long_name("list");
  entry5.set_short_name('l');
  entry5.set_description("The List");
  add_entry(entry5, m_arg_list);
}

bool ExampleOptionGroup::on_pre_parse(Glib::OptionContext& context, Glib::OptionGroup& group)
{
  //This is called before the m_arg_* instances are given their values.
  return Glib::OptionGroup::on_pre_parse(context, group);
}

bool ExampleOptionGroup::on_post_parse(Glib::OptionContext& context, Glib::OptionGroup& group)
{
  //This is called after the m_arg_* instances are given their values.
  return Glib::OptionGroup::on_post_parse(context, group);
}

void ExampleOptionGroup::on_error(Glib::OptionContext& context, Glib::OptionGroup& group)
{
  Glib::OptionGroup::on_error(context, group);
}

int main(int argc, char *argv[])
{
  //This example should be executed like so:
  //./example --foo=1 --bar=2 --goo=abc
  //./example --help
   
  Glib::OptionContext context;
  
  ExampleOptionGroup group;
  context.set_main_group(group);
  
  #ifdef GLIBMM_EXCEPTIONS_ENABLED
  try
  {
  #endif //GLIBMM_EXCEPTIONS_ENABLED
    Gtk::Main main_instance(argc, argv, context);
  
    //Here we can see the parsed values of our custom command-line arguments:

    std::cout << "parsed values: " << std::endl <<
      "  foo = " << group.m_arg_foo << std::endl << 
      "  filename = " << group.m_arg_filename << std::endl <<
      "  activate_something = " << (group.m_arg_boolean ? "enabled" : "disabled") << std::endl <<
      "  goo = " << group.m_arg_goo << std::endl;
    
    //This one shows the results of multiple instance of the same option, such as --list=1 --list=a --list=b
    std::cout << "  list = ";
    for(Glib::OptionGroup::vecustrings::const_iterator iter = group.m_arg_list.begin(); iter != group.m_arg_list.end(); ++iter)
    {
      std::cout << *iter << ", ";
    }
    std::cout << std::endl;
 

    //Any standard GTK+ command-line arguments will have an effect on this window:
    //Try --name="bobble" to change the window's title to "bobble", for instance.
    Gtk::Window testWindow;
    main_instance.run(testWindow); //Shows the window and returns when it is closed.
  #ifdef GLIBMM_EXCEPTIONS_ENABLED
  }
  catch(const Glib::Error& ex)
  {
    std::cout << "Exception: " << ex.what() << std::endl;
  }
  #endif //GLIBMM_EXCEPTIONS_ENABLED


  return 0;
}
