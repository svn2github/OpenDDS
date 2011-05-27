eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlACE::Run_Test;

$status = 0;
$use_svc_config = !new PerlACE::ConfigList->check_config ('STATIC');

$opts = $use_svc_config ? "-ORBSvcConf tcp.conf" : '';
$repo_bit_opt = $opts;
$pub_opts = "$opts -DCPSConfigFile pub.ini";
$sub_opts = "$opts -DCPSConfigFile sub.ini";

if ($ARGV[0] eq 'udp') {
    $opts .= ($use_svc_config ? " -ORBSvcConf udp.conf " : '') . "-t udp";
    $pub_opts = "$opts -DCPSConfigFile pub_udp.ini";
    $sub_opts = "$opts -DCPSConfigFile sub_udp.ini";
}
elsif ($ARGV[0] eq 'mcast') {
    $opts .= ($use_svc_config ? " -ORBSvcConf mcast.conf " : '') . "-t mcast";
    $pub_opts = "$opts -DCPSConfigFile pub_mcast.ini";
    $sub_opts = "$opts -DCPSConfigFile sub_mcast.ini";
}
elsif ($ARGV[0] eq 'reliable_mcast') {
    $opts .= ($use_svc_config ? " -ORBSvcConf reliable_mcast.conf " : '')
        . "-t reliable_mcast";
    $pub_opts = "$opts -DCPSConfigFile pub_reliable_mcast.ini";
    $sub_opts = "$opts -DCPSConfigFile sub_reliable_mcast.ini";
}
elsif ($ARGV[0] eq 'default_tcp') {
    $opts .= " -t default_tcp";
    $pub_opts = "$opts";
    $sub_opts = "$opts";
}
elsif ($ARGV[0] eq 'default_udp') {
    $opts .= ($use_svc_config ? " -ORBSvcConf udp.conf " : '')
	. " -t default_udp";
    $pub_opts = "$opts";
    $sub_opts = "$opts";
}
elsif ($ARGV[0] eq 'default_mcast') {
    $opts .= ($use_svc_config ? " -ORBSvcConf mcast.conf " : '');
    $pub_opts = "$opts -t default_mcast_pub";
    $sub_opts = "$opts -t default_mcast_sub";
}
elsif ($ARGV[0] ne '') {
    print STDERR "ERROR: invalid test case\n";
    exit 1;
}

$domains_file = PerlACE::LocalFile ("domain_ids");
$dcpsrepo_ior = PerlACE::LocalFile ("repo.ior");

unlink $dcpsrepo_ior;

$DCPSREPO = new PerlACE::Process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
				  "$repo_bit_opt -o $dcpsrepo_ior -d $domains_file");
$Subscriber = new PerlACE::Process ("subscriber", " $sub_opts");
$Publisher = new PerlACE::Process ("publisher", " $pub_opts");

print $DCPSREPO->CommandLine() . "\n";
$DCPSREPO->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}

print $Publisher->CommandLine() . "\n";
$Publisher->Spawn ();

print $Subscriber->CommandLine() . "\n";
$Subscriber->Spawn ();


$PublisherResult = $Publisher->WaitKill (300);
if ($PublisherResult != 0) {
    print STDERR "ERROR: publisher returned $PublisherResult \n";
    $status = 1;
}

$SubscriberResult = $Subscriber->WaitKill (15);
if ($SubscriberResult != 0) {
    print STDERR "ERROR: subscriber returned $SubscriberResult \n";
    $status = 1;
}

$ir = $DCPSREPO->TerminateWaitKill(5);
if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}

unlink $dcpsrepo_ior;

if ($status == 0) {
  print "test PASSED.\n";
} else {
  print STDERR "test FAILED.\n";
}

exit $status;
