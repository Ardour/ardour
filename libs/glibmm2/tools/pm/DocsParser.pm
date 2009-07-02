# gtkmm - DocsParser module
#
# Copyright 2001 Free Software Foundation
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
#

# Based on XML::Parser tutorial found at http://www.devshed.com/Server_Side/Perl/PerlXML/PerlXML1/page1.html
# This module isn't properly Object Orientated because the XML Parser needs global callbacks.

package DocsParser;
use XML::Parser;
use strict;
use warnings;

# use Util;
use Function;
use GtkDefs;
use Object;

BEGIN {
     use Exporter   ();
     our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);

     # set the version for version checking
     $VERSION     = 1.00;

     @ISA         = qw(Exporter);
     @EXPORT      = ( );
     %EXPORT_TAGS = ( );

     # your exported package globals go here,
     # as well as any optionally exported functions
     @EXPORT_OK   = ( );
}
our @EXPORT_OK;

#####################################

use strict;
use warnings;

#####################################

$DocsParser::CurrentFile = "";

$DocsParser::refAppendTo = undef; # string reference to store the data into
$DocsParser::currentParam = undef;

$DocsParser::objCurrentFunction = undef; #Function
%DocsParser::hasharrayFunctions = (); #Function elements
#~ $DocsParser::bOverride = 0; #First we parse the C docs, then we parse the C++ override docs.

$DocsParser::commentStart = "  /** ";
$DocsParser::commentMiddleStart = "   * ";
$DocsParser::commentEnd = "   */";

sub read_defs($$$)
{
  my ($path, $filename, $filename_override) = @_;
  
  my $objParser = new XML::Parser(ErrorContext => 0);
  $objParser->setHandlers(Start => \&parse_on_start, End => \&parse_on_end, Char => \&parse_on_cdata);
  
  # C documentation:
  $DocsParser::CurrentFile = "$path/$filename";
  if ( ! -r $DocsParser::CurrentFile)
  {
     print "DocsParser.pm: Warning: Can't read file \"" . $DocsParser::CurrentFile . "\".\n";
     return;
  }
  # Parse
  eval { $objParser->parsefile($DocsParser::CurrentFile) };
  if( $@ )
  {
    $@ =~ s/at \/.*?$//s;
    print "\nError in \"" . $DocsParser::CurrentFile . "\":$@\n";
    return;
  }

  # C++ overide documentation:
  $DocsParser::CurrentFile = "$path/$filename_override";
  if ( ! -r $DocsParser::CurrentFile)
  {
     print "DocsParser.pm: Warning: Can't read file \"" . $DocsParser::CurrentFile . "\".\n";
     return;
  }
  # Parse
  eval { $objParser->parsefile($DocsParser::CurrentFile) };
  if( $@ )
  {
    $@ =~ s/at \/.*?$//s;
    print "\nError in \"" . $DocsParser::CurrentFile . "\":$@";
    return;
  }
}

sub parse_on_start($$%)
{
  my ($objParser, $tag, %attr) = @_;

  $tag = lc($tag);

  if($tag eq "function")
  {
    if(defined $DocsParser::objCurrentFunction)
    {
      $objParser->xpcroak("\nClose a function tag before you open another one.");
    }
    
    my $functionName = $attr{name};

    #Reuse existing Function, if it exists:
    #(For instance, if this is the override parse)
    $DocsParser::objCurrentFunction = $DocsParser::hasharrayFunctions{$functionName};
    if(!$DocsParser::objCurrentFunction)
    {
      #Make a new one if necessary:
      $DocsParser::objCurrentFunction = Function::new_empty();
      # The idea is to change the policy a bit:
      # If a function is redefined in a later parsing run only values which are redefined
      # will be overwritten. For the name this is trivial. The description is simply rewritten.
      # Same goes for the return description and the class mapping. Only exception is the
      # parameter list. Everytime we enter a <parameters> tag the list is emptied again.
      $$DocsParser::objCurrentFunction{name} = $functionName;
      $$DocsParser::objCurrentFunction{description} = "";
      $$DocsParser::objCurrentFunction{param_names} = [];
      $$DocsParser::objCurrentFunction{param_descriptions} = ();
      $$DocsParser::objCurrentFunction{return_description} = "";
      $$DocsParser::objCurrentFunction{mapped_class} = "";
      # We don't need this any more, the only reference to this field is commented
      # $$DocsParser::objCurrentFunction{description_overridden} = $DocsParser::bOverride;
    }
  }
  elsif($tag eq "parameters")
  {
    $$DocsParser::objCurrentFunction{param_names} = [];
    $$DocsParser::objCurrentFunction{param_descriptions} = ();
  }
  elsif($tag eq "parameter")
  {
    $DocsParser::currentParam = $attr{name};
    $$DocsParser::objCurrentFunction{param_descriptions}->{$DocsParser::currentParam} = "";
  }
  elsif($tag eq "description")
  {
    $$DocsParser::objCurrentFunction{description} = "";
    # Set destination for parse_on_cdata().
    $DocsParser::refAppendTo = \$$DocsParser::objCurrentFunction{description};
  }
  elsif($tag eq "parameter_description")
  {
    # Set destination for parse_on_cdata().
    my $param_desc = \$$DocsParser::objCurrentFunction{param_descriptions};
    $DocsParser::refAppendTo = \$$param_desc->{$DocsParser::currentParam};
  }
  elsif($tag eq "return")
  {
    $$DocsParser::objCurrentFunction{return_description} = "";
    # Set destination for parse_on_cdata().
    $DocsParser::refAppendTo = \$$DocsParser::objCurrentFunction{return_description};
  }
  elsif($tag eq "mapping")
  {
    $$DocsParser::objCurrentFunction{mapped_class} = $attr{class};
  }
  elsif($tag ne "root")
  {
    $objParser->xpcroak("\nUnknown tag \"$tag\".");
  }
}


sub parse_on_end($$)
{
  my ($parser, $tag) = @_;

  # Clear destination for parse_on_cdata().
  $DocsParser::refAppendTo = undef;

  $tag = lc($tag);

  if($tag eq "function")
  {
    # Store the Function structure in the array:
    my $functionName = $$DocsParser::objCurrentFunction{name};
    $DocsParser::hasharrayFunctions{$functionName} = $DocsParser::objCurrentFunction;
    $DocsParser::objCurrentFunction = undef;
  }
  elsif($tag eq "parameter")
  {
    # <parameter name="returns"> and <return> means the same.
    if($DocsParser::currentParam eq "returns")
    {
      my $param_descriptions = \$$DocsParser::objCurrentFunction{param_descriptions};
      my $return_description = \$$DocsParser::objCurrentFunction{return_description};
      $$return_description = delete $$param_descriptions->{"returns"};
    }
    else
    {
      # Append to list of parameters.
      push(@{$$DocsParser::objCurrentFunction{param_names}}, $DocsParser::currentParam);
    }

    $DocsParser::currentParam = undef;
  }
}


sub parse_on_cdata($$)
{
  my ($parser, $data) = @_;

  if(defined $DocsParser::refAppendTo)
  {
    # Dispatch $data to the current destination string.
    $$DocsParser::refAppendTo .= $data;
  }
}


# $strCommentBlock lookup_documentation($strFunctionName)
sub lookup_documentation($$)
{
  my ($functionName, $deprecation_docs) = @_;

  my $objFunction = $DocsParser::hasharrayFunctions{$functionName};
  if(!$objFunction)
  {
    #print "DocsParser.pm: Warning: function not found: $functionName\n";
    return ""
  }

  my $text = $$objFunction{description};

  if(length($text) eq 0)
  {
    print "DocsParser.pm: Warning: No C docs for function: \"$functionName\"\n";
  }


  DocsParser::convert_docs_to_cpp($objFunction, \$text);

  #Add note about deprecation if we have specified that in our _WRAP_METHOD() call:
  if($deprecation_docs ne "")
  {
    $text .= "\n\@deprecated $deprecation_docs";
  }

  DocsParser::append_parameter_docs($objFunction, \$text);
  DocsParser::append_return_docs($objFunction, \$text);


  # Escape the space after "i.e." or "e.g." in the brief description.
  $text =~ s/^([^.]*\b(?:i\.e\.|e\.g\.))\s/$1\\ /;

  # Convert to Doxygen-style comment.
  $text =~ s/\n/\n${DocsParser::commentMiddleStart}/g;
  $text =  $DocsParser::commentStart . $text;
  $text .= "\n${DocsParser::commentEnd}\n";

  return $text;
}


sub append_parameter_docs($$)
{
  my ($obj_function, $text) = @_;

  my @param_names = @{$$obj_function{param_names}};
  my $param_descriptions = \$$obj_function{param_descriptions};

  # Strip first parameter if this is a method.
  my $defs_method = GtkDefs::lookup_method_dont_mark($$obj_function{name});
  # the second alternative is for use with method-mappings meaning:
  # this function is mapped into this Gtk::class
  shift(@param_names) if(($defs_method && $$defs_method{class} ne "") ||
                         ($$obj_function{mapped_class} ne ""));

  foreach my $param (@param_names)
  {
    if ($param ne "error" ) #We wrap GErrors as exceptions, so ignore these.
    {
      my $desc = $$param_descriptions->{$param};
    
      $param =~ s/([a-zA-Z0-9]*(_[a-zA-Z0-9]+)*)_?/$1/g;
      DocsParser::convert_docs_to_cpp($obj_function, \$desc);
      if(length($desc) > 0)
      {
        $desc  .= '.' unless($desc =~ /(?:^|\.)$/);
        $$text .= "\n\@param ${param} \u${desc}";
      }
    }
  }
}


sub append_return_docs($$)
{
  my ($obj_function, $text) = @_;

  my $desc = $$obj_function{return_description};
  DocsParser::convert_docs_to_cpp($obj_function, \$desc);

  $desc  =~ s/\.$//;
  $$text .= "\n\@return \u${desc}." unless($desc eq "");
}


sub convert_docs_to_cpp($$)
{
  my ($obj_function, $text) = @_;

  # Chop off leading and trailing whitespace.
  $$text =~ s/^\s+//;
  $$text =~ s/\s+$//;
# HagenM: this is the only reference to $$obj_function{description_overridden}
# and it seems not to be in use.
#  if(!$$obj_function{description_overridden})
#  {
    # Convert C documentation to C++.
    DocsParser::convert_tags_to_doxygen($text);
    DocsParser::substitute_identifiers($$obj_function{name}, $text);

    $$text =~ s/\bX\s+Window\b/X&nbsp;\%Window/g;
    $$text =~ s/\bWindow\s+manager/\%Window manager/g;
#  }
}


sub convert_tags_to_doxygen($)
{
  my ($text) = @_;

  for($$text)
  {
    # Replace format tags.
    s"&lt;(/?)emphasis&gt;"<$1em>"g;
    s"&lt;(/?)literal&gt;"<$1tt>"g;
    s"&lt;(/?)function&gt;"<$1tt>"g;

    # Some argument names are suffixed by "_" -- strip this.
    # gtk-doc uses @thearg, but doxygen uses @a thearg.
    s" ?\@([a-zA-Z0-9]*(_[a-zA-Z0-9]+)*)_?\b" \@a $1"g;

    # Don't convert doxygen's @throws and @param, so these can be used in the
    # docs_override.xml:
    s" \@a (throws|param)" \@$1"g;
    s"^Note ?\d?: "\@note "mg;

    s"&lt;/?programlisting&gt;""g;
    s"&lt;informalexample&gt;"\@code"g;
    s"&lt;/informalexample&gt;"\@endcode"g;
    s"&lt;!&gt;""g;

    # Remove all link tags.
    s"&lt;/?u?link[^&]*&gt;""g;

    # Remove all para tags (from tmpl sgml files).
    s"&lt;/?para&gt;""g;

    # Use our doxgen since/newin tags:
    # TODO: Do this generically, regardless of the number:
    s"Since: 2\.2"\@newin2p2"mg;
    s"Since: 2\.4"\@newin2p4"mg;
    s"Since: 2\.6"\@newin2p6"mg;
    s"Since: 2\.8"\@newin2p8"mg;
    s"Since: 2\.10"\@newin2p10"mg;
    s"Since: 2\.12"\@newin2p12"mg;
    s"Since: 2\.14"\@newin2p14"mg;
    s"Since: 2\.16"\@newin2p16"mg;
    s"Since: 2\.18"\@newin2p18"mg;

    s"\b-&gt;\b"->"g;

    # Doxygen is too dumb to handle &mdash;
    s"&mdash;" \@htmlonly&mdash;\@endhtmlonly "g;

    s"\%?FALSE\b"<tt>false</tt>"g;
    s"\%?TRUE\b"<tt>true</tt>"g;
    s"\%?NULL\b"<tt>0</tt>"g;

    s"#?\bgboolean\b"<tt>bool</tt>"g;
    s"#?\bg(int|short|long)\b"<tt>$1</tt>"g;
    s"#?\bgu(int|short|long)\b"<tt>unsigned $1</tt>"g;

    # For Gtk::TextIter.
    s"(\\[rn])\b"<tt>\\$1</tt>"g;
  }
}


sub substitute_identifiers($$)
{
  my ($doc_func, $text) = @_;

  for($$text)
  {
    # TODO: handle more than one namespace

    s/[#%]([A-Z][a-z]*)([A-Z][A-Za-z]+)\b/$1::$2/g; # type names

    s/[#%]([A-Z])([A-Z]*)_([A-Z\d_]+)\b/$1\L$2\E::$3/g; # enum values

    # Undo wrong substitutions.
    s/\bHas::/HAS_/g;
    s/\bNo::/NO_/g;
    s/\bG:://g; #Rename G::Something to Something. Doesn't seem to work. murrayc.

    # Replace C function names with C++ counterparts.
    s/\b([a-z]+_[a-z][a-z\d_]+) ?\(\)/&DocsParser::substitute_function($doc_func, $1)/eg;
  }
}


sub substitute_function($$)
{
  my ($doc_func, $name) = @_;

  if(my $defs_method = GtkDefs::lookup_method_dont_mark($name))
  {
    if(my $defs_object = DocsParser::lookup_object_of_method($$defs_method{class}, $name))
    {
      my $module = $$defs_object{module};
      my $class  = $$defs_object{name};

      DocsParser::build_method_name($doc_func, $module, $class, \$name);
    }
  }
  else
  {
    # Not perfect, but better than nothing.
    $name =~ s/^g_/Glib::/;
  }

  return $name . "()";
}


sub lookup_object_of_method($$)
{
  my ($object, $name) = @_;

  if($object ne "")
  {
    # We already know the C object name, because $name is a non-static method.
    return GtkDefs::lookup_object($object);
  }

  my @parts = split(/_/, $name);
  pop(@parts);

  # (gtk, foo, bar) -> (Gtk, Foo, Bar)
  foreach(@parts) { $_ = (length > 2) ? ucfirst : uc; }

  # Do a bit of try'n'error.
  while(scalar(@parts) > 1)
  {
    my $try = join("", @parts);

    if(my $defs_object = GtkDefs::lookup_object($try))
      { return $defs_object; }

    pop(@parts);
  }

  return undef;
}


sub build_method_name($$$$)
{
  my ($doc_func, $module, $class, $name) = @_;

  my $prefix = $module . $class;

  $prefix =~ s/([a-z])([A-Z])/$1_$2/g;
  $prefix = lc($prefix) . '_';

  if($$name =~ /^$prefix/)
  {
    my $scope = "";
    $scope = "${module}::${class}::" unless($doc_func =~ /^$prefix/);

    substr($$name, 0, length($prefix)) = $scope;
  }
}


1; # indicate proper module load.
