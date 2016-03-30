/* extract doxygen comments from C++ header files
 *
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <map>
#include <clang-c/Index.h>
#include <clang-c/Documentation.h>

struct dox2js {
	dox2js () : clang_argc (3), clang_argv (0), excl_argc (0), excl_argv (0)
	{
		excl_argv = (char**) calloc (1, sizeof (char*));
		clang_argv = (char**) malloc (clang_argc * sizeof (char*));
		clang_argv[0] = strdup ("-x");
		clang_argv[1] = strdup ("c++");
		clang_argv[2] = strdup ("-std=c++11");
	}

	~dox2js () {
		for (int i = 0; i < clang_argc; ++i) {
			free (clang_argv[i]);
		}
		for (int i = 0; excl_argv[i]; ++i) {
			free (excl_argv[i]);
		}
		free (clang_argv);
		free (excl_argv);
	}

	void add_clang_arg (const char *a) {
		clang_argv = (char**) realloc (clang_argv, (clang_argc + 1) * sizeof (char*));
		clang_argv[clang_argc++] = strdup (a);
	}

	void add_exclude (const char *a) {
		excl_argv = (char**) realloc (excl_argv, (excl_argc + 2) * sizeof (char*));
		excl_argv[excl_argc++] = strdup (a);
		excl_argv[excl_argc] = NULL;
	}

	int    clang_argc;
	char** clang_argv;
	int    excl_argc;
	char** excl_argv;
	std::map <std::string, std::string> results;
};

static const char*
kind_to_txt (CXCursor cursor)
{
	CXCursorKind kind  = clang_getCursorKind (cursor);
	switch (kind) {
		case CXCursor_StructDecl   : return "Struct";
		case CXCursor_EnumDecl     : return "Enum";
		case CXCursor_UnionDecl    : return "Union";
		case CXCursor_FunctionDecl : return "C Function";
		case CXCursor_VarDecl      : return "Variable";
		case CXCursor_ClassDecl    : return "C++ Class";
		case CXCursor_CXXMethod    : return "C++ Method";
		case CXCursor_Namespace    : return "C++ Namespace";
		case CXCursor_Constructor  : return "C++ Constructor";
		case CXCursor_Destructor   : return "C++ Destructor";
		case CXCursor_FieldDecl    : return "Data Member/Field";
		default: break;
	}
	return "";
}

static std::string
escape_json (const std::string &s)
{
	std::ostringstream o;
	for (auto c = s.cbegin (); c != s.cend (); c++) {
		switch (*c) {
			case '"':  o << "\\\""; break;
			case '\\': o << "\\\\"; break;
			case '\n': o << "\\n"; break;
			case '\r': o << "\\r"; break;
			case '\t': o << "\\t"; break;
			default:
				if ('\x00' <= *c && *c <= '\x1f') {
					o << "\\u" << std::hex << std::setw (4) << std::setfill ('0') << (int)*c;
				} else {
				  o << *c;
				}
		}
	}
	return o.str ();
}

static std::string
recurse_parents (CXCursor cr) {
	std::string rv;
	CXCursor pc = clang_getCursorSemanticParent (cr);
	if (CXCursor_TranslationUnit == clang_getCursorKind (pc)) {
		return rv;
	}
	if (!clang_Cursor_isNull (pc)) {
		rv += recurse_parents (pc);
		rv += clang_getCString (clang_getCursorDisplayName (pc));
		rv += "::";
	}
	return rv;
}

static bool
check_excludes (const std::string& decl, CXClientData d) {
	struct dox2js* dj = (struct dox2js*) d;
	char** excl = dj->excl_argv;
	for (int i = 0; excl[i]; ++i) {
		if (decl.compare (0, strlen (excl[i]), excl[i]) == 0) {
			return true;
		}
	}
	return false;
}

static enum CXChildVisitResult
traverse (CXCursor cr, CXCursor /*parent*/, CXClientData d)
{
	struct dox2js* dj = (struct dox2js*) d;
	CXComment c = clang_Cursor_getParsedComment (cr);

	if (clang_Comment_getKind (c) != CXComment_Null
			&& clang_isDeclaration (clang_getCursorKind (cr))
			&& 0 != strlen (kind_to_txt (cr))
		 ) {

		// TODO: resolve typedef enum { .. } name;
		// use clang_getCursorDefinition (clang_getCanonicalCursor (cr)) ??
		std::string decl = recurse_parents (cr);
		decl += clang_getCString (clang_getCursorDisplayName (cr));

		if (decl.empty () || check_excludes (decl, d)) {
			return CXChildVisit_Recurse;
		}

		std::ostringstream o;
		o << "{ \"decl\" : \"" << decl << "\",\n";

		if (clang_Cursor_isVariadic (cr)) {
			o << "  \"variadic\" : true,\n";
		}

		CXSourceLocation  loc = clang_getCursorLocation (cr);
		CXFile file; unsigned line, col, off;
		clang_getFileLocation (loc, &file, &line, &col, &off);

		o << "  \"kind\" : \"" << kind_to_txt (cr) << "\",\n"
			<< "  \"src\" : \"" << clang_getCString (clang_getFileName (file)) << ":" << line << "\",\n"
			<< "  \"doc\" : \"" << escape_json (clang_getCString (clang_FullComment_getAsHTML (c))) << "\"\n"
			<< "},\n";

		dj->results[decl] = o.str ();
	}
	return CXChildVisit_Recurse;
}

static void
process_file (const char* fn, struct dox2js *dj, bool check)
{
	if (check) {
		fprintf (stderr, "--- %s ---\n", fn);
	}
	CXIndex index = clang_createIndex (0, check ? 1 : 0);
	CXTranslationUnit tu = clang_createTranslationUnitFromSourceFile (index, fn, dj->clang_argc, dj->clang_argv, 0, 0);

	if (tu == NULL) {
		fprintf (stderr, "Cannot create translation unit for src: %s\n", fn);
		return;
	}

	clang_visitChildren (clang_getTranslationUnitCursor (tu), traverse, (CXClientData) dj);

	clang_disposeTranslationUnit (tu);
	clang_disposeIndex (index);
}

static void
usage (int status)
{
	printf ("doxy2json - extract doxygen doc from C++ headers.\n\n");
	fprintf (stderr, "Usage: dox2json [-I path]* [-X exclude]* <filename> [filename]*\n");
	exit (status);
}

int main (int argc, char** argv)
{
	struct dox2js dj;

	bool report_progress = false;
	bool check_compile = false;
  int c;
	while (EOF != (c = getopt (argc, argv, "D:I:TX:"))) {
		switch (c) {
			case 'I':
				dj.add_clang_arg ("-I");
				dj.add_clang_arg (optarg);
				break;
			case 'D':
				dj.add_clang_arg ("-D");
				dj.add_clang_arg (optarg);
				break;
			case 'X':
				dj.add_exclude (optarg);
				break;
			case 'T':
				check_compile = true;
				break;
			case 'h':
				usage (0);
			default:
				usage (EXIT_FAILURE);
				break;
		}
	}

	if (optind >= argc) {
		usage (EXIT_FAILURE);
	}

	const int total = (argc - optind);
	if (total > 6 && !check_compile) {
		report_progress = true;
	}

	for (int i = optind; i < argc; ++i) {
		process_file (argv[i], &dj, check_compile);
		if (report_progress) {
			fprintf (stderr, "progress: %4.1f%%  [%4d / %4d] decl: %ld         \r",
					100.f * (1.f + i - optind) / (float)total, i - optind, total,
					dj.results.size ());
			fflush (stderr);
		}
	}

	if (!check_compile) {
		printf ("[\n");
		for (std::map <std::string, std::string>::const_iterator i = dj.results.begin (); i != dj.results.end (); ++i) {
			printf ("%s\n", (*i).second.c_str ());
		}
		printf ("{} ]\n");
	}

  return 0;
}
