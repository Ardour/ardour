#!/usr/bin/env python3

import argparse
import os
import re
import sys

semicolon = ";"
title = "Ardour Shortcuts"
in_group_def = False
group_name = ""
group_text = ""
group_key = ""
group_number = 0
group_names = {}
group_texts = {}
owner_bindings = {}
group_owners = {}
group_bindings = {}
modifier_map = {}
group_numbering = {}
merge_bindings = {}
section_text = {}

platform = "linux"
winkey = "Win"
make_cheatsheet = 0
make_accelmap = 1
merge_from = ""
html = 0

parser = argparse.ArgumentParser()
parser.add_argument("--platform", default=platform)
parser.add_argument("--winkey", default=winkey)
parser.add_argument("--cheatsheet", type=int, default=make_cheatsheet)
parser.add_argument("--accelmap", type=int, default=make_accelmap)
parser.add_argument("--merge", default=merge_from)
parser.add_argument("--html", type=int, default=html)
parser.add_argument("input")
args = parser.parse_args()

platform = args.platform
winkey = args.winkey
make_cheatsheet = args.cheatsheet
make_accelmap = args.accelmap
merge_from = args.merge
html = args.html

if platform == "darwin":
    gtk_modifier_map = {
        'PRIMARY': 'Primary',   # GTK supports Primary to allow platform-independent binding to the "primary" modifier, which on OS X is Command
        'SECONDARY': 'Control',
        'TERTIARY': 'Shift',
        'LEVEL4': 'Mod1',
    }

    # cs_modifier_map == "Cheat Sheet Modifier Map"
    # Used to control what gets shown in the
    # cheat sheet for a given (meta)-modifier

    cs_modifier_map = {
        'PRIMARY': 'Cmd',
        'SECONDARY': 'Ctrl',
        'TERTIARY': 'Shift',
        'LEVEL4': 'Opt',
    }

    # Used to display what gets shown in the
    # cheat sheet for mouse bindings. Differs
    # from cs_modifier map in using shorter
    # abbreviations.

    mouse_modifier_map = {
        'PRIMARY': 'Cmd',
        'SECONDARY': 'Ctrl',
        'TERTIARY': 'Shift',
        'LEVEL4': 'Opt',
    }
else:
    gtk_modifier_map = {
        'PRIMARY': 'Control',
        'SECONDARY': 'Alt',
        'TERTIARY': 'Shift',
        'LEVEL4': winkey,    # Something like "Mod4><Super"
    }

    cs_modifier_map = {
        'PRIMARY': 'Control',
        'SECONDARY': 'Alt',
        'TERTIARY': 'Shift',
        'LEVEL4': 'Win',
    }

    mouse_modifier_map = {
        'PRIMARY': 'Ctl',
        'SECONDARY': 'Alt',
        'TERTIARY': 'Shift',
        'LEVEL4': 'Win',
    }

html_modifier_map = {
    'PRIMARY': '1',
    'SECONDARY': '2',
    'TERTIARY': '3',
    'LEVEL4': '4',
}

keycodes = {}

if html:
    keycodes = {
        'asciicircum': '^',
        'apostrophe': "'",
        'bracketleft': '[',
        'bracketright': ']',
        'braceleft': '{',
        'braceright': '}',
        'backslash': "\\",
        'slash': '/',
        'rightanglebracket': '&gt;',
        'leftanglebracket': '&lt;',
        'ampersand': '&',
        'comma': ',',
        'period': '.',
        'semicolon': ';',
        'colon': ':',
        'equal': '=',
        'minus': '-',
        'plus': '+',
        'grave': '`',
        'rightarrow': '&rarr;',
        'leftarrow': '&larr;',
        'uparrow': '&uarr;',
        'downarrow': '&darr;',
        'Page_Down': 'PageDown',
        'Page_Up': 'PageUp',
        'space': 'space',
        'KP_Right': 'KP-&rarr;',
        'KP_Left': 'KP-&larr;',
        'KP_Up': 'KP-&uarr;',
        'KP_Down': 'KP-&darr;',
        'KP_0': 'KP-0;',
        'greater': '&gt;',
        'less': '&lt;',
        'ISO_Left_Tab': 'Tab',
    }
else:
    keycodes = {
        'asciicircum': '\\verb=^=',
        'apostrophe': "'",
        'bracketleft': '[',
        'bracketright': ']',
        'braceleft': '\\{',
        'braceright': '\\}',
        'backslash': '$\\backslash$',
        'slash': '/',
        'rightanglebracket': '>',
        'leftanglebracket': '<',
        'ampersand': '\\&',
        'comma': ',',
        'period': '.',
        'semicolon': ';',
        'colon': ':',
        'equal': '=',
        'minus': '-',
        'plus': '+',
        'grave': '`',
        'rightarrow': '$\\rightarrow$',
        'leftarrow': '$\\leftarrow$',
        'uparrow': '$\\uparrow$',
        'downarrow': '$\\downarrow$',
        'Page_Down': 'Page Down',
        'Page_Up': 'Page Up',
        'space': 'space',
        'KP_': 'KP$_$',
        'greater': '>',
        'less': '<',
    }

if merge_from:
    try:
        with open(merge_from) as f:
            for line in f:
                if re.match(r'^' + semicolon, line):
                    continue
                if line.startswith('(gtk_accel'):
                    line = line.rstrip('\n').rstrip(')')
                    line = line.replace('"', '')
                    parts = line.split()
                    _, action, binding = parts
                    merge_bindings[action] = binding
    except IOError:
        sys.exit("merge from bindings: file not readable")

if make_accelmap and not merge_from:
    print('<?xml version="1.0" encoding="UTF-8"?>')

bindings_name = os.path.basename(args.input)
bindings_name = re.sub(r'\.bindings\.in$', '', bindings_name)

with open(args.input) as source:
    for line in source:
        if re.match(r'^' + semicolon, line):
            continue

        if re.match(r'^\$', line):
            line = re.sub(r'^\$', '', line)
            title = line.rstrip('\n')
            continue

        if re.match(r'^%', line):
            if in_group_def:
                group_text = group_text.rstrip('\n')
                group_names[group_key] = group_name
                group_texts[group_key] = group_text
                group_numbering[group_key] = group_number
                group_bindings[group_key] = [[]]

            line = re.sub(r'^%', '', line)
            line = line.rstrip('\n')
            group_key, owner, group_name = re.split(r'\s+', line, maxsplit=2)
            if make_accelmap:
                if owner not in owner_bindings:
                    owner_bindings[owner] = [[]]
                group_owners[group_key] = owner
            group_number += 1
            group_text = ""
            in_group_def = True
            continue

        if in_group_def:
            if re.match(r'^@', line):
                group_text = group_text.rstrip('\n')
                group_names[group_key] = group_name
                group_texts[group_key] = group_text
                in_group_def = False
            else:
                if re.match(r'^[ \t]+$', line):
                    continue
                group_text += line
                continue

        if re.match(r'^@', line):
            line = re.sub(r'^@', '', line)
            line = line.rstrip('\n')
            key, action, binding, text = re.split(r'\|', line, maxsplit=3)

            # Do not include "alt-" or "alternate-" actions in the HTML output
            if html and re.search(r'/alt', action):
                continue

            gkey = key
            gkey = re.sub(r'^-', '', gkey)
            owner = group_owners.get(gkey)

            # Strip white space from the binding; this allows reformatting the input file for legibility
            binding = re.sub(r'^\s+|\s+$', '', binding)

            # Substitute bindings

            gtk_binding = binding

            if merge_from:
                lookup = "<Actions>/" + action
                if lookup in merge_bindings:
                    binding = merge_bindings[lookup]
                else:
                    if re.match(r'^\+', key):
                        # Forced inclusion of bindings from template
                        pass
                    else:
                        # This action is not defined in the merge from set, so forget it
                        continue

            # Store the accelmap output
            if re.match(r'^\+', key):
                # Remove + and don't print it in the accelmap
                key = re.sub(r'^\+', '', key)
            else:
                # Include this in the accelmap if it is part of a group that has an "owner"
                if not merge_from and make_accelmap and owner is not None and owner in owner_bindings:
                    b = binding
                    b = re.sub(r'<@', '', b)
                    b = re.sub(r'@>', '', b)
                    b = re.sub(r'PRIMARY', 'Primary-', b)
                    b = re.sub(r'SECONDARY', 'Secondary-', b)
                    b = re.sub(r'TERTIARY', 'Tertiary-', b)
                    b = re.sub(r'LEVEL4', 'Level4-', b)

                    g = group_names.get(gkey, '')
                    g = re.sub(r'\\&', '&amp;', g)

                    bref = owner_bindings[owner]
                    bref.append([action, b, g])

            if re.match(r'^-', key):
                # Do not include this binding in the cheat sheet
                continue

            bref = group_bindings.setdefault(key, [])
            bref.append([binding, text])

            sref = section_text.setdefault(key, [])
            sref.append([owner])

            continue

        continue

if make_accelmap:
    print(f'<BindingSet name="{bindings_name}">')

    for owner in sorted(owner_bindings):
        print(f' <Bindings name="{owner}">\n  <Press>')
        bindings = owner_bindings[owner]
        bindings.pop(0)  # Remove initial empty element
        for binding in bindings:
            print(f'   <Binding key="{binding[1]}" action="{binding[0]}" group="{binding[2]}"/>')
        print('  </Press>\n </Bindings>')

    # Merge in the "fixed" bindings that are not defined by the argument given to this program.
    # This covers things like the step editor, monitor and processor box bindings.

    for hardcoded_bindings in ("mixer.bindings", "step_editing.bindings", "monitor.bindings", "processor_box.bindings", "trigger.bindings", "regionfx_box.bindings", "automation.bindings"):
        path = os.path.join(os.path.dirname(args.input), hardcoded_bindings)
        with open(path) as hardcoded:
            for hline in hardcoded:
                print(hline, end='')

    print('</BindingSet>')

if (make_accelmap or not make_cheatsheet) and not html:
    sys.exit(0)

if html:
    groups_sorted_by_number = sorted(group_numbering, key=lambda g: group_numbering[g])

    for gk in groups_sorted_by_number:

        if re.match(r'^m', gk):
            # mouse stuff - ignore
            continue

        # bref is a reference to the array of arrays for this group.
        bref = group_bindings.get(gk)

        if bref and len(bref) > 1:
            name = group_names.get(gk, '')
            name = re.sub(r'\\linebreak.*', '', name)
            name = re.sub(r'\\&', '&', name)
            name = re.sub(r'\$\\_\$', '-', name)
            name = re.sub(r'\\[a-z]+ ', '', name)
            name = re.sub(r'[{}]', '', name)
            name = re.sub(r'\\par', '', name)

            print(f'<h2>{name}</h2>')

            gtext = group_texts.get(gk, '')
            gtext = re.sub(r'\\linebreak.*', '', gtext)
            gtext = re.sub(r'\\&', '&', gtext)
            gtext = re.sub(r'\$\\_\$', '-', gtext)
            gtext = re.sub(r'\\[a-z]+ ', '', gtext)
            gtext = re.sub(r'[{}]', '', gtext)
            gtext = re.sub(r'\\par', '', gtext)

            if gtext != '':
                print(f'{gtext}\n')

            # Ignore the first entry, which was empty
            bref.pop(0)

            # Set up the list
            print('<table class="dl">')

            # Sort the array of arrays by the descriptive text for nicer appearance,
            # and print them
            bref.sort(key=lambda b: b[1])

            for bbref in bref:
                # bbref is a reference to an array
                binding = bbref[0]
                text = bbref[1]

                if re.search(r':', binding):  # mouse binding with "where" clause
                    binding, where = re.split(r':', binding, 1)

                for k in cs_modifier_map:
                    binding = re.sub(r'@' + k + r'@', html_modifier_map[k], binding)

                # Remove braces for HTML

                binding = re.sub(r'><', '', binding)
                binding = re.sub(r'^<', '', binding)
                binding = re.sub(r'>', '+', binding)

                # Substitute keycode names for something printable

                keycode_pattern = '|'.join(re.escape(k) for k in keycodes)
                binding = re.sub(r'(' + keycode_pattern + r')', lambda m: keycodes[m.group(1)], binding)

                # Tidy up description

                descr = bbref[1]
                descr = re.sub(r'\\linebreak.*', '', descr)
                descr = re.sub(r'\\&', '&', descr)
                descr = re.sub(r'\$\\_\$', '-', descr)
                descr = re.sub(r'\\[a-z]+ ', '', descr)
                descr = re.sub(r'[{}]', '', descr)
                descr = re.sub(r'\\par', '', descr)

                if re.search(r'\+', binding):
                    mods, k = re.split(r'\+', binding, 1)
                    mods = f'mod{mods}'
                else:
                    mods = ''
                    k = binding

                print(f'<tr><th>{descr}</th><td><kbd class="{mods}">{k}</kbd></td></tr>')

            print('</table>')

    print('&nbsp; <!-- remove this if more text is added below -->')
    sys.exit(0)

# Now print the cheatsheet

boilerplate_header = r"""\documentclass[10pt,landscape]{article}
%\documentclass[10pt,landscape,a4paper]{article}
%\documentclass[10pt,landscape,letterpaper]{article}
\usepackage{multicol}
\usepackage{calc}
\usepackage{ifthen}
\usepackage{palatino}
\usepackage{geometry}

\setlength{\parskip}{0pt}
\setlength{\parsep}{0pt}
\setlength{\headsep}{0pt}
\setlength{\topskip}{0pt}
\setlength{\topmargin}{0pt}
\setlength{\topsep}{0pt}
\setlength{\partopsep}{0pt}

% This sets page margins to .5 inch if using letter paper, and to 1cm
% if using A4 paper. (This probably isnott strictly necessary.)
% If using another size paper, use default 1cm margins.
\ifthenelse{\lengthtest { \paperwidth = 11in}}
	{ \geometry{top=.5in,left=.5in,right=.5in,bottom=.5in} }
	{\ifthenelse{ \lengthtest{ \paperwidth = 297mm}}
		{\geometry{top=1cm,left=1cm,right=1cm,bottom=1cm} }
		{\geometry{top=1cm,left=1cm,right=1cm,bottom=1cm} }
	}

% Turn off header and footer
\pagestyle{empty}

% Redefine section commands to use less space
\makeatletter
\renewcommand{\section}{\@startsection{section}{1}{0mm}%
                                {-1ex plus -.5ex minus -.2ex}%
                                {0.5ex plus .2ex}%
                                {\normalfont\large\bfseries}}
\renewcommand{\subsection}{\@startsection{subsection}{2}{0mm}%
                                {-1explus -.5ex minus -.2ex}%
                                {0.5ex plus .2ex}%
                                {\normalfont\normalsize\bfseries}}
\renewcommand{\subsubsection}{\@startsection{subsubsection}{3}{0mm}%
                                {-1ex plus -.5ex minus -.2ex}%
                                {1ex plus .2ex}%
                                {\normalfont\small\bfseries}}
\makeatother

% Do not print section numbers% Do not print section numbers
\setcounter{secnumdepth}{0}

\setlength{\parindent}{0pt}
\setlength{\parskip}{0pt plus 0.5ex}

%-------------------------------------------

\begin{document}
\newlength{\MyLen}
\raggedright
\footnotesize
\begin{multicols}{3}
"""

boilerplate_footer = r"""\rule{0.3\linewidth}{0.25pt}
\scriptsize

Copyright \copyright\ 2013 ardour.org

% Should change this to be date of file, not current date.

http://manual.ardour.org

\end{multicols}
\end{document}
"""

if make_cheatsheet:
    print(boilerplate_header)
    print(f'\\begin{{center}}\\Large\\bf {title} \\end{{center}}')

groups_sorted_by_number = sorted(group_numbering, key=lambda g: group_numbering[g])

for gk in groups_sorted_by_number:
    # bref is a reference to the array of arrays for this group
    bref = group_bindings.get(gk)

    if bref and len(bref) > 1:
        print(f'\\section{{{group_names.get(gk, "")}}}')

        if group_texts.get(gk, '') != '':
            print(f'{group_texts[gk]}\n\\par')

        # Ignore the first entry, which was empty
        bref.pop(0)

        # Find the longest descriptive text (this is not 100% accuracy due to typography)

        maxtextlen = 0
        maxtext = ""

        for bbref in bref:
            # $bbref is a reference to an array
            text = bbref[1]

            #
            # If there is a linebreak, just use everything up the linebreak
            # to determine the width.
            #

            if re.search(r'\\linebreak', text):
                matchtext = re.sub(r'\\linebreak.*', '', text)
            else:
                matchtext = text
            if len(matchtext) > maxtextlen:
                maxtextlen = len(matchtext)
                maxtext = matchtext

        if re.match(r'^m', gk):
            # Mouse mode: Don't extend max text at all - space it tight
            maxtext += '.'
        else:
            maxtext += '....'

        # Set up the table
        print(f'\\settowidth{{\\MyLen}}{{\\texttt{{{maxtext}}}}}')
        print('\\begin{tabular}{@{}p{\\the\\MyLen}%'
              ' @{}p{\\linewidth-\\the\\MyLen}%'
              ' @{}}')

        # Sort the array of arrays by the descriptive text for nicer appearance,
        # and print them

        bref.sort(key=lambda b: b[1])

        for bbref in bref:
            # bbref is a reference to an array
            binding = bbref[0]
            text = bbref[1]

            where = ''
            if re.search(r':', binding):  # Mouse binding with "where" clause
                binding, where = re.split(r':', binding, 1)

            if re.match(r'^m', gk):
                # Mouse mode - Use shorter abbrevs
                for k in mouse_modifier_map:
                    binding = re.sub(r'@' + k + r'@', mouse_modifier_map[k], binding)
            else:
                for k in cs_modifier_map:
                    binding = re.sub(r'@' + k + r'@', cs_modifier_map[k], binding)

            binding = re.sub(r'><', '+', binding)
            binding = re.sub(r'^<', '', binding)
            binding = re.sub(r'>', '+', binding)

            # Substitute keycode names for something printable

            keycode_pattern = '|'.join(re.escape(k) for k in keycodes)
            binding = re.sub(r'(' + keycode_pattern + r')', lambda m: keycodes[m.group(1)], binding)

            # Split up mouse bindings to "click" and "where" parts
            if gk == "mobject":
                print(f'{{\\tt {bbref[1]} }} & {{\\tt {binding}}} {{\\it {where}}}\\\\')
            else:
                print(f'{{\\tt {bbref[1]} }} & {{\\tt {binding}}} \\\\')

        print('\\end{tabular}')

print(boilerplate_footer)
