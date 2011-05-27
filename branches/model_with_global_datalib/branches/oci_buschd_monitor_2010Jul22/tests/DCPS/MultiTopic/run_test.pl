eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
     & eval 'exec perl -S $0 $argv:q'
     if 0;

# -*- perl -*-

use lib "$ENV{ACE_ROOT}/bin";
use lib "$ENV{DDS_ROOT}/bin";
use PerlACE::Run_Test;
use PerlDDS::Run_Test;
use strict;


my $dcpsrepo_ior = "repo.ior";
unlink $dcpsrepo_ior;

my $DCPSREPO = PerlDDS::create_process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                                        "-NOBITS -o $dcpsrepo_ior");

$DCPSREPO->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}

my $use_svc_config = !new PerlACE::ConfigList->check_config ('STATIC');
my $opts = $use_svc_config ? "-ORBSvcConf $ENV{DDS_ROOT}/tests/tcp.conf" : '';

my $TEST = PerlDDS::create_process ('MultiTopicTest',
                                    "-DCPSConfigFile dcps.ini -DCPSBit 0 ".
                                    $opts);
my $result = $TEST->SpawnWaitKill(60);
if ($result != 0) {
  print STDERR "ERROR: test returned $result\n";
}

$DCPSREPO->TerminateWaitKill(5);

exit (($result == 0) ? 0 : 1);
