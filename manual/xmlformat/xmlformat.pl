#! /usr/bin/perl -w 
# vim:set ts=2 sw=2 expandtab:

# xmlformat - configurable XML file formatter/pretty-printer

# Copyright (c) 2004, 2005 Kitebird, LLC.  All rights reserved.
# Some portions are based on the REX shallow XML parser, which
# is Copyright (c) 1998, Robert D. Cameron. These include the
# regular expression parsing variables and the shallow_parse()
# method.
# This software is licensed as described in the file LICENSE,
# which you should have received as part of this distribution.

# Syntax: xmlformat [config-file] xml-file

# Default config file is $ENV{XMLFORMAT_CONF} or ./xmlformat.conf, in that
# order.

# Paul DuBois
# paul@kitebird.com
# 2003-12-14

# The input document first is parsed into a list of strings.  Each string
# represents one of the following:
# - text node
# - processing instruction (the XML declaration is treated as a PI)
# - comment
# - CDATA section
# - DOCTYPE declaration
# - element tag (either <abc>, </abc>, or <abc/>), *including attributes*

# Entities are left untouched. They appear in their original form as part
# of the text node in which they occur.

# The list of strings then is converted to a hierarchical structure.
# The document top level is represented by a reference to a list.
# Each list element is a reference to a node -- a hash that has "type"
# and "content" key/value pairs. The "type" key indicates the node
# type and has one of the following values:

# "text"    - text node
# "pi"      - processing instruction node
# "comment" - comment node
# "CDATA"   - CDATA section node
# "DOCTYPE" - DOCTYPE node
# "elt"     - element node

# (For purposes of this program, it's really only necessary to have "text",
# "elt", and "other".  The types other than "text" and "elt" currently are
# all treated the same way.)

# For all but element nodes, the "content" value is the text of the node.

# For element nodes, the "content" hash is a reference to a list of
# nodes for the element's children. In addition, an element node has
# three additional key/value pairs:
# - The "name" value is the tag name within the opening tag, minus angle
#   brackets or attributes.
# - The "open_tag" value is the full opening tag, which may also be the
#   closing tag.
# - The "close_tag" value depends on the opening tag.  If the open tag is
#   "<abc>", the close tag is "</abc>". If the open tag is "<abc/>", the
#   close tag is the empty string.

# If the tree structure is converted back into a string with
# tree_stringify(), the result can be compared to the input file
# as a regression test. The string should be identical to the original
# input document.

use strict;

use Getopt::Long;
$Getopt::Long::ignorecase = 0; # options are case sensitive
$Getopt::Long::bundling = 1;   # allow short options to be bundled

my $PROG_NAME = "xmlformat";
my $PROG_VERSION = "1.04";
my $PROG_LANG = "Perl";

# ----------------------------------------------------------------------

package XMLFormat;

use strict;

# ----------------------------------------------------------------------

# Regular expressions for parsing document components. Based on REX.

# SPE = shallow parsing expression
# SE = scanning expression
# CE = completion expression
# RSB = right square brackets
# QM = question mark

my $TextSE = "[^<]+";
my $UntilHyphen = "[^-]*-";
my $Until2Hyphens = "$UntilHyphen(?:[^-]$UntilHyphen)*-";
my $CommentCE = "$Until2Hyphens>?";
my $UntilRSBs = "[^\\]]*\\](?:[^\\]]+\\])*\\]+";
my $CDATA_CE = "$UntilRSBs(?:[^\\]>]$UntilRSBs)*>";
my $S = "[ \\n\\t\\r]+";
my $NameStrt = "[A-Za-z_:]|[^\\x00-\\x7F]";
my $NameChar = "[A-Za-z0-9_:.-]|[^\\x00-\\x7F]";
my $Name = "(?:$NameStrt)(?:$NameChar)*";
my $QuoteSE = "\"[^\"]*\"|'[^']*'";
my $DT_IdentSE = "$S$Name(?:$S(?:$Name|$QuoteSE))*";
my $MarkupDeclCE = "(?:[^\\]\"'><]+|$QuoteSE)*>";
my $S1 = "[\\n\\r\\t ]";
my $UntilQMs = "[^?]*\\?+";
my $PI_Tail = "\\?>|$S1$UntilQMs(?:[^>?]$UntilQMs)*>";
my $DT_ItemSE =
"<(?:!(?:--$Until2Hyphens>|[^-]$MarkupDeclCE)|\\?$Name(?:$PI_Tail))|%$Name;|$S";
my $DocTypeCE = "$DT_IdentSE(?:$S)?(?:\\[(?:$DT_ItemSE)*\\](?:$S)?)?>?";
my $DeclCE =
"--(?:$CommentCE)?|\\[CDATA\\[(?:$CDATA_CE)?|DOCTYPE(?:$DocTypeCE)?";
my $PI_CE = "$Name(?:$PI_Tail)?";
my $EndTagCE = "$Name(?:$S)?>?";
my $AttValSE = "\"[^<\"]*\"|'[^<']*'";
my $ElemTagCE = "$Name(?:$S$Name(?:$S)?=(?:$S)?(?:$AttValSE))*(?:$S)?/?>?";
my $MarkupSPE =
"<(?:!(?:$DeclCE)?|\\?(?:$PI_CE)?|/(?:$EndTagCE)?|(?:$ElemTagCE)?)";
my $XML_SPE = "$TextSE|$MarkupSPE";

# ----------------------------------------------------------------------

# Allowable options and their possible values:
# - The keys of this hash are the allowable option names
# - The value for each key is list of allowable option values
# - If the value is undef, the option value must be numeric
# If any new formatting option is added to this program, it
# must be specified here, *and* a default value for it should
# be listed in the *DOCUMENT and *DEFAULT pseudo-element
# option hashes.

my %opt_list = (
  "format"    => [ "block", "inline", "verbatim" ],
  "normalize"   => [ "yes", "no" ],
  "subindent"   => undef,
  "wrap-length" => undef,
  "entry-break" => undef,
  "exit-break"  => undef,
  "element-break" => undef
);

# Object creation: set up the default formatting configuration
# and variables for maintaining input and output document.

sub new
{
my $type = shift;

  my $self = {};

  # Formatting options for each element.

  $self->{elt_opts} = { };

  # The formatting options for the *DOCUMENT and *DEFAULT pseudo-elements can
  # be overridden in the configuration file, but the options must also be
  # built in to make sure they exist if not specified in the configuration
  # file.  Each of the structures must have a value for every option.

  # Options for top-level document children.
  # - Do not change entry-break: 0 ensures no extra newlines before
  #   first element of output.
  # - Do not change exit-break: 1 ensures a newline after final element
  #   of output document.
  # - It's probably best not to change any of the others, except perhaps
  #   if you want to increase the element-break.

  $self->{elt_opts}->{"*DOCUMENT"} = {
    "format"    => "block",
    "normalize"   => "no",
    "subindent"   => 0,
    "wrap-length" => 0,
    "entry-break" => 0, # do not change
    "exit-break"  => 1, # do not change
    "element-break" => 1
  };

  # Default options. These are used for any elements in the document
  # that are not specified explicitly in the configuration file.

  $self->{elt_opts}->{"*DEFAULT"} = {
    "format"    => "block",
    "normalize"   => "no",
    "subindent"   => 1,
    "wrap-length" => 0,
    "entry-break" => 1,
    "exit-break"  => 1,
    "element-break" => 1
  };

  # Run the *DOCUMENT and *DEFAULT options through the option-checker
  # to verify that the built-in values are legal.

  my $err_count = 0;

  foreach my $elt_name (keys (%{$self->{elt_opts}}))  # ... for each element
  {
    # Check each option for element
    while (my ($opt_name, $opt_val) = each (%{$self->{elt_opts}->{$elt_name}}))
    {
      my $err_msg;

      ($opt_val, $err_msg) = check_option ($opt_name, $opt_val);
      if (!defined ($err_msg))
      {
        $self->{elt_opts}->{$elt_name}->{$opt_name} = $opt_val;
      }
      else
      {
        warn "LOGIC ERROR: $elt_name default option is invalid\n";
        warn "$err_msg\n";
        ++$err_count;
      }
    }
  }

  # Make sure that the every option is represented in the
  # *DOCUMENT and *DEFAULT structures.

  foreach my $opt_name (keys (%opt_list))
  {
    foreach my $elt_name (keys (%{$self->{elt_opts}}))
    {
      if (!exists ($self->{elt_opts}->{$elt_name}->{$opt_name}))
      {
        warn "LOGIC ERROR: $elt_name has no default '$opt_name' option\n";
        ++$err_count;
      }
    }
  }

  die "Cannot continue; internal default formatting options must be fixed\n"
    if $err_count > 0;

  bless $self, $type;    # bless object and return it
}

# Initialize the variables that are used per-document

sub init_doc_vars
{
my $self = shift;

  # Elements that are used in the document but not named explicitly
  # in the configuration file.

  $self->{unconf_elts} = { };

  # List of tokens for current document.

  $self->{tokens} = [ ];

  # List of line numbers for each token

  $self->{line_num} = [ ];

  # Document node tree (constructed from the token list).

  $self->{tree} = [ ];

  # Variables for formatting operations:
  # out_doc = resulting output document (constructed from document tree)
  # pending = array of pending tokens being held until flushed

  $self->{out_doc} = "";
  $self->{pending} = [ ];

  # Inline elements within block elements are processed using the
  # text normalization (and possible line-wrapping) values of their
  # enclosing block. Blocks and inlines may be nested, so we maintain
  # a stack that allows the normalize/wrap-length values of the current
  # block to be determined.

  $self->{block_name_stack} = [ ];  # for debugging
  $self->{block_opts_stack} = [ ];

  # A similar stack for maintaining each block's current break type.

  $self->{block_break_type_stack} = [ ];
}

# Accessors for token list and resulting output document

sub tokens
{
my $self = shift;

  return $self->{tokens};
}

sub out_doc
{
my $self = shift;

  return $self->{out_doc};
}


# Methods for adding strings to output document or
# to the pending output array

sub add_to_doc
{
my ($self, $str) = @_;

  $self->{out_doc} .= $str;
}

sub add_to_pending
{
my ($self, $str) = @_;

  push (@{$self->{pending}}, $str);
}


# Block stack mainenance methods

# Push options onto or pop options off from the stack.  When doing
# this, also push or pop an element onto the break-level stack.

sub begin_block
{
my ($self, $name, $opts) = @_;

  push (@{$self->{block_name_stack}}, $name);
  push (@{$self->{block_opts_stack}}, $opts);
  push (@{$self->{block_break_type_stack}}, "entry-break");
}

sub end_block
{
my $self = shift;

  pop (@{$self->{block_name_stack}});
  pop (@{$self->{block_opts_stack}});
  pop (@{$self->{block_break_type_stack}});
}

# Return the current block's normalization status or wrap length

sub block_normalize
{
my $self = shift;

  my $size = @{$self->{block_opts_stack}};
  my $opts = $self->{block_opts_stack}->[$size-1];
  return $opts->{normalize} eq "yes";
}

sub block_wrap_length
{
my $self = shift;

  my $size = @{$self->{block_opts_stack}};
  my $opts = $self->{block_opts_stack}->[$size-1];
  return $opts->{"wrap-length"};
}

# Set the current block's break type, or return the number of newlines
# for the block's break type

sub set_block_break_type
{
my ($self, $type) = @_;

  my $size = @{$self->{block_break_type_stack}};
  $self->{block_break_type_stack}->[$size-1] = $type;
}

sub block_break_value
{
my $self = shift;

  my $size = @{$self->{block_opts_stack}};
  my $opts = $self->{block_opts_stack}->[$size-1];
  $size = @{$self->{block_break_type_stack}};
  my $type = $self->{block_break_type_stack}->[$size-1];
  return $opts->{$type};
}


# ----------------------------------------------------------------------

# Read configuration information.  For each element, construct a hash
# containing a hash key and value for each option name and value.
# After reading the file, fill in missing option values for
# incomplete option structures using the *DEFAULT options.

sub read_config
{
my $self = shift;
my $conf_file = shift;
my @elt_names = ();
my $err_msg;
my $in_continuation = 0;
my $saved_line = "";

  open (FH, $conf_file) or die "Cannot read config file $conf_file: $!\n";
  while (<FH>)
  {
    chomp;

    next if /^\s*($|#)/;  # skip blank lines, comments
    if ($in_continuation)
    {
      $_ = $saved_line . " " . $_;
      $saved_line = "";
      $in_continuation = 0;
    }
    if (!/^\s/)
    {
      # Line doesn't begin with whitespace, so it lists element names.
      # Names are separated by whitespace or commas, possibly followed
      # by a continuation character or a comment.
      if (/\\$/)
      {
        s/\\$//;                          # remove continuation character
        $saved_line = $_;
        $in_continuation = 1;
        next;
      }
      s/\s*#.*$//;                        # remove any trailing comment
      @elt_names = split (/[\s,]+/, $_);
      # make sure each name has an entry in the elt_opts structure
      foreach my $elt_name (@elt_names)
      {
        $self->{elt_opts}->{$elt_name} = { }
          unless exists ($self->{elt_opts}->{$elt_name});
      }
    }
    else
    {
      # Line begins with whitespace, so it contains an option
      # to apply to the current element list, possibly followed by
      # a comment.  First check that there is a current list.
      # Then parse the option name/value.

      die "$conf_file:$.: Option setting found before any "
          . "elements were named.\n"
        if !@elt_names;
      s/\s*#.*$//;
      my ($opt_name, $opt_val) = /^\s+(\S+)(?:\s+|\s*=\s*)(\S+)$/;
      die "$conf_file:$.: Malformed line: $_\n" unless defined ($opt_val);

      # Check option. If illegal, die with message. Otherwise,
      # add option to each element in current element list

      ($opt_val, $err_msg) = check_option ($opt_name, $opt_val);
      die "$conf_file:$.: $err_msg\n" if defined ($err_msg);
      foreach my $elt_name (@elt_names)
      {
        $self->{elt_opts}->{$elt_name}->{$opt_name} = $opt_val;
      }
    }
  }
  close (FH);

  # For any element that has missing option values, fill in the values
  # using the options for the *DEFAULT pseudo-element.  This speeds up
  # element option lookups later.  It also makes it unnecessary to test
  # each option to see if it's defined: All element option structures
  # will have every option defined.

  my $def_opts = $self->{elt_opts}->{"*DEFAULT"};

  foreach my $elt_name (keys (%{$self->{elt_opts}}))
  {
    next if $elt_name eq "*DEFAULT";
    foreach my $opt_name (keys (%{$def_opts}))
    {
      next if exists ($self->{elt_opts}->{$elt_name}->{$opt_name}); # already set
      $self->{elt_opts}->{$elt_name}->{$opt_name} = $def_opts->{$opt_name};
    }
  }
}


# Check option name to make sure it's legal. Check the value to make sure
# that it's legal for the name.  Return a two-element array:
# (value, undef) if the option name and value are legal.
# (undef, message) if an error was found; message contains error message.
# For legal values, the returned value should be assigned to the option,
# because it may get type-converted here.

sub check_option
{
my ($opt_name, $opt_val) = @_;

  # - Check option name to make sure it's a legal option
  # - Then check the value.  If there is a list of values
  #   the value must be one of them.  Otherwise, the value
  #   must be an integer.

  return (undef, "Unknown option name: $opt_name")
    unless exists ($opt_list{$opt_name});
  my $allowable_val = $opt_list{$opt_name};
  if (defined ($allowable_val))
  {
    return (undef, "Unknown '$opt_name' value: $opt_val")
      unless grep (/^$opt_val$/, @{$allowable_val});
  }
  else  # other options should be numeric
  {
    # "$opt_val" converts $opt_val to string for pattern match
    return (undef, "'$opt_name' value ($opt_val) should be an integer")
      unless "$opt_val" =~ /^\d+$/;
  }
  return ($opt_val, undef);
}


# Return hash of option values for a given element.  If no options are found:
# - Add the element name to the list of unconfigured options.
# - Assign the default options to the element.  (This way the test for the
#   option fails only once.)

sub get_opts
{
my $self = shift;
my $elt_name = shift;

  my $opts = $self->{elt_opts}->{$elt_name};
  if (!defined ($opts))
  {
    $self->{unconf_elts}->{$elt_name} = 1;
    $opts = $self->{elt_opts}->{$elt_name} = $self->{elt_opts}->{"*DEFAULT"};
  }
  return $opts;
}


# Display contents of configuration options to be used to process document.
# For each element named in the elt_opts structure, display its format
# type, and those options that apply to the type.

sub display_config
{
my $self = shift;
# Format types and the additional options that apply to each type
my $format_opts = {
  "block" => [
              "entry-break",
              "element-break",
              "exit-break",
              "subindent",
              "normalize",
              "wrap-length"
              ],
  "inline" => [ ],
  "verbatim" => [ ]
};

  foreach my $elt_name (sort (keys (%{$self->{elt_opts}})))
  {
    print "$elt_name\n";
    my %opts = %{$self->{elt_opts}->{$elt_name}};
    my $format = $opts{format};
    # Write out format type, then options that apply to the format type
    print "  format = $format\n";
    foreach my $opt_name (@{$format_opts->{$format}})
    {
      print "  $opt_name = $opts{$opt_name}\n";
    }
    print "\n";
  }
}


# Display the list of elements that are used in the document but not
# configured in the configuration file.

# Then re-unconfigure the elements so that they won't be considered
# as configured for the next document, if there is one.

sub display_unconfigured_elements
{
my $self = shift;

  my @elts = keys (%{$self->{unconf_elts}});
  if (@elts == 0)
  {
    print "The document contains no unconfigured elements.\n";
  }
  else
  {
    print "The following document elements were assigned no formatting options:\n";
    foreach my $line ($self->line_wrap ([ join (" ", sort (@elts)) ], 0, 0, 65))
    {
      print "$line\n";
    }
  }

  foreach my $elt_name (@elts)
  {
    delete ($self->{elt_opts}->{$elt_name});
  }
}

# ----------------------------------------------------------------------

# Main document processing routine.
# - Argument is a string representing an input document
# - Return value is the reformatted document, or undef. An undef return
#   signifies either that an error occurred, or that some option was
#   given that suppresses document output. In either case, don't write
#   any output for the document.  Any error messages will already have
#   been printed when this returns.

sub process_doc
{
my $self = shift;
my ($doc, $verbose, $check_parser, $canonize_only, $show_unconf_elts) = @_;
my $str;

  $self->init_doc_vars ();

  # Perform lexical parse to split document into list of tokens
  warn "Parsing document...\n" if $verbose;
  $self->shallow_parse ($doc);

  if ($check_parser)
  {
    warn "Checking parser...\n" if $verbose;
    # concatentation of tokens should be identical to original document
    if ($doc eq join ("", @{$self->tokens ()}))
    {
      print "Parser is okay\n";
    }
    else
    {
      print "PARSER ERROR: document token concatenation differs from document\n";
    }
    return undef;
  }

  # Assign input line number to each token
  $self->assign_line_numbers ();

  # Look for and report any error tokens returned by parser
  warn "Checking document for errors...\n" if $verbose;
  if ($self->report_errors () > 0)
  {
    warn "Cannot continue processing document.\n";
    return undef;
  }

  # Convert the token list to a tree structure
  warn "Converting document tokens to tree...\n" if $verbose;
  if ($self->tokens_to_tree () > 0)
  {
    warn "Cannot continue processing document.\n";
    return undef;
  }

  # Check: Stringify the tree to convert it back to a single string,
  # then compare to original document string (should be identical)
  # (This is an integrity check on the validity of the to-tree and stringify
  # operations; if one or both do not work properly, a mismatch should occur.)
  #$str = $self->tree_stringify ();
  #print $str;
  #warn "ERROR: mismatch between document and resulting string\n" if $doc ne $str;

  # Canonize tree to remove extraneous whitespace
  warn "Canonizing document tree...\n" if $verbose;
  $self->tree_canonize ();

  if ($canonize_only)
  {
    print $self->tree_stringify () . "\n";
    return undef;
  }

  # One side-effect of canonizing the tree is that the formatting
  # options are looked up for each element in the document.  That
  # causes the list of elements that have no explicit configuration
  # to be built.  Display the list and return if user requested it.

  if ($show_unconf_elts)
  {
    $self->display_unconfigured_elements ();
    return undef;
  }

  # Format the tree to produce formatted XML as a single string
  warn "Formatting document tree...\n" if $verbose;
  $self->tree_format ();

  # If the document is not empty, add a newline and emit a warning if
  # reformatting failed to add a trailing newline.  This shouldn't
  # happen if the *DOCUMENT options are set up with exit-break = 1,
  # which is the reason for the warning rather than just silently
  # adding the newline.

  $str = $self->out_doc ();
  if ($str ne "" && $str !~ /\n$/)
  {
    warn "LOGIC ERROR: trailing newline had to be added\n";
    $str .= "\n";
  }

  return $str;
}

# ----------------------------------------------------------------------

# Parse XML document into array of tokens and store array

sub shallow_parse
{ 
my ($self, $xml_document) = @_;

  $self->{tokens} = [ $xml_document =~ /$XML_SPE/g ];
}

# ----------------------------------------------------------------------

# Extract a tag name from a tag and return it.

# Dies if the tag cannot be found, because this is supposed to be
# called only with a legal tag.

sub extract_tag_name
{
my $tag = shift;

  die "Cannot find tag name in tag: $tag\n" unless $tag =~ /^<\/?($Name)/;
  return $1;
}

# ----------------------------------------------------------------------

# Assign an input line number to each token.  The number indicates
# the line number on which the token begins.

sub assign_line_numbers
{
my $self = shift;
my $line_num = 1;

  $self->{line_num} = [ ];
  for (my $i = 0; $i < @{$self->{tokens}}; $i++)
  {
    my $token = $self->{tokens}->[$i];
    push (@{$self->{line_num}}, $line_num);
    # count newlines and increment line counter (tr returns no. of matches)
    $line_num += ($token =~ tr/\n/\n/);
  }
}

# ----------------------------------------------------------------------

# Check token list for errors and report any that are found. Error
# tokens are those that begin with "<" but do not end with ">".

# Returns the error count.

# Does not modify the original token list.

sub report_errors
{
my $self = shift;
my $err_count = 0;

  for (my $i = 0; $i < @{$self->{tokens}}; $i++)
  {
    my $token = $self->{tokens}->[$i];
    if ($token =~ /^</ && $token !~ />$/)
    {
      my $line_num = $self->{line_num}->[$i];
      warn "Malformed token at line $line_num, token " . ($i+1) . ": $token\n";
      ++$err_count;
    }
  }
  warn "Number of errors found: $err_count\n" if $err_count > 0;
  return $err_count;
}

# ----------------------------------------------------------------------

# Helper routine to print tag stack for tokens_to_tree

sub print_tag_stack
{
my ($label, @stack) = @_;
  if (@stack < 1)
  {
    warn "  $label: none\n";
  }
  else
  {
    warn "  $label:\n";
    for (my $i = 0; $i < @stack; $i++)
    {
      warn "  ", ($i+1), ": ", $stack[$i], "\n";
    }
  }
}

# Convert the list of XML document tokens to a tree representation.
# The implementation uses a loop and a stack rather than recursion.

# Does not modify the original token list.

# Returns an error count.

sub tokens_to_tree
{
my $self = shift;

  my @tag_stack = ();     # stack for element tags
  my @children_stack = ();  # stack for lists of children
  my $children = [ ];     # current list of children
  my $err_count = 0;

  for (my $i = 0; $i < @{$self->{tokens}}; $i++)
  {
    my $token = $self->{tokens}->[$i];
    my $line_num = $self->{line_num}->[$i];
    my $tok_err = "Error near line $line_num, token " . ($i+1) . " ($token)";
    if ($token !~ /^</)           # text
    {
      push (@{$children}, text_node ($token));
    }
    elsif ($token =~ /^<!--/)       # comment
    {
      push (@{$children}, comment_node ($token));
    }
    elsif ($token =~ /^<\?/)        # processing instruction
    {
      push (@{$children}, pi_node ($token));
    }
    elsif ($token =~ /^<!DOCTYPE/)      # DOCTYPE
    {
      push (@{$children}, doctype_node ($token));
    }
    elsif ($token =~ /^<!\[/)       # CDATA
    {
      push (@{$children}, cdata_node ($token));
    }
    elsif ($token =~ /^<\//)        # element close tag
    {
      if (!@tag_stack)
      {
        warn "$tok_err: Close tag w/o preceding open tag; malformed document?\n";
        ++$err_count;
        next;
      }
      if (!@children_stack)
      {
        warn "$tok_err: Empty children stack; malformed document?\n";
        ++$err_count;
        next;
      }
      my $tag = pop (@tag_stack);
      my $open_tag_name = extract_tag_name ($tag);
      my $close_tag_name = extract_tag_name ($token);
      if ($open_tag_name ne $close_tag_name)
      {
        warn "$tok_err: Tag mismatch; malformed document?\n";
        warn "  open tag: $tag\n";
        warn "  close tag: $token\n";
        print_tag_stack ("enclosing tags", @tag_stack);
        ++$err_count;
        next;
      }
      my $elt = element_node ($tag, $token, $children);
      $children = pop (@children_stack);
      push (@{$children}, $elt);
    }
    else                  # element open tag
    {
      # If we reach here, we're seeing the open tag for an element:
      # - If the tag is also the close tag (e.g., <abc/>), close the
      #   element immediately, giving it an empty child list.
      # - Otherwise, push tag and child list on stacks, begin new child
      #   list for element body.
      if ($token =~ /\/>$/)     # tag is of form <abc/>
      {
        push (@{$children}, element_node ($token, "", [ ]));
      }
      else              # tag is of form <abc>
      {
        push (@tag_stack, $token);
        push (@children_stack, $children);
        $children = [ ];
      }
    }
  }

  # At this point, the stacks should be empty if the document is
  # well-formed.

  if (@tag_stack)
  {
    warn "Error at EOF: Unclosed tags; malformed document?\n";
    print_tag_stack ("unclosed tags", @tag_stack);
    ++$err_count;
  }
  if (@children_stack)
  {
    warn "Error at EOF: Unprocessed child elements; malformed document?\n";
# TODO: print out info about them
    ++$err_count;
  }

  $self->{tree} = $children;
  return $err_count;
}


# Node-generating helper methods for tokens_to_tree

# Generic node generator

sub node         { return { "type" => $_[0], "content" => $_[1] }; }

# Generators for specific non-element nodes

sub text_node    { return node ("text",    $_[0]); }
sub comment_node { return node ("comment", $_[0]); }
sub pi_node      { return node ("pi",      $_[0]); }
sub doctype_node { return node ("DOCTYPE", $_[0]); }
sub cdata_node   { return node ("CDATA",   $_[0]); }

# For an element node, create a standard node with the type and content
# key/value pairs. Then add pairs for the "name", "open_tag", and
# "close_tag" hash keys.

sub element_node
{
my ($open_tag, $close_tag, $children) = @_;

  my $elt = node ("elt", $children);
  # name is the open tag with angle brackets and attibutes stripped
  $elt->{name} = extract_tag_name ($open_tag);
  $elt->{open_tag} = $open_tag;
  $elt->{close_tag} = $close_tag;
  return $elt;
}

# ----------------------------------------------------------------------

# Convert the given XML document tree (or subtree) to string form by
# concatentating all of its components.  Argument is a reference
# to a list of nodes at a given level of the tree.

# Does not modify the node list.

sub tree_stringify
{
my $self = shift;
my $children = shift || $self->{tree}; # use entire tree if no arg;
my $str = "";

  for (my $i = 0; $i < @{$children}; $i++)
  {
    my $child = $children->[$i];

    # - Elements have list of child nodes as content (process recursively)
    # - All other node types have text content

    if ($child->{type} eq "elt")
    {
      $str .= $child->{open_tag}
          . $self->tree_stringify ($child->{content})
          . $child->{close_tag};
    }
    else
    {
      $str .= $child->{content};
    }
  }
  return $str;
}

# ----------------------------------------------------------------------


# Put tree in "canonical" form by eliminating extraneous whitespace
# from element text content.

# $children is a list of child nodes

# This function modifies the node list.

# Canonizing occurs as follows:
# - Comment, PI, DOCTYPE, and CDATA nodes remain untouched
# - Verbatim elements and their descendants remain untouched
# - Within non-normalized block elements:
#   - Delete all-whitespace text node children
#   - Leave other text node children untouched
# - Within normalized block elements:
#   - Convert runs of whitespace (including line-endings) to single spaces
#   - Trim leading whitespace of first text node
#   - Trim trailing whitespace of last text node
#   - Trim whitespace that is adjacent to a verbatim or non-normalized
#     sub-element.  (For example, if a <programlisting> is followed by
#     more text, delete any whitespace at beginning of that text.)
# - Within inline elements:
#   - Normalize the same way as the enclosing block element, with the
#     exception that a space at the beginning or end is not removed.
#     (Otherwise, <para>three<literal> blind </literal>mice</para>
#     would become <para>three<literal>blind</literal>mice</para>.)

sub tree_canonize
{
my $self = shift;

  $self->{tree} = $self->tree_canonize2 ($self->{tree}, "*DOCUMENT");
}


sub tree_canonize2
{
my $self = shift;
my $children = shift;
my $par_name = shift;

  # Formatting options for parent
  my $par_opts = $self->get_opts ($par_name);

  # If parent is a block element, remember its formatting options on
  # the block stack so they can be used to control canonization of
  # inline child elements.

  $self->begin_block ($par_name, $par_opts) if $par_opts->{format} eq "block";

  # Iterate through list of child nodes to preserve, modify, or
  # discard whitespace.  Return resulting list of children.

  # Canonize element and text nodes. Leave everything else (comments,
  # processing instructions, etc.) untouched.

  my @new_children = ();

  while (@{$children})
  {
    my $child = shift (@{$children});

    if ($child->{type} eq "elt")
    {
      # Leave verbatim elements untouched. For other element nodes,
      # canonize child list using options appropriate to element.

      if ($self->get_opts ($child->{name})->{format} ne "verbatim")
      {
        $child->{content} = $self->tree_canonize2 ($child->{content},
                            $child->{name});
      }
    }
    elsif ($child->{type} eq "text")
    {
      # Delete all-whitespace node or strip whitespace as appropriate.

      # Paranoia check: We should never get here for verbatim elements,
      # because normalization is irrelevant for them.

      die "LOGIC ERROR: trying to canonize verbatim element $par_name!\n"
        if $par_opts->{format} eq "verbatim";

      if (!$self->block_normalize ())
      {
        # Enclosing block is not normalized:
        # - Delete child all-whitespace text nodes.
        # - Leave other text nodes untouched.

        next if $child->{content} =~ /^\s*$/;
      }
      else
      {
        # Enclosing block is normalized, so normalize this text node:
        # - Convert runs of whitespace characters (including
        #   line-endings characters) to single spaces.
        # - Trim leading whitespace if this node is the first child
        #   of a block element or it follows a non-normalized node.
        # - Trim leading whitespace if this node is the last child
        #   of a block element or it precedes a non-normalized node.

        # These are nil if there is no prev or next child
        my $prev_child = $new_children[$#new_children];
        my $next_child = $children->[0];

        $child->{content} =~ s/\s+/ /g;
        $child->{content} =~ s/^ //
          if (!defined ($prev_child) && $par_opts->{format} eq "block")
            || $self->non_normalized_node ($prev_child);
        $child->{content} =~ s/ $//
          if (!defined ($next_child) && $par_opts->{format} eq "block")
            || $self->non_normalized_node ($next_child);

        # If resulting text is empty, discard the node.
        next if $child->{content} =~ /^$/;
      }
    }
    push (@new_children, $child);
  }

  # Pop block stack if parent was a block element
  $self->end_block () if $par_opts->{format} eq "block";

  return \@new_children;
}


# Helper function for tree_canonize().

# Determine whether a node is normalized.  This is used to check
# the node that is adjacent to a given text node (either previous
# or following).
# - No is node is nil
# - No if the node is a verbatim element
# - If the node is a block element, yes or no according to its
#   normalize option
# - No if the node is an inline element.  Inlines are normalized
#   if the parent block is normalized, but this method is not called
#   except while examinine normalized blocks. So its inline children
#   are also normalized.
# - No if node is a comment, PI, DOCTYPE, or CDATA section. These are
#   treated like verbatim elements.

sub non_normalized_node
{
my $self = shift;
my $node = shift;

  return 0 if !$node;
  my $type = $node->{type};
  if ($type eq "elt")
  {
    my $node_opts = $self->get_opts ($node->{name});
    if ($node_opts->{format} eq "verbatim")
    {
      return 1;
    }
    if ($node_opts->{format} eq "block")
    {
      return $node_opts->{normalize} eq "no";
    }
    if ($node_opts->{format} eq "inline")
    {
      return 0;
    }
    die "LOGIC ERROR: non_normalized_node: unhandled node format.\n";
  }
  if ($type eq "comment" || $type eq "pi" || $type eq "DOCTYPE"
            || $type eq "CDATA")
  {
    return 1;
  }
  if ($type eq "text")
  {
    die "LOGIC ERROR: non_normalized_node: got called for text node.\n";
  }
  die "LOGIC ERROR: non_normalized_node: unhandled node type.\n";
}

# ----------------------------------------------------------------------

# Format (pretty-print) the document tree

# Does not modify the node list.

# The class maintains two variables for storing output:
# - out_doc stores content that has been seen and "flushed".
# - pending stores an array of strings (content of text nodes and inline
#   element tags).  These are held until they need to be flushed, at
#   which point they are concatenated and possibly wrapped/indented.
#   Flushing occurs when a break needs to be written, which happens
#   when something other than a text node or inline element is seen.

# If parent name and children are not given, format the entire document.
# Assume prevailing indent = 0 if not given.

sub tree_format
{
my $self = shift;
my $par_name = shift || "*DOCUMENT";    # format entire document if no arg
my $children = shift || $self->{tree};  # use entire tree if no arg
my $indent = shift || 0;

  # Formatting options for parent element
  my $par_opts = $self->get_opts ($par_name);

  # If parent is a block element:
  # - Remember its formatting options on the block stack so they can
  #   be used to control formatting of inline child elements.
  # - Set initial break type to entry-break.
  # - Shift prevailing indent right before generating child content.

  if ($par_opts->{format} eq "block")
  {
    $self->begin_block ($par_name, $par_opts);
    $self->set_block_break_type ("entry-break");
    $indent += $par_opts->{"subindent"};
  }

  # Variables for keeping track of whether the previous child
  # was a text node. Used for controlling break behavior in
  # non-normalized block elements: No line breaks are added around
  # text in such elements, nor is indenting added.

  my $prev_child_is_text = 0;
  my $cur_child_is_text = 0;

  foreach my $child (@{$children})
  {
    $prev_child_is_text = $cur_child_is_text;

    # Text nodes: just add text to pending output

    if ($child->{type} eq "text")
    {
      $cur_child_is_text = 1;
      $self->add_to_pending ($child->{content});
      next;
    }

    $cur_child_is_text = 0;

    # Element nodes: handle depending on format type

    if ($child->{type} eq "elt")
    {
      my $child_opts = $self->get_opts ($child->{name});

      # Verbatim elements:
      # - Print literally without change (use _stringify).
      # - Do not line-wrap or add any indent.

      if ($child_opts->{format} eq "verbatim")
      {
        $self->flush_pending ($indent);
        $self->emit_break (0)
          unless $prev_child_is_text && !$self->block_normalize ();
        $self->set_block_break_type ("element-break");
        $self->add_to_doc ($child->{open_tag}
                          . $self->tree_stringify ($child->{content})
                          . $child->{close_tag});
        next;
      }

      # Inline elements:
      # - Do not break or indent.
      # - Do not line-wrap content; just add content to pending output
      #   and let it be wrapped as part of parent's content.

      if ($child_opts->{format} eq "inline")
      {
        $self->add_to_pending ($child->{open_tag});
        $self->tree_format ($child->{name}, $child->{content}, $indent);
        $self->add_to_pending ($child->{close_tag});
        next;
      }

      # If we get here, node is a block element.

      # - Break and flush any pending output
      # - Break and indent (no indent if break count is zero)
      # - Process element itself:
      #   - Put out opening tag
      #   - Put out element content
      #   - Put out any indent needed before closing tag. None needed if:
      #     - Element's exit-break is 0 (closing tag is not on new line,
      #       so don't indent it)
      #     - There is no separate closing tag (it was in <abc/> format)
      #     - Element has no children (tags will be written as
      #       <abc></abc>, so don't indent closing tag)
      #     - Element has children, but the block is not normalized and
      #       the last child is a text node
      #   - Put out closing tag

      $self->flush_pending ($indent);
      $self->emit_break ($indent)
        unless $prev_child_is_text && !$self->block_normalize ();
      $self->set_block_break_type ("element-break");
      $self->add_to_doc ($child->{open_tag});
      $self->tree_format ($child->{name}, $child->{content}, $indent);
      $self->add_to_doc (" " x $indent)
        unless $child_opts->{"exit-break"} <= 0
        || $child->{close_tag} eq ""
        || !@{$child->{content}}
        || (@{$child->{content}}
              && $child->{content}->[$#{$child->{content}}]->{type} eq "text"
              && $child_opts->{normalize} eq "no");
      $self->add_to_doc ($child->{close_tag});
      next;
    }

    # Comments, PIs, etc. (everything other than text and elements),
    # treat similarly to verbatim block:
    # - Flush any pending output
    # - Put out a break
    # - Add node content to collected output

    $self->flush_pending ($indent);
    $self->emit_break (0)
      unless $prev_child_is_text && !$self->block_normalize ();
    $self->set_block_break_type ("element-break");
    $self->add_to_doc ($child->{content});
  }

  $prev_child_is_text = $cur_child_is_text;

  # Done processing current element's children now.

  # If current element is a block element:
  # - If there were any children, flush any pending output and put
  #   out the exit break.
  # - Pop the block stack

  if ($par_opts->{format} eq "block")
  {
    if (@{$children})
    {
      $self->flush_pending ($indent);
      $self->set_block_break_type ("exit-break");
      $self->emit_break (0)
        unless $prev_child_is_text && !$self->block_normalize ();
    }
    $self->end_block ();
  }
}


# Emit a break - the appropriate number of newlines according to the
# enclosing block's current break type.

# In addition, emit the number of spaces indicated by indent.  (indent
# > 0 when breaking just before emitting an element tag that should
# be indented within its parent element.)

# Exception: Emit no indent if break count is zero. That indicates
# any following output will be written on the same output line, not
# indented on a new line.

# Initially, when processing a node's child list, the break type is
# set to entry-break. Each subsequent break is an element-break.
# (After child list has been processed, an exit-break is produced as well.)

sub emit_break
{
my ($self, $indent) = @_;

  # number of newlines to emit
  my $break_value = $self->block_break_value ();

  $self->add_to_doc ("\n" x $break_value);
  # add indent if there *was* a break
  $self->add_to_doc (" " x $indent) if $indent > 0 && $break_value > 0;
}


# Flush pending output to output document collected thus far:
# - Wrap pending contents as necessary, with indent before *each* line.
# - Add pending text to output document (thus "flushing" it)
# - Clear pending array.

sub flush_pending
{
my ($self, $indent) = @_;

  # Do nothing if nothing to flush
  return if !@{$self->{pending}};

  # If current block is not normalized:
  # - Text nodes cannot be modified (no wrapping or indent).  Flush
  #   text as is without adding a break or indent.
  # If current block is normalized:
  # - Add a break.
  # - If line wrap is disabled:
  #   - Add indent if there is a break. (If there isn't a break, text
  #     should immediately follow preceding tag, so don't add indent.)
  #   - Add text without wrapping
  # - If line wrap is enabled:
  #   - First line indent is 0 if there is no break. (Text immediately
  #     follows preceding tag.) Otherwise first line indent is same as
  #     prevailing indent.
  #   - Any subsequent lines get the prevailing indent.

  # After flushing text, advance break type to element-break.

  my $s = "";

  if (!$self->block_normalize ())
  {
    $s .= join ("", @{$self->{pending}});
  }
  else
  {
    $self->emit_break (0);
    my $wrap_len = $self->block_wrap_length ();
    my $break_value = $self->block_break_value ();
    if ($wrap_len <= 0)
    {
      $s .= " " x $indent if $break_value > 0;
      $s .= join ("", @{$self->{pending}});
    }
    else
    {
      my $first_indent = ($break_value > 0 ? $indent : 0);
      # Wrap lines, then join by newlines (don't add one at end)
      my @lines = $self->line_wrap ($self->{pending},
                  $first_indent,
                  $indent,
                  $wrap_len);
      $s .= join ("\n", @lines);
    }
  }

  $self->add_to_doc ($s);
  $self->{pending} = [ ];
  $self->set_block_break_type ("element-break");
}


# Perform line-wrapping of string array to lines no longer than given
# length (including indent).
# Any word longer than line length appears by itself on line.
# Return array of lines (not newline-terminated).

# $strs - reference to array of text items to be joined and line-wrapped.
# Each item may be:
# - A tag (such as <emphasis role="bold">). This should be treated as
#   an atomic unit, which is important for preserving inline tags intact.
# - A possibly multi-word string (such as "This is a string"). In this
#   latter case, line-wrapping preserves internal whitespace in the
#   string, with the exception that if whitespace would be placed at
#   the end of a line, it is discarded.

# $first_indent - indent for first line
# $rest_indent - indent for any remaining lines
# $max_len - maximum length of output lines (including indent)

sub line_wrap
{
my ($self, $strs, $first_indent, $rest_indent, $max_len) = @_;

  # First, tokenize the strings

  my @words = ();
  foreach my $str (@{$strs})
  {
    if ($str =~ /^</)
    {
      # String is a tag; treat as atomic unit and don't split
      push (@words, $str);
    }
    else
    {
      # String of white and non-white tokens.
      # Tokenize into white and non-white tokens.
      push (@words, ($str =~ /\S+|\s+/g));
    }
  }

  # Now merge tokens that are not separated by whitespace tokens. For
  # example, "<i>", "word", "</i>" gets merged to "<i>word</i>".  But
  # "<i>", " ", "word", " ", "</i>" gets left as separate tokens.

  my @words2 = ();
  foreach my $word (@words)
  {
    # If there is a previous word that does not end with whitespace,
    # and the currrent word does not begin with whitespace, concatenate
    # current word to previous word.  Otherwise append current word to
    # end of list of words.
    if (@words2 && $words2[$#words2] !~ /\s$/ && $word !~ /^\s/)
    {
      $words2[$#words2] .= $word;
    }
    else
    {
      push (@words2, $word);
    }
  }

  my @lines = ();
  my $line = "";
  my $llen = 0;
  # set the indent for the first line
  my $indent = $first_indent;
  # saved-up whitespace to put before next non-white word
  my $white = "";

  foreach my $word (@words2)   # ... while words remain to wrap
  {
    # If word is whitespace, save it. It gets added before next
    # word if no line-break occurs.
    if ($word =~ /^\s/)
    {
       $white .= $word;
      next;
    }
    my $wlen = length ($word);
    if ($llen == 0)
    {
      # New output line; it gets at least one word (discard any
      # saved whitespace)
      $line = " " x $indent . $word;
      $llen = $indent + $wlen;
      $indent = $rest_indent;
      $white = "";
      next;
    }
    if ($llen + length ($white) + $wlen > $max_len)
    {
      # Word (plus saved whitespace) won't fit on current line.
      # Begin new line (discard any saved whitespace).
      push (@lines, $line);
      $line = " " x $indent . $word;
      $llen = $indent + $wlen;
      $indent = $rest_indent;
      $white = "";
      next;
    }
    # add word to current line with saved whitespace between
    $line .= $white . $word;
    $llen += length ($white) + $wlen;
    $white = "";
  }

  # push remaining line, if any
  push (@lines, $line) if $line ne "";

  return @lines;
}

1;

# ----------------------------------------------------------------------

# Begin main program

package main;


my $usage = <<EOF;
Usage: $PROG_NAME [options] xml-file

Options:
--help, -h
    Print this message and exit.
--backup suffix -b suffix
    Back up the input document, adding suffix to the input
    filename to create the backup filename.
--canonized-output
    Proceed only as far as the document canonization stage,
    printing the result.
--check-parser
    Parse the document into tokens and verify that their
    concatenation is identical to the original input document.
    This option suppresses further document processing.
--config-file file_name, -f file_name
    Specify the configuration filename. If no file is named,
    xmlformat uses the file named by the environment variable
    XMLFORMAT_CONF, if it exists, or ./xmlformat.conf, if it
    exists. Otherwise, xmlformat uses built-in formatting
    options.
--in-place, -i
    Format the document in place, replacing the contents of
    the input file with the reformatted document. (It's a
    good idea to use --backup along with this option.)
--show-config
    Show configuration options after reading configuration
    file. This option suppresses document processing.
--show-unconfigured-elements
    Show elements that are used in the document but for
    which no options were specified in the configuration
    file. This option suppresses document output.
--verbose, -v
    Be verbose about processing stages.
--version, -V
    Show version information and exit.
EOF

# Variables for command line options; most are undefined initially.
my $help;
my $backup_suffix;
my $conf_file;
my $canonize_only;
my $check_parser;
my $in_place;
my $show_conf;
my $show_unconf_elts;
my $show_version;
my $verbose;

GetOptions (
  # =i means an integer argument is required after the option
  # =s means a string argument is required after the option
  # :s means a string argument is optional after the option
  "help|h"           => \$help,          # print help message
  "backup|b=s"       => \$backup_suffix, # make backup using suffix
  "canonized-output" => \$canonize_only, # print canonized document
  "check-parser"     => \$check_parser,  # verify parser integrity
  "config-file|f=s"  => \$conf_file,     # config file
  "in-place|i"       => \$in_place,      # format in place
  "show-config"      => \$show_conf,     # show configuration file
  # need better name
  "show-unconfigured-elements" => \$show_unconf_elts,   # show unconfigured elements
  "verbose|v"        => \$verbose,       # be verbose
  "version|V"        => \$show_version,  # show version info
) or do { print "$usage\n"; exit (1); };

if (defined ($help))
{
  print "$usage\n";
  exit (0);
}

if (defined ($show_version))
{
  print "$PROG_NAME $PROG_VERSION ($PROG_LANG version)\n";
  exit (0);
}

# --in-place option requires a named file

warn "WARNING: --in-place/-i option ignored (requires named input files)\n"
  if defined ($in_place) && @ARGV == 0;

# --backup/-b is meaningless without --in-place

if (defined ($backup_suffix))
{
  if (!defined ($in_place))
  {
    die "--backup/-b option meaningless without --in-place/-i option\n";
  }
}

# Save input filenames
my @in_file = @ARGV;

my $xf = XMLFormat->new ();

# If a configuration file was named explicitly, use it. An error occurs
# if the file does not exist.

# If no configuration file was named, fall back to:
# - The file named by the environment variable XMLFORMAT_CONF, if it exists
# - ./xmlformat.conf, if it exists

# If no configuration file can be found at all, the built-in default options
# are used. (These are set up in new().)

my $env_conf_file = $ENV{XMLFORMAT_CONF};
my $def_conf_file = "./xmlformat.conf";

# If no config file was named, but XMLFORMAT_CONF is set, use its value
# as the config file name.
if (!defined ($conf_file))
{
  $conf_file = $env_conf_file if defined ($env_conf_file);
}
# If config file still isn't defined, use the default file if it exists.
if (!defined ($conf_file))
{
  if (-r $def_conf_file && ! -d $def_conf_file)
  {
    $conf_file = $def_conf_file;
  }
}
if (defined ($conf_file))
{
  warn "Reading configuration file...\n" if $verbose;
  die "Configuration file '$conf_file' is not readable.\n" if ! -r $conf_file;
  die "Configuration file '$conf_file' is a directory.\n"  if -d $conf_file;
  $xf->read_config ($conf_file)
}

if ($show_conf)   # show configuration and exit
{
  $xf->display_config ();
  exit(0);
}

my ($in_doc, $out_doc);

# Process arguments.
# - If no files named, read string, write to stdout.
# - If files named, read and process each one. Write output to stdout
#   unless --in-place option was given.  Make backup of original file
#   if --backup option was given.

if (@ARGV == 0)
{
  warn "Reading document...\n" if $verbose;
  {
    local $/ = undef;
    $in_doc = <>;            # slurp input document as single string
  }

  $out_doc = $xf->process_doc ($in_doc,
              $verbose, $check_parser, $canonize_only, $show_unconf_elts);
  if (defined ($out_doc))
  {
    warn "Writing output document...\n" if $verbose;
    print $out_doc;
  }
}
else
{
  foreach my $file (@ARGV)
  {
    warn "Reading document $file...\n" if $verbose;
    open (IN, $file)
      or die "Cannot read $file: $!\n";
    {
      local $/ = undef;
      $in_doc = <IN>;            # slurp input document as single string
    }
    close (IN);
    $out_doc = $xf->process_doc ($in_doc,
                $verbose, $check_parser, $canonize_only, $show_unconf_elts);
    next unless defined ($out_doc);
    if (defined ($in_place))
    {
      if (defined ($backup_suffix))
      {
        warn "Making backup of $file to $file$backup_suffix...\n" if $verbose;
        rename ($file, $file . $backup_suffix)
          or die "Could not rename $file to $file$backup_suffix: $!\n";
      }
      warn "Writing output document to $file...\n" if $verbose;
      open (OUT, ">$file") or die "Cannot write to $file: $!\n";
      print OUT $out_doc;
      close (OUT);
    }
    else
    {
      warn "Writing output document...\n" if $verbose;
      print $out_doc;
    }
  }
}

warn "Done!\n" if $verbose;

exit (0);
