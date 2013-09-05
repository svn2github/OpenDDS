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

my $status = 0;

my $logging = "-ORBDebugLevel 1 -ORBVerboseLogging 1 " .
    "-DCPSTransportDebugLevel 6";
my $pub_opts = "$logging -ORBLogFile pub.log ";
my $sub_opts = "$logging -ORBLogFile sub.log ";
my $repo_bit_opt = '';

if ($ARGV[0] eq 'udp') {
    $repo_bit_opt = '-NOBITS';
    $pub_opts .= "-DCPSBit 0 -DCPSConfigFile udp.ini";
    $sub_opts .= "-DCPSBit 0 -DCPSConfigFile udp.ini";
}
elsif ($ARGV[0] eq 'multicast') {
    $repo_bit_opt = '-NOBITS';
    $pub_opts .= "-DCPSBit 0 -DCPSConfigFile pub_multicast.ini";
    $sub_opts .= "-DCPSBit 0 -DCPSConfigFile sub_multicast.ini";
}
elsif ($ARGV[0] eq 'nobits') {
    $repo_bit_opt = '-NOBITS';
    $pub_opts .= ' -DCPSBit 0 -DCPSConfigFile tcp.ini';
    $sub_opts .= ' -DCPSBit 0 -DCPSConfigFile tcp.ini';
}
elsif ($ARGV[0] ne '') {
    print STDERR "ERROR: invalid test case\n";
    exit 1;
}
else {
    $pub_opts .= ' -DCPSConfigFile tcp.ini';
    $sub_opts .= ' -DCPSConfigFile tcp.ini';
}

my $dcpsrepo_ior = "repo.ior";

unlink $dcpsrepo_ior;
unlink <*.log>;

my $DCPSREPO = PerlDDS::create_process("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                                       "-ORBDebugLevel 10 " .
                                       "-ORBLogFile DCPSInfoRepo.log " .
                                       "$repo_bit_opt -o $dcpsrepo_ior");

my $Subscriber = PerlDDS::create_process("subscriber", $sub_opts);
my $Publisher = PerlDDS::create_process("publisher", $pub_opts);

my $pub2_opts = $pub_opts;
$pub2_opts =~ s/pub\.log/pub2.log/;
my $Publisher2 = PerlDDS::create_process("publisher", $pub2_opts);

print $DCPSREPO->CommandLine() . "\n";
$DCPSREPO->Spawn();
if (PerlACE::waitforfile_timed($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill();
    exit 1;
}

print $Publisher->CommandLine() . "\n";
$Publisher->Spawn();

print $Publisher2->CommandLine() . "\n";
$Publisher2->Spawn();

print $Subscriber->CommandLine() . "\n";
$Subscriber->Spawn();


my $PublisherResult = $Publisher->WaitKill(60);
if ($PublisherResult != 0) {
    print STDERR "ERROR: publisher returned $PublisherResult\n";
    $status = 1;
}

my $Publisher2Result = $Publisher2->WaitKill(10);
if ($Publisher2Result != 0) {
    print STDERR "ERROR: publisher #2 returned $PublisherResult\n";
    $status = 1;
}

my $SubscriberResult = $Subscriber->WaitKill(15);
if ($SubscriberResult != 0) {
    print STDERR "ERROR: subscriber returned $SubscriberResult\n";
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
  print STDERR "test FAILED.\n";
}

exit $status;
