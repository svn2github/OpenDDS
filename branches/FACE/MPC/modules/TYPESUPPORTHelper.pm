package TYPESUPPORTHelper;

use strict;
use CommandHelper;
use File::Basename;
our @ISA = qw(CommandHelper);

sub get_output {
  my($self, $file, $flags) = @_;

  my $dir = '';
  $flags = '' unless defined $flags;
  if ($flags =~ /-o +(\S+)/) {
    $dir = "$1/";
  }

  my @out = ();
  if ($flags =~ /-SI/) {
    return \@out;
  }

  my $tsidl = $file;
  $tsidl =~ s/\.idl$/TypeSupport.idl/;
  push(@out, $dir . basename($tsidl));

  my $i2j = CommandHelper::get('idl2jni_files');

  if ($flags =~ /-Wb,java/) {

    $i2j->do_cached_parse($file, $flags);

    my $tsfile = $tsidl;
    $tsfile =~ s/\\/\//g;

    my $tsinfo = $i2j->get_typesupport_info($tsfile);
    if (defined $tsinfo) {
      my @types = split /;/, $tsinfo;
      foreach my $type (@types) {
        $type =~ s/::/\//g;
        push(@out, $dir . $type . 'TypeSupportImpl.java');
      }
    }
  }

  if ($flags =~ /-Gface/) {
    my $name = basename($file, '.idl');
    foreach my $suffix ('_TSS.hpp', '_TSS.cpp') {
      push(@out, $dir . $name . $suffix);
    }
  }

  return \@out;
}

sub get_outputexts {
  return ['\\.idl', '\\.java']; # these are regular expressions
}

1;
