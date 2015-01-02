eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

my @original_ARGV = @ARGV;

use Env (DDS_ROOT);
use lib "$DDS_ROOT/bin";
use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlDDS::Run_Test;
use strict;

my $status = 0;

my $test = new PerlDDS::TestFramework();

$test->{dcps_debug_level} = 4;
$test->{dcps_transport_debug_level} = 2;
# will manually set -DCPSConfigFile
$test->{add_transport_config} = 0;
my $dbg_lvl = ' -DCPSDebugLevel 1 -DCPSTransportDebugLevel 1 ';
my $pub_opts = "$dbg_lvl";
my $sub_opts = "$dbg_lvl";
my $repo_bit_opt = "";
my $is_rtps_disc = 0;
my $DCPSREPO;

my $thread_per_connection = "";
if ($test->flag('thread_per')) {
    $thread_per_connection = " -p ";
}

#
# stub parameters
#
my $stubPubSideHost  = "localhost";
my $stubPubSidePort = PerlACE::random_port();;
my $stubSubSideHost = "localhost";
my $stubSubSidePort = PerlACE::random_port();;
my $stubKillDelay = 4;
my $stubKillCount = 2;

#
# stub command and arguments.
#
# my $socatCmd = "/usr/bin/socat";
# my $socatArgs = " -d -d TCP-LISTEN:$socatPubSidePort,reuseaddr " .
#                 "TCP:stubSubSideHost:$stubSubSidePort";
my $stubCmd = "stub";
my $stubArgs = " -p:$stubPubSideHost:$stubPubSidePort -s:$stubSubSideHost:$stubSubSidePort";
# $stubArgs .= " -v ";

#
# generate ini file for subscriber
#
my $resub = "[common]\n" .
    "DCPSGlobalTransportConfig=\$file\n\n" .
    "[transport/t1]\n" .
    "transport_type=tcp\n" .
    "local_address=$stubSubSideHost:$stubSubSidePort\n" .
    "pub_address=$stubPubSideHost:$stubPubSidePort\n";

open(my $fh, '>', 'resub.ini');
print $fh $resub;
close $fh;

$pub_opts .= ' -DCPSConfigFile repub.ini';
$sub_opts .= ' -DCPSConfigFile resub.ini';


$pub_opts .= $thread_per_connection;

$test->setup_discovery("-NOBITS -ORBDebugLevel 1 -ORBLogFile DCPSInfoRepo.log " .
                       "$repo_bit_opt");

$test->process("publisher", "publisher", $pub_opts);
$test->process("subscriber", "subscriber", $sub_opts);
my $stub = PerlDDS::create_process($stubCmd, $stubArgs);

$test->start_process("subscriber");
print $stub->CommandLine() . "\n";
$stub->Spawn();
$test->start_process("publisher");

while ($stubKillCount > 0) {
  sleep($stubKillDelay);
  $stub->Kill();
  $stub->Spawn();
  $stubKillCount--;
}

# ignore this issue that is already being tracked in redmine
$test->ignore_error("(Redmine Issue# 1446)");

# Ignore normal disconnect/reconnect messages
$test->ignore_error("Failed to connect. connect: Connection refused");
$test->ignore_error("Unrecoverable problem with data link detected");

# start killing processes in 60 seconds
my $fin = $test->finish(60);
$stub->Kill();
exit $fin;
