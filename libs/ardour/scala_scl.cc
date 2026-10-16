/****************************************************************
          libscala-file, (C) 2020 Mark Conway Wirt

Copyright (c) 2020 Mark Conway Wirt

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
****************************************************************/

#include <iostream>
#include <fstream>
#include <regex>
#include <math.h>
#include <stdexcept>

#if defined(PLATFORM_WINDOWS) && defined(COMPILER_MINGW)
#include <glibmm.h>
#endif

#include "ardour/scala_file.h"

namespace scala {

scale
read_scl (std::ifstream& input_file)
{
	/*
	   C++ Code to parse the Scala scale file, as documented here:

	   http://www.huygens-fokker.org/scala/scl_format.html

	   A couple of extensions:

	   - Allow white-space before the comment character.  Spec is a little ambiguous.
	   - Allow blank lines. In the standard only the scale name is mentioned as potentially blank.
	*/

	int non_commnets_processed = 0;
#ifdef SCALA_STRICT
	unsigned long entries = 0;
#endif
	int numerator, denominator;
	std::string buffer;
#if defined(PLATFORM_WINDOWS) && defined(COMPILER_MINGW)
	std::smatch match;
	Glib::MatchInfo trimmed;
#else
	std::smatch match, trimmed;
#endif
	scale scala_scale;
#ifdef SCALA_STRICT
	bool description_parsed = false ;
#endif

#ifdef SCALA_STRICT
	std::regex COMMENT_REGEX = std::regex("^!.*");
#else
#if defined(PLATFORM_WINDOWS) && defined(COMPILER_MINGW)
	Glib::RefPtr<Glib::Regex> COMMENT_REGEX = Glib::Regex::create ("[ \t]*!.*");
#else
	std::regex COMMENT_REGEX = std::regex("[ \t]*!.*");
#endif
#endif

	while (input_file) {
		getline(input_file, buffer);
#if defined(PLATFORM_WINDOWS) && defined(COMPILER_MINGW)
		if (COMMENT_REGEX->match (buffer)) {
#else
		if (std::regex_match (buffer, COMMENT_REGEX)) {
#endif
			// We're defining a comment as the first non-whitespace character being a "!".
#if defined(PLATFORM_WINDOWS) && defined(COMPILER_MINGW)
		} else if (Glib::Regex::create ("[ \t]*")->match (buffer)) {
#else
		} else if (std::regex_match (buffer, std::regex("[ \t]*") )) {
#endif
			// Blank line. Discard. This may be an extension of the format.
#ifdef SCALA_STRICT
			if (description_parsed) {
				// If we're at a blank line which is not the description, assume it's
				// a final linefeed at the end of the file.
				break;
			}
			if (non_commnets_processed == 0){
				description_parsed = true;
			}
#endif
			non_commnets_processed = non_commnets_processed + 1;

		} else {
			// Extract the part after optional leading whitespace and before an optional space and label
#if defined(PLATFORM_WINDOWS) && defined(COMPILER_MINGW)
			Glib::RefPtr<Glib::Regex> regex = Glib::Regex::create ("([ \t]*)([^ \t]*)(.*)");
			regex->match (buffer, trimmed);
			std::string entry = std::string(trimmed.fetch(2)); /* Might be empty */
#else
			std::regex_search(buffer, trimmed, std::regex("([ \t]*)([^ \t]*)(.*)"));
			std::string entry = trimmed[2]; /* Might be empty */
#endif
			if (non_commnets_processed == 0) {
				// First non-comment is the description. Can be ignored
				non_commnets_processed = non_commnets_processed + 1;
				scala_scale.name = entry;
#ifdef SCALA_STRICT
				description_parsed = true;
#endif
				continue;
			}
			else if (non_commnets_processed == 1){
				// Second non-comment line containers the number of entries.
#ifdef SCALA_STRICT
				entries  = std::stoul(entry);
#endif
				non_commnets_processed = non_commnets_processed + 1;
				continue;
			}
#if defined(PLATFORM_WINDOWS) && defined(COMPILER_MINGW)
			else if (Glib::Regex::create (".*[.]+.*")->match (entry)) {
#else
			else if (std::regex_match (entry, std::regex(".*[.]+.*") )) {
#endif
				// Cent values *must* have a period. It's the law.
				double cents = std::stod(entry);
				scala_scale.add_degree(*new degree(cents));
			}
#if defined(PLATFORM_WINDOWS) && defined(COMPILER_MINGW)
			else if (Glib::Regex::create (".*[/]{1}.*")->match (entry)) {
#else
			else if (std::regex_match (entry, std::regex(".*[/]{1}.*") )) {
#endif
				// A ratio
				std::regex_search(entry, match, std::regex("(.*)/(.*)"));
				numerator = std::stoi(match.str(1));
				denominator = std::stoi(match.str(2));
				scala_scale.add_degree(*new degree(numerator, denominator));
#if defined(PLATFORM_WINDOWS) && defined(COMPILER_MINGW)
			} else if (Glib::Regex::create("[0-9]*")->match(entry)) {
#else
			} else if (std::regex_match (entry, std::regex("[0-9]*") )) {
#endif
				// According to the standard, single numbers should be treated as ratios
				numerator = std::stoi(entry);
				denominator = 1;
				scala_scale.add_degree(*new degree(numerator, denominator));
			}
#ifdef SCALA_STRICT
			else {
				// In strict mode we'll make sure to throw an error if the line
				// can't be interpreted.  In lax mode we just give up and move on.
				std::string message = "Scala parse error: cannot interpret: ";
				throw std::runtime_error(message.append(buffer));
			}
#endif
		}
	}

#ifdef SCALA_STRICT
	if (scala_scale.get_scale_length() != entries + 1){
		// If we make is here one of the entries probably didn't parse, but it wasn't
		// such that an error was thrown.  Strict adherence says you should throw an
		// error on all file parse errors.
		throw std::runtime_error("Scala file parse error: Unexpected number of entries");
	}
#endif

	return scala_scale;
}

} // namespace 
