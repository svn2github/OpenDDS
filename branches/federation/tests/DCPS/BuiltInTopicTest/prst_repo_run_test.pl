eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (DDS_ROOT);
use lib "$DDS_ROOT/bin";
use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use DDS_Run_Test;

$status = 0;

my $debug;
my $transportDebug;
my $debugFile;
# $debug = 10;
# $transportDebug = 10;
# $debugFile = "debug.out";

my $debugOpts = "";
$debugOpts .= "-DCPSDebugLevel $debug " if $debug;
$debugOpts .= "-DCPSTransportDebugLevel $transportDebug " if $transportDebug;
$debugOpts .= "-ORBLogFile $debugFile " if $debugFile and ($debug or $transportDebug);

$opts = new PerlACE::ConfigList->check_config ('STATIC')
    ? '' : '-ORBSvcConf ../../tcp.conf';

$dcpsrepo_ior = "repo.ior";
$repo_bit_opt = $opts eq '' ? '' : '-ORBSvcConf tcp.conf';

$opts         .= " " . $debugOpts if $debug or $transportDebug;
$repo_bit_opt .= " " . $debugOpts if $debug or $transportDebug;

$info_prst_file = "info.pr";
$num_messages = 60;
$pub_opts = "$opts -DCPSConfigFile pub.ini -n $num_messages";
$sub_opts = "$opts -DCPSConfigFile sub.ini -n $num_messages";
$SRV_PORT = PerlACE::random_port();

unlink $dcpsrepo_ior;
unlink $info_prst_file;
unlink $debugFile;

# If InfoRepo is running in persistent mode, use a
#  static endpoint (instead of transient)
$DCPSREPO = PerlDDS::create_process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                                    "$repo_bit_opt -o $dcpsrepo_ior "
                                    #. "-ORBDebugLevel 10 "
                                    . "-ORBSvcConf mySvc.conf "
                                    . "-orbendpoint iiop://:$SRV_PORT");
$Subscriber = PerlDDS::create_process ("subscriber", " $sub_opts");
$Publisher = PerlDDS::create_process ("publisher", " $pub_opts");
$Monitor1 = PerlDDS::create_process ("monitor", " $opts -l 7");
$Monitor2 = PerlDDS::create_process ("monitor", " $opts -u");

$data_file = "test_run_prst.data";
unlink $data_file;

open (OLDOUT, ">&STDOUT");
open (STDOUT, ">$data_file") or die "can't redirect stdout: $!";
open (OLDERR, ">&STDERR");
open (STDERR, ">&STDOUT") or die "can't redirect stderror: $!";

print "Spawning DCPSInfoRepo.\n";

print $DCPSREPO->CommandLine() . "\n";
$DCPSREPO->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}

print "Spawning first monitor.\n";

print $Monitor1->CommandLine() . "\n";
$Monitor1->Spawn ();

print "Spawning publisher.\n";

print $Publisher->CommandLine() . "\n";
$Publisher->Spawn ();

print "Spawning subscriber.\n";

print $Subscriber->CommandLine() . "\n";
$Subscriber->Spawn ();
 
sleep (15);

print "Killing first DCPSInfoRepo.\n";

$ir = $DCPSREPO->TerminateWaitKill(10);
if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}
unlink $dcpsrepo_ior;
 
print "Spawning second DCPSInfoRepo.\n";
print $DCPSREPO->CommandLine() . "\n";
$DCPSREPO->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}

sleep (15);

print "Spawning second monitor.\n";

print $Monitor2->CommandLine() . "\n";
$Monitor2->Spawn ();

$MonitorResult = $Monitor1->WaitKill (20);
if ($MonitorResult != 0) {
    print STDERR "ERROR: Monitor1 returned $MonitorResult \n";
    $status = 1;
}

$MonitorResult = $Monitor2->WaitKill (300);
if ($MonitorResult != 0) {
    print STDERR "ERROR: Monitor2 returned $MonitorResult \n";
    $status = 1;
}

$SubscriberResult = $Subscriber->TerminateWaitKill (10);
if ($SubscriberResult != 0) {
    print STDERR "ERROR: subscriber returned $SubscriberResult \n";
    $status = 1;
}

$PublisherResult = $Publisher->TerminateWaitKill (10);
if ($PublisherResult != 0) {
    print STDERR "ERROR: publisher returned $PublisherResult \n";
    $status = 1;
}

$ir = $DCPSREPO->TerminateWaitKill(10);
if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}

close (STDERR);
close (STDOUT);
open (STDOUT, ">&OLDOUT");
open (STDERR, ">&OLDERR");

unlink $dcpsrepo_ior;
#unlink $data_file;
unlink $info_prst_file;

if ($status == 0) {
  print "test PASSED.\n";
} else {
  print STDERR "test FAILED.\n";
}

exit $status;
