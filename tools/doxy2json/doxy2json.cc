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
#include <clang-c/Index.h>
#include <clang-c/Documentation.h>

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

static void recurse_parents (CXCursor cr) {
	CXCursor pc = clang_getCursorSemanticParent (cr);
	if (CXCursor_TranslationUnit == clang_getCursorKind (pc)) {
		return;
	}
	if (!clang_Cursor_isNull (pc)) {
		recurse_parents (pc);
		printf ("%s::", clang_getCString (clang_getCursorDisplayName (pc)));
	}
}

static enum CXChildVisitResult
traverse (CXCursor cr, CXCursor /*parent*/, CXClientData)
{
	CXComment c = clang_Cursor_getParsedComment (cr);

	if (clang_Comment_getKind (c) != CXComment_Null
			&& clang_isDeclaration (clang_getCursorKind (cr))
			&& 0 != strlen (kind_to_txt (cr))
		 ) {

		printf ("{ \"decl\" : \"");
		recurse_parents (cr);

		// TODO: resolve typedef enum { .. } name;
		// use clang_getCursorDefinition (clang_getCanonicalCursor (cr)) ??
		printf ("%s\",\n", clang_getCString (clang_getCursorDisplayName (cr)));

		if (clang_Cursor_isVariadic (cr)) {
			printf ("  \"variadic\" : true,\n");
		}

		printf ("  \"kind\" : \"%s\",\n", kind_to_txt (cr));

		CXSourceLocation  loc = clang_getCursorLocation (cr);
		CXFile file; unsigned line, col, off;
		clang_getFileLocation (loc, &file, &line, &col, &off);

		printf ("  \"src\" : \"%s:%d\",\n",
				clang_getCString (clang_getFileName (file)), line);

		printf ("  \"doc\" : \"%s\"\n",
				escape_json (clang_getCString (clang_FullComment_getAsHTML (c))).c_str ());
		printf ("},\n");
	}
	return CXChildVisit_Recurse;
}

static void
process_file (int argc, char **args, const char *fn)
{
	CXIndex index = clang_createIndex (0, 0);
	CXTranslationUnit tu = clang_createTranslationUnitFromSourceFile (index, fn, argc, args, 0, 0);

	if (tu == NULL) {
		fprintf (stderr, "Cannot create translation unit for src: %s\n", fn);
		return;
	}

	clang_visitChildren (clang_getTranslationUnitCursor (tu), traverse, 0);

	clang_disposeTranslationUnit (tu);
	clang_disposeIndex (index);
}

static void
usage (int status)
{
	printf ("doxy2json - extract doxygen doc from C++ headers.\n\n");
	fprintf (stderr, "Usage: dox2json [-I path]* <filename> [filename]*\n");
	exit (status);
}

int main (int argc, char **argv)
{
	int cnt = 2;
	char **args = (char**) malloc (cnt * sizeof (char*));
	args[0] = strdup ("-x");
	args[1] = strdup ("c++");
  int c;
	while (EOF != (c = getopt (argc, argv, "I:"))) {
		switch (c) {
			case 'I':
				args = (char**) realloc (args, (cnt + 2) * sizeof (char*));
				args[cnt++] = strdup ("-I");
				args[cnt++] = strdup (optarg);
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

	printf ("[\n");
	for (int i = optind; i < argc; ++i) {
		process_file (cnt, args, argv[i]);
	}
	printf ("{} ]\n");

	for (int i = 0; i < cnt; ++i) {
		free (args[i]);
	}
	free (args);

  return 0;
}
