eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (DDS_ROOT);
use lib "$DDS_ROOT/bin";
use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlDDS::Run_Test;
use strict;

PerlDDS::add_lib_path('../../Utils/');
PerlDDS::add_lib_path('../FooType4');

my $status = 0;

my $logging_p = "-DCPSDebugLevel 1 -ORBVerboseLogging 1 " .
    "-DCPSTransportDebugLevel 1";
my $logging_s = "-DCPSDebugLevel 1 -ORBVerboseLogging 1 " .
    "-DCPSTransportDebugLevel 1";
my $reliable = 1;

my $nobit = 1; # Set to a non-zero value to disable the Builtin Topics.

my $pub_ini = ' -DCPSConfigFile tcp.ini';
my $sub_ini = ' -DCPSConfigFile tcp.ini';

for my $arg (@ARGV) {
    if ($arg eq 'udp') {
        $pub_ini = " -DCPSConfigFile udp.ini";
        $sub_ini = " -DCPSConfigFile udp.ini";
        $reliable = 0;
    }
    elsif ($arg eq 'multicast') {
        $pub_ini = " -DCPSConfigFile multicast.ini";
        $sub_ini = " -DCPSConfigFile multicast.ini";
    }
    elsif ($arg eq 'multicast_async') {
        $pub_ini = " -DCPSConfigFile pub_multicast_async.ini";
        $sub_ini = " -DCPSConfigFile multicast.ini";
    }
    elsif ($arg eq 'shmem') {
        $pub_ini = " -DCPSConfigFile shmem.ini";
        $sub_ini = " -DCPSConfigFile shmem.ini";
    }
    elsif ($arg eq 'rtps') {
        $pub_ini = " -DCPSConfigFile rtps.ini";
        $sub_ini = " -DCPSConfigFile rtps.ini";
    }
    elsif ($arg eq 'rtps') {
        $pub_ini = " -DCPSConfigFile rtps.ini";
        $sub_ini = " -DCPSConfigFile rtps.ini";
    }
    elsif ($arg eq 'BIT' || $arg eq 'bit') {
        $nobit = 0;
    }
    elsif ($arg ne '') {
        print STDERR "ERROR: invalid test case\n";
        exit 1;
    }
}

my $app_bit_opt = '';
my $repo_bit_opt = '';

$app_bit_opt = '-DCPSBit 0 ' if $nobit;
$repo_bit_opt = '-NOBITS' if $nobit;

my $pub_opts = "$app_bit_opt $pub_ini";
my $sub_opts = "$app_bit_opt $sub_ini";
$sub_opts .= " -r $reliable";
my $messages = 60;
my $pub1_opts = "$logging_p -ORBLogFile pub1.log $pub_opts -stage 1 -messages $messages";
my $sub1_opts = "$logging_s -ORBLogFile sub1.log $sub_opts -stage 1 -verbose";

my $pub2_opts = "$logging_p -ORBLogFile pub2.log $pub_opts -stage 2 -messages $messages";
my $sub2_opts = "$logging_s -ORBLogFile sub2.log $sub_opts -stage 2 -verbose";

my $dcpsrepo_ior = "repo.ior";
my $info_prst_file = "info.pr";

unlink $dcpsrepo_ior;
unlink $info_prst_file;
unlink <*.log>;

my $SRV_PORT = PerlACE::random_port();
my $DCPSREPO = PerlDDS::create_process("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                                       "$repo_bit_opt -o $dcpsrepo_ior "
                                       . "-ORBSvcConf mySvc.conf "
                                       . "-orbendpoint iiop://:$SRV_PORT ");

my $Subscriber1 = PerlDDS::create_process("subscriber", $sub1_opts);
my $Publisher1 = PerlDDS::create_process("publisher", $pub1_opts);

my $Subscriber2 = PerlDDS::create_process("subscriber", $sub2_opts);
my $Publisher2 = PerlDDS::create_process("publisher", $pub2_opts);

print $DCPSREPO->CommandLine() . "\n";
$DCPSREPO->Spawn();
if (PerlACE::waitforfile_timed($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for Info Repo IOR file\n";
    $DCPSREPO->Kill();
    exit 1;
}

print $Publisher1->CommandLine() . "\n";
$Publisher1->Spawn();
$DCPSREPO->Wait(1);

print $Subscriber1->CommandLine() . "\n";
$Subscriber1->Spawn();

# 1 second delay between messages
my $wait_time = $messages;
$DCPSREPO->Wait($wait_time / 2);
my $ir = $DCPSREPO->TerminateWaitKill(10);
if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}
$DCPSREPO->Wait($wait_time / 2);

unlink $dcpsrepo_ior;

print $DCPSREPO->CommandLine() . "\n";
$DCPSREPO->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for Info Repo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}

print $Publisher2->CommandLine() . "\n";
$Publisher2->Spawn();
$DCPSREPO->Wait(1);

print $Subscriber2->CommandLine() . "\n";
$Subscriber2->Spawn();

# 1 second delay between messages + some extra time
$wait_time = $messages * 2;
$Subscriber1->Wait($wait_time);
print STDERR "shutting down subscriber #1\n";
my $Subscriber1Result = $Subscriber1->WaitKill(10);
if ($Subscriber1Result != 0) {
    print STDERR "ERROR: subscriber #1 returned $Subscriber1Result\n";
    $status = 1;
}

my $Subscriber2Result = $Subscriber2->WaitKill(10);
if ($Subscriber2Result != 0) {
    print STDERR "ERROR: subscriber #2 returned $Subscriber2Result\n";
    $status = 1;
}

my $Publisher1Result = $Publisher1->WaitKill(10);
if ($Publisher1Result != 0) {
    print STDERR "ERROR: publisher #1 returned $Publisher1Result\n";
    $status = 1;
}

my $Publisher2Result = $Publisher2->WaitKill(10);
if ($Publisher2Result != 0) {
    print STDERR "ERROR: publisher #2 returned $Publisher2Result\n";
    $status = 1;
}

my $ir = $DCPSREPO->TerminateWaitKill(10);
if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}

unlink $dcpsrepo_ior;

if ($status == 0) {
  print "test PASSED.\n";
} else {
  print "**** Begin log file output *****\n";
  if (open FILE, "<", "pub1.log") {
      print "Publisher1:\n";
      while (my $line = <FILE>) {
          print "$line";
      }
      print "\n\n";
      close FILE;
  }

  if (open FILE, "<", "pub2.log") {
      print "Publisher2:\n";
      while (my $line = <FILE>) {
          print "$line";
      }
      print "\n\n";
      close FILE;
  }

  if (open FILE, "<", "sub1.log") {
      print "Subscriber1:\n";
      while (my $line = <FILE>) {
          print "$line";
      }
      print "\n\n";
      close FILE;
  }

  print "**** End log file output *****\n";

  print STDERR "test FAILED.\n";
}

exit $status;
