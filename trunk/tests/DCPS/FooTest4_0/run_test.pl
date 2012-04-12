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

$status = 0;

PerlDDS::add_lib_path('../FooType4');
PerlDDS::add_lib_path('../common');

if ($ARGV[0] eq '') {
  #default test - single datareader single instance.
}
else {
  print STDERR "ERROR: invalid parameter $ARGV[0] \n";
  exit 1;
}

$dcpsrepo_ior = "repo.ior";

unlink $dcpsrepo_ior;
unlink $pub_id_file;

# test multiple cases
$parameters = "-DCPSLivelinessFactor 300 -z " ;


$DCPSREPO = PerlDDS::create_process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                                     #. " -ORBDebugLevel 1 "
                                     "-o $dcpsrepo_ior ");

$FooTest4 = PerlDDS::create_process ("FooTest4_0", $parameters);
print $FooTest4->CommandLine(), "\n";


$DCPSREPO->Spawn ();

if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for Info Repo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}


$FooTest4->Spawn ();

$result = $FooTest4->WaitKill (120);

if ($result != 0) {
    print STDERR "ERROR: FooTest4 returned $result \n";
    $status = 1;
}


$ir = $DCPSREPO->TerminateWaitKill(5);

if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}

exit $status;
