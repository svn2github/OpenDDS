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

$opts = new PerlACE::ConfigList->check_config ('STATIC')
    ? '' : '-ORBSvcConf ../../tcp.conf';

$pub_opts = "$opts -DCPSConfigFile pub.ini";
$sub_opts = "$opts -DCPSConfigFile sub.ini";

$domains_file = "domain_ids";
$dcpsrepo_ior = "repo.ior";
$repo_bit_opt = $opts eq '' ? '' : '-ORBSvcConf tcp.conf';

unlink $dcpsrepo_ior;

$DCPSREPO = PerlDDS::create_process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                                    "$repo_bit_opt -o $dcpsrepo_ior -d $domains_file");
$Subscriber = PerlDDS::create_process ("subscriber", " $sub_opts");
$Publisher = PerlDDS::create_process ("publisher", " $pub_opts");
$Monitor1 = PerlDDS::create_process ("monitor", " $opts -l 7");
$Monitor2 = PerlDDS::create_process ("monitor", " $opts -u");

$data_file = "test_run.data";
unlink $data_file;

print $DCPSREPO->CommandLine() . "\n";
print $Publisher->CommandLine() . "\n";
print $Subscriber->CommandLine() . "\n";
print $Monitor1->CommandLine() . "\n";
print $Monitor2->CommandLine() . "\n";


open (OLDOUT, ">&STDOUT");
open (STDOUT, ">$data_file") or die "can't redirect stdout: $!";
open (OLDERR, ">&STDERR");
open (STDERR, ">&STDOUT") or die "can't redirect stderror: $!";

$DCPSREPO->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}

$Monitor1->Spawn ();

$Publisher->Spawn ();
$Subscriber->Spawn ();
 
sleep (15);

$Monitor2->Spawn ();

$MonitorResult = $Monitor1->WaitKill (300);
if ($MonitorResult != 0) {
    print STDERR "ERROR: Monitor1 returned $MonitorResult \n";
    $status = 1;
}
$MonitorResult = $Monitor2->WaitKill (300);
if ($MonitorResult != 0) {
    print STDERR "ERROR: Monitor2 returned $MonitorResult \n";
    $status = 1;
}

$SubscriberResult = $Subscriber->WaitKill (300);
if ($SubscriberResult != 0) {
    print STDERR "ERROR: subscriber returned $SubscriberResult \n";
    $status = 1;
}

$PublisherResult = $Publisher->WaitKill (300);
if ($PublisherResult != 0) {
    print STDERR "ERROR: publisher returned $PublisherResult \n";
    $status = 1;
}

$ir = $DCPSREPO->TerminateWaitKill(5);
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

if ($status == 0) {
  print "test PASSED.\n";
} else {
  print STDERR "test FAILED.\n";
}

exit $status;
