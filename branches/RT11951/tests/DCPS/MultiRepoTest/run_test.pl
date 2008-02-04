eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlACE::Run_Test;

my $status = 0;
my $failed = 0;

PerlACE::add_lib_path('../FooType5');

# Name the pieces.

my $samples = 10;

my $sys1_addr = "localhost:16701";
my $sys1_pub_domain = 511;
my $sys1_sub_domain = 411;
my $sys1_pub_topic  = "Sys1";
my $sys1_sub_topic  = "Sys1";
my $sys2_addr = "localhost:16703";
my $sys2_pub_domain = 811;
my $sys2_sub_domain = 711;
my $sys2_pub_topic  = "Sys2";
my $sys2_sub_topic  = "Sys2";
my $sys3_addr = "localhost:16705";
my $sys3_pub_domain = 911;
my $sys3_sub_domain = 911;
my $sys3_pub_topic  = "Left";
my $sys3_sub_topic  = "Right";
my $monitor_addr = "localhost:29803";

my $domains1_file = PerlACE::LocalFile ("domain1_ids");
my $domains2_file = PerlACE::LocalFile ("domain2_ids");
my $domains3_file = PerlACE::LocalFile ("domain3_ids");

my $dcpsrepo1_ior = PerlACE::LocalFile ("repo1.ior");
my $dcpsrepo2_ior = PerlACE::LocalFile ("repo2.ior");
my $dcpsrepo3_ior = PerlACE::LocalFile ("repo3.ior");

my $system1_ini   = PerlACE::LocalFile ("system1.ini");
my $system2_ini   = PerlACE::LocalFile ("system2.ini");
my $system3_ini   = PerlACE::LocalFile ("system3.ini");
my $monitor_ini   = PerlACE::LocalFile ("monitor.ini");

# Change how test is configured according to which test we are.

my $system1_config = "";
my $system2_config = "";
my $system3_config = "";
my $monitor_config = "";

if ($ARGV[0] eq 'fileconfig') {
  # File configuration test.
  $system1_config = "-DCPSConfigFile $system1_ini ";
  $system2_config = "-DCPSConfigFile $system2_ini ";
  $system3_config = "-DCPSConfigFile $system3_ini ";
  $monitor_config = "-DCPSConfigFile $monitor_ini "
                  . "-WriterDomain $sys1_sub_domain -ReaderDomain $sys1_pub_domain "
                  . "-WriterDomain $sys2_sub_domain -ReaderDomain $sys2_pub_domain "
                  . "-WriterDomain $sys3_sub_domain -ReaderDomain $sys3_pub_domain "; 
  print STDERR "WARNING: file configuration test not implemented\n";

} elsif ($ARGV[0] eq '') {
  # Default: Command line configuration test.
  $system1_config = "-InfoRepo file://$dcpsrepo1_ior ";
  $system2_config = "-InfoRepo file://$dcpsrepo2_ior ";
  $system3_config = "-InfoRepo file://$dcpsrepo3_ior ";

  # This is horribly position dependent.  The InfoRepo IOR values start
  # with the first RepositoryKey value 0 and increment as they are
  # encountered with the current (in the command line) key value used to
  # bind any domain definitions from the command line.
  $monitor_config = "-InfoRepo file://$dcpsrepo1_ior "
                  . "-WriterDomain $sys1_sub_domain -ReaderDomain $sys1_pub_domain "
                  . "-InfoRepo file://$dcpsrepo2_ior "
                  . "-WriterDomain $sys2_sub_domain -ReaderDomain $sys2_pub_domain "
                  . "-InfoRepo file://$dcpsrepo3_ior "
                  . "-WriterDomain $sys3_sub_domain -ReaderDomain $sys3_pub_domain "; 

} else {
  print STDERR "ERROR: invalid parameter $ARGV[0]\n";
  exit 1;
}

# Clean out any left overs from a previous run.

unlink $dcpsrepo1_ior;
unlink $dcpsrepo2_ior;
unlink $dcpsrepo3_ior;

# Configure the repositories.

my $svc_config = new PerlACE::ConfigList->check_config ('STATIC') ? ''
    : "-ORBSvcConf ../../tcp.conf ";

my $DCPSREPO1 = new PerlACE::Process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                             "$svc_config -o $dcpsrepo1_ior"
#                             . " -ORBDebugLevel 1"
                             . " -d $domains1_file");
print $DCPSREPO1->CommandLine(), "\n";

my $DCPSREPO2 = new PerlACE::Process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                             "$svc_config -o $dcpsrepo2_ior"
#                             . " -ORBDebugLevel 1"
                             . " -d $domains2_file");
print $DCPSREPO2->CommandLine(), "\n";

my $DCPSREPO3 = new PerlACE::Process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                             "$svc_config -o $dcpsrepo3_ior"
#                             . " -ORBDebugLevel 1"
                             . " -d $domains3_file");
print $DCPSREPO3->CommandLine(), "\n";

# Configure the subsystems.

my $sys1_parameters = "$svc_config $system1_config "
                    .             "-WriterDomain $sys1_pub_domain -ReaderDomain $sys1_sub_domain "
                    .             "-WriterTopic  $sys1_pub_topic  -ReaderTopic  $sys1_sub_topic "
                    .             "-Address $sys1_addr ";
my $System1 = new PerlACE::Process ("system", $sys1_parameters);
print $System1->CommandLine(), "\n";

my $sys2_parameters = "$svc_config $system2_config "
                    .             "-WriterDomain $sys2_pub_domain -ReaderDomain $sys2_sub_domain "
                    .             "-WriterTopic  $sys2_pub_topic  -ReaderTopic  $sys2_sub_topic "
                    .             "-Address $sys2_addr ";
my $System2 = new PerlACE::Process ("system", $sys2_parameters);
print $System2->CommandLine(), "\n";

my $sys3_parameters = "$svc_config $system3_config "
                    .             "-WriterDomain $sys3_pub_domain -ReaderDomain $sys3_sub_domain "
                    .             "-WriterTopic  $sys3_pub_topic  -ReaderTopic  $sys3_sub_topic "
                    .             "-Address $sys3_addr ";
my $System3 = new PerlACE::Process ("system", $sys3_parameters);
print $System3->CommandLine(), "\n";

# Configure the monitor.

# The monitor has 3 Writers and 3 Readers, which are to be connected with
# the subsystems defined above.  The topology goes like this:
#
#   monitor writer [0]    --->   system1 reader
#   monitor reader [0]   <---    system1 writer
#
#   monitor writer [1]    --->   system2 reader
#   monitor reader [1]   <---    system2 writer
#
#   monitor writer [2]    --->   system3 reader
#   monitor reader [2]   <---    system3 writer
#

$monitor_parameters = "$svc_config -Samples $samples $monitor_config "
                    . "-WriterTopic $sys1_sub_topic "
                    . "-WriterTopic $sys2_sub_topic "
                    . "-WriterTopic $sys3_sub_topic "
                    . "-ReaderTopic $sys1_pub_topic "
                    . "-ReaderTopic $sys2_pub_topic "
                    . "-ReaderTopic $sys3_pub_topic "
                    . "-Address $monitor_addr ";

my $Monitor = new PerlACE::Process ("monitor", $monitor_parameters);
print $Monitor->CommandLine(), "\n";

# Fire up the repositories.

$DCPSREPO1->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo1_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo 1 IOR file\n";
    $DCPSREPO1->Kill ();
    exit 1;
}

$DCPSREPO2->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo2_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo 2 IOR file\n";
    $DCPSREPO2->Kill ();
    $DCPSREPO1->Kill ();
    exit 1;
}

$DCPSREPO3->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo3_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo 3 IOR file\n";
    $DCPSREPO3->Kill ();
    $DCPSREPO2->Kill ();
    $DCPSREPO1->Kill ();
    exit 1;
}

# Fire up the monitor process.

$Monitor->Spawn ();

# Fire up the subsystems.

$System1->Spawn ();
$System2->Spawn ();
$System3->Spawn ();

# Wait up to 5 minutes for test to complete.

$status = $Monitor->WaitKill (300);
if ($status != 0) {
    print STDERR "ERROR: monitor returned $status\n";
}
$failed += $status;

# And it can, in the worst case, take up to a minute to shut down the rest.

$status = $System1->WaitKill (15);
if ($status != 0) {
    print STDERR "ERROR: system 1 returned $status\n";
}
$failed += $status;

$status = $System2->WaitKill (15);
if ($status != 0) {
    print STDERR "ERROR: system 2 returned $status\n";
}
$failed += $status;

$status = $System3->WaitKill (15);
if ($status != 0) {
    print STDERR "ERROR: system 3 returned $status\n";
}
$failed += $status;

$status = $DCPSREPO1->TerminateWaitKill(5);
if ($status != 0) {
    print STDERR "ERROR: DCPSInfoRepo 1 returned $status\n";
}
$failed += $status;

$status = $DCPSREPO2->TerminateWaitKill(5);
if ($status != 0) {
    print STDERR "ERROR: DCPSInfoRepo 2 returned $status\n";
}
$failed += $status;

$status = $DCPSREPO3->TerminateWaitKill(5);
if ($status != 0) {
    print STDERR "ERROR: DCPSInfoRepo 3 returned $status\n";
}
$failed += $status;

# Clean up.

unlink $dcpsrepo1_ior;
unlink $dcpsrepo2_ior;
unlink $dcpsrepo3_ior;

# Report results.

if ($failed == 0) {
  print "test PASSED.\n";
}
else {
  print STDERR "test FAILED.\n";
}

exit $status;

