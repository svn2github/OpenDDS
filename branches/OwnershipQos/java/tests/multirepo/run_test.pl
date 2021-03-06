eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
     & eval 'exec perl -S $0 $argv:q'
     if 0;

# -*- perl -*-

use Env qw(ACE_ROOT JAVA_HOME DDS_ROOT);
use lib "$DDS_ROOT/bin";
use lib "$ACE_ROOT/bin";
use PerlDDS::Run_Test;
use PerlDDS::Process_Java;
use strict;

my $status = 0;
my $debug = '0';

foreach my $i (@ARGV) {
    if ($i eq '-debug') {
        $debug = '10';
    } 
}

my $opts = "-ORBSvcConf $DDS_ROOT/tests/DCPS/Messenger/tcp.conf";
my $debug_opt = ($debug eq '0') ? ''
    : "-ORBDebugLevel $debug -DCPSDebugLevel $debug";

my $master_opts = "$opts -ORBListenEndpoints iiop://127.0.0.1:12346 $debug_opt ".
    "-ORBLogFile master.log -DCPSConfigFile multirepo.ini";

my $slave_opts = "$opts -ORBListenEndpoints iiop://127.0.0.1:12347 $debug_opt ".
    "-ORBLogFile slave.log -DCPSConfigFile multirepo.ini";

my $dcpsrepo1_ior = "repo1.ior";
my $dcpsrepo2_ior = "repo2.ior";

my $domains1_file = "domain1_ids";
my $domains2_file = "domain2_ids";

unlink $dcpsrepo1_ior;
unlink $dcpsrepo2_ior;

my $DCPSREPO1 = PerlDDS::create_process ("$DDS_ROOT/bin/DCPSInfoRepo",
               "-DCPSDebugLevel 10 ".
               "-ORBListenEndpoints iiop://127.0.0.1:1111 -ORBDebugLevel 10 ".
               "-ORBLogFile DCPSInfoRepo.log $opts -o $dcpsrepo1_ior ".
               "-d $domains1_file");

my $DCPSREPO2 = PerlDDS::create_process ("$DDS_ROOT/bin/DCPSInfoRepo",
               "-DCPSDebugLevel 10 ".
               "-ORBListenEndpoints iiop://127.0.0.1:1112 -ORBDebugLevel 10 ".
               "-ORBLogFile DCPSInfoRepo.log $opts -o $dcpsrepo2_ior ".
               "-d $domains2_file");

PerlACE::add_lib_path ("$DDS_ROOT/java/tests/multirepo");

my $MASTER = new PerlDDS::Process_Java ("MultiRepoMaster", $master_opts);
my $SLAVE = new PerlDDS::Process_Java ("MultiRepoSlave", $slave_opts);

$DCPSREPO1->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo1_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO1->Kill ();
    exit 1;
}

$DCPSREPO2->Spawn ();
if (PerlACE::waitforfile_timed ($dcpsrepo2_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO2->Kill ();
    exit 1;
}

$MASTER->Spawn ();
$SLAVE->Spawn ();

my $MasterResult = $MASTER->WaitKill (300);
if ($MasterResult != 0) {
    print STDERR "ERROR: master returned $MasterResult \n";
    $status = 1;
}

my $SlaveResult = $SLAVE->WaitKill (300);
if ($SlaveResult != 0) {
    print STDERR "ERROR: slave returned $SlaveResult \n";
    $status = 1;
}

my $ir1 = $DCPSREPO1->TerminateWaitKill(5);
if ($ir1 != 0) {
    print STDERR "ERROR: DCPSInfoRepo1 returned $ir1\n";
    $status = 1;
}

my $ir2 = $DCPSREPO2->TerminateWaitKill(5);
if ($ir2 != 0) {
    print STDERR "ERROR: DCPSInfoRepo2 returned $ir2\n";
    $status = 1;
}

unlink $dcpsrepo1_ior;
unlink $dcpsrepo2_ior;

if ($status == 0) {
  print "test PASSED.\n";
} else {
  print STDERR "test FAILED.\n";
}

exit $status;
