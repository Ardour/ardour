/* Copyright (C) 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm.h>
#include <iomanip>
#include <iostream>


namespace
{

#ifndef GLIBMM_EXCEPTIONS_ENABLED
//This is an alternative, to use when we have disabled exceptions:
std::auto_ptr<Glib::Error> processing_error;
#endif //GLIBMM_EXCEPTIONS_ENABLED

void file_get_contents(const std::string& filename, Glib::ustring& contents)
{
  #ifdef GLIBMM_EXCEPTIONS_ENABLED
  const Glib::RefPtr<Glib::IOChannel> channel = Glib::IOChannel::create_from_file(filename, "r");
  channel->read_to_end(contents);
  #else
  const Glib::RefPtr<Glib::IOChannel> channel = Glib::IOChannel::create_from_file(filename, "r", processing_error);
  channel->read_to_end(contents, processing_error);
  #endif //GLIBMM_EXCEPTIONS_ENABLED
}

Glib::ustring trim_whitespace(const Glib::ustring& text)
{
  Glib::ustring::const_iterator pbegin (text.begin());
  Glib::ustring::const_iterator pend   (text.end());

  while(pbegin != pend && Glib::Unicode::isspace(*pbegin))
    ++pbegin;

  Glib::ustring::const_iterator temp (pend);

  while(pbegin != temp && Glib::Unicode::isspace(*--temp))
    pend = temp;

  return Glib::ustring(pbegin, pend);
}


class DumpParser : public Glib::Markup::Parser
{
public:
  DumpParser();
  virtual ~DumpParser();

protected:
  virtual void on_start_element(Glib::Markup::ParseContext& context,
                                const Glib::ustring&        element_name,
                                const AttributeMap&         attributes);

  virtual void on_end_element(Glib::Markup::ParseContext& context,
                              const Glib::ustring& element_name);

  virtual void on_text(Glib::Markup::ParseContext& context, const Glib::ustring& text);

private:
  int parse_depth_;

  void indent();
};

DumpParser::DumpParser()
:
  parse_depth_ (0)
{}

DumpParser::~DumpParser()
{}

void DumpParser::on_start_element(Glib::Markup::ParseContext&,
                                  const Glib::ustring& element_name,
                                  const AttributeMap&  attributes)
{
  indent();
  std::cout << '<' << element_name;

  for(AttributeMap::const_iterator p = attributes.begin(); p != attributes.end(); ++p)
  {
    std::cout << ' ' << p->first << "=\"" << p->second << '"';
  }

  std::cout << ">\n";

  ++parse_depth_;
}

void DumpParser::on_end_element(Glib::Markup::ParseContext&, const Glib::ustring& element_name)
{
  --parse_depth_;

  indent();
  std::cout << "</" << element_name << ">\n";
}

void DumpParser::on_text(Glib::Markup::ParseContext&, const Glib::ustring& text)
{
  const Glib::ustring trimmed_text = trim_whitespace(text);

  if(!trimmed_text.empty())
  {
    indent();
    std::cout << trimmed_text << '\n';
  }
}

void DumpParser::indent()
{
  if(parse_depth_ > 0)
  {
    std::cout << std::setw(4 * parse_depth_)
      /* gcc 2.95.3 doesn't like this: << std::right */
      << ' ';
  }
}

} // anonymous namespace


int main(int argc, char** argv)
{
  if(argc < 2)
  {
    std::cerr << "Usage: parser filename\n";
    return 1;
  }

  DumpParser parser;
  Glib::Markup::ParseContext context (parser);

  #ifdef GLIBMM_EXCEPTIONS_ENABLED
  try
  {
  #endif //GLIBMM_EXCEPTIONS_ENABLED
    Glib::ustring contents;
    file_get_contents(argv[1], contents);

    context.parse(contents);
    context.end_parse();
  #ifdef GLIBMM_EXCEPTIONS_ENABLED
  }
  catch(const Glib::Error& error)
  {
    std::cerr << argv[1] << ": " << error.what() << std::endl;
    return 1;
  }
  #else
  if(processing_error.get())
  {
    std::cerr << argv[1] << ": " << processing_error->what() << std::endl;
    return 1;
  }
  #endif //GLIBMM_EXCEPTIONS_ENABLED


  return 0;
}

