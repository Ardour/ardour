#! /usr/bin/perl

# The lisp definitions for flags does not include order.
# thus we must extract it ourselves.
# Usage: ./enum.pl /gnome/head/cvs/gconf/gconf/*.h > gconf_enums.defs

use warnings;

my %token;
$module="none";

while ($ARGV[0] =~ /^--(\S+)/)
{
  shift @ARGV;
  $module=shift @ARGV if ($1 eq "module");
  if ($1 eq "help")
  {
     print "enum.pl [--module modname] header_files ....\n";
     exit 0;
  }
}
  
foreach $file (@ARGV)
{
  &parse($file);
}

exit;



# parse enums from C
sub parse
{
  my ($file)=@_;

  $from=0;
  open(FILE,$file);

  $enum=0;
  $deprecated=0;
  $comment=0;

  while(<FILE>)
  {
    if($comment)
    {
      # end of multiline comment
      $comment = 0 if(/\*\//);
      next;
    }

    $deprecated = 1 if(s/^#ifndef [A-Z_]+_DISABLE_DEPRECATED//);

    ++$deprecated if($deprecated > 0 && /^#\s*if/);
    --$deprecated if($deprecated > 0 && /^#\s*endif/);

    next if($deprecated > 0);

    # filter single-line comments
    s/\/\*.*\*\///g;

    # begin of multiline comment
    if(/\/\*/)
    {
      $comment = 1;
      next;
    }

    s/','/\%\%COMMA\%\%/;
    s/'}'/\%\%RBRACE\%\%/;
    if (/^\s*typedef enum/ )
    {
      print ";; From $file\n\n" if (!$from);
      $from=1;
      $enum=1;
      next;
    }

    if ($enum && /\}/)
    {
       $enum=0;
       &process($line,$_);
       $line="";
    }
    $line.=$_ if ($enum);
  }
}


# convert enums to lisp
sub process
{
  my ($line,$def)=@_;

  $def=~s/\s*\}\s*//g;
  $def=~s/\s*;\s*$//;
  my $c_name=$def;

  $line=~s/\s+/ /g;
  $line=~s/\/\*.*\*\///g;
  $line=~s/\s*{\s*//;

  my $entity = "enum";
  $c_name =~ /^([A-Z][a-z]*)/;
  $module = $1;
  $def =~ s/$module//;

  @c_name=();
  @name=();
  @number=();

  $val=0;
  foreach $i (split(/,/,$line))
    {
      $i=~s/^\s+//;
      $i=~s/\s+$//;
      if ($i =~ /^\S+$/)
      { 
        push(@c_name,$i);
        push(@number,sprintf("%d",$val));
        $token{$i}=$val;
      }
      elsif ($i =~ /^(\S+)\s*=\s*(0x[0-9a-fA-F]+)$/ || 
             $i =~ /^(\S+)\s*=\s*(-?[0-9]+)$/ ||
             $i =~ /^(\S+)\s*=\s*\(?(1\s*<<\s*[0-9]+)\)?$/
            )
      { 
        my ($tmp1, $tmp2) = ($1, $2);
        push(@c_name, $tmp1);
        eval("\$val = $tmp2;");
        $entity = "flags" if($tmp2 =~ /^1\s*<</ || $tmp2 =~ /^0x/);
        push(@number, $tmp2);
        $token{$tmp1} = $tmp2;
      }
      elsif ($i =~ /^(\S+)\s*=\s*([ _x0-9a-fA-Z|()~]+)$/)
      { 
        my ($tmp1, $tmp2) = ($1, $2);
        push(@c_name, $tmp1);
        $tmp2 =~ s/([A-Z_]+)/($token{$1})/;
        eval("\$val = $tmp2;");
	$val = "#error" if(!$val);
        $val = sprintf("0x%X", $val) if($entity eq "flags");
        push(@number, $val);
        $token{$tmp1} = $val;
      }
      elsif ($i =~ /^(\S+)\s*=\s*'(.)'$/)
      {
        push(@c_name,$1);
        push(@number,"\'$2\'");
        $val=ord($2);
        $token{$1}=$val;
      }
      elsif ($i =~ /^(\S+)\s*=\s*(\%\%[A-Z]+\%\%)$/)
      {
        $tmp=$1;
        $_=$2;
        s/\%\%COMMA\%\%/,/; 
        s/\%\%RBRACE\%\%/]/; 
        push(@c_name,$tmp);
        push(@number,"\'$_\'");
        $val=ord($_);
        $token{$tmp}=$val;
      }
      else
      {
        #print STDERR "$i\n";
      }
      $val++;
    }

  # remove the prefix to form names
  &form_names(\@name,\@c_name);

  my $format = "%d";
  $format = "0x%X" if($entity eq "flags");

  # evaluate any unevaluated values
  my $j;
  for ($j=0;$j<$#number+1;$j++)
  {
    if ($number[$j]=~/\$/)
    {
      $number[$j]=sprintf($format, eval($number[$j]));
    }
  }

  #print ";; Enum $def\n\n";
  print "(define-$entity-extended $def\n";
  print "  (in-module \"$module\")\n";
  print "  (c-name \"$c_name\")\n";

  print "  (values\n";
  for ($j=0;$j<$#c_name+1;$j++)
  {
    print "    \'(\"$name[$j]\" \"$c_name[$j]\"";
    print " \"$number[$j]\"" if ($number[$j] ne "");
    print ")\n";
  }
  print "  )\n";
  print ")\n\n";
}


sub form_names
{
  my ($name,$c_name)=@_;
 
  my $len=length($$c_name[0]) - 1;
  my $j;

  NAME: for ($j=0;$j<$#c_name;$j++)
  {
    while (substr($$c_name[$j],$len-1,1) ne "_" ||
           substr($$c_name[$j],0,$len) ne substr($$c_name[$j+1],0,$len))
    {
      $len--;
      last NAME if ($len <= 0);
    }
    #print substr($$c_name[$j],0,$len),"\n";
  }
  
  my $prefix=substr($$c_name[0],0,$len);

  for ($j=0;$j<$#c_name+1;$j++)
  {
    $_=$$c_name[$j];
    s/^$prefix//;
    tr/A-Z_/a-z-/;
    push(@$name,$_);
  }

}  
