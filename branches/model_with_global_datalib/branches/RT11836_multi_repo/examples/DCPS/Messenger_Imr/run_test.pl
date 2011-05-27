eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlACE::Run_Test;
use Sys::Hostname;

my $useImr = 1;
if ($ARGV[0] eq 'noImr') {
    $useImr = 0;
}

my $status = 0;

my $opts =  "-ORBSvcConf ../../tcp.conf";
my $pub_opts = "$opts -DCPSConfigFile pub.ini -orbendpoint iiop://:12345";
my $sub_opts = "$opts -DCPSConfigFile sub.ini";

#my $OBJ_REF_STYLE = "-orbobjrefstyle url";
my $domains_file = PerlACE::LocalFile ("domain_ids");
my $dcpsrepo_ior = PerlACE::LocalFile ("repo.ior");

my $implrepo_ior = PerlACE::LocalFile ("imr.ior");
my $activator_ior = PerlACE::LocalFile ("activator.ior");
my $imr_init_ref = "-ORBInitRef ImplRepoService=file://$implrepo_ior";
my $implrepo_server = "$ENV{TAO_ROOT}/orbsvcs/ImplRepo_Service/ImplRepo_Service";
my $imr_activator = "$ENV{TAO_ROOT}/orbsvcs/ImplRepo_Service/ImR_Activator";
my $tao_imr = "$ENV{ACE_ROOT}/bin/tao_imr";
my $RepoOpts = "-NOBITS -o $dcpsrepo_ior -d $domains_file $OBJ_REF_STYLE";
if ($useImr == 1) {
    $RepoOpts = $RepoOpts . " -ORBuseimr 1 $imr_init_ref";
}

my $ImR = new PerlACE::Process ($implrepo_server, "-o $implrepo_ior $OBJ_REF_STYLE -orbendpoint iiop://:12346");
my $Act = new PerlACE::Process ($imr_activator, "-o $activator_ior $imr_init_ref $OBJ_REF_STYLE -orbendpoint iiop://:12347");
my $DCPSREPO = new PerlACE::Process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo", $RepoOpts);
my $imr_util = new PerlACE::Process ("$tao_imr");
my $Subscriber = new PerlACE::Process ("subscriber", " $sub_opts");
my $Publisher = new PerlACE::Process ("publisher", " $pub_opts");

# We want the tao_imr executable to be found exactly in the path
# given, without being modified by the value of -ExeSubDir.
# So, we tell its Process object to ignore the setting of -ExeSubDir.
$imr_util->IgnoreExeSubDir(1);

sub CleanupOutput {
    unlink $dcpsrepo_ior;
    unlink $implrepo_ior;
    unlink $activator_ior;
}

sub RunImRUtil {
    my $cmd = shift;
    $imr_util->Arguments("$imr_init_ref $cmd");
    print ">>> " . $imr_util->CommandLine() . "\n";
    return $imr_util->SpawnWaitKill(5);
}

sub SpawnWait {
    my $process = shift;
    my $file = shift;
    my $timeout = shift;

    print ">>> " . $process->CommandLine() . "\n";
    $process->Spawn();
    my $ret = PerlACE::waitforfile_timed($file, $timeout);
    if ($ret == -1) {
	print STDERR "ERROR: Cannot find file <$file>\n";
    }
    return $ret;
}

CleanupOutput();

if ($useImr == 1) {
    if (SpawnWait($ImR, $implrepo_ior, 10) != 0) {
	$ImR->Kill();
	exit 1;
    }

    if (SpawnWait($Act, $activator_ior, 10) != 0) {
	$ImR->Kill();
	$Act->Kill();
	exit 1;
    }
}

#instead of using tao_imr add, we'll use tao_imr update, because
#we want to run the server once to generate the ior file.

if (SpawnWait($DCPSREPO, $dcpsrepo_ior, 30) != 0) {
    $ImR->Kill();
    $DCPSREPO->Kill ();
    exit 1;
}

RunImRUtil("shutdown InfoRepo");
# The Info Repo can be killed once the IOR has been generated.
$DCPSREPO->Kill();

# Note : If the server registers itself, then it won't set the
# activator name. If we don't set it here, then the activator
# won't be able to start the server.
my $actname = hostname;
my $repoExe = $DCPSREPO->Executable();
RunImRUtil("update InfoRepo -l $actname -c \"$repoExe $RepoOpts\"");

RunImRUtil("list -v");

# The publisher invocation should incarnate the InfoRepo.
print $Publisher->CommandLine() . "\n";
$Publisher->Spawn ();

print $Subscriber->CommandLine() . "\n";
$Subscriber->Spawn ();

$PublisherResult = $Publisher->WaitKill (30);
if ($PublisherResult != 0) {
    print STDERR "ERROR: publisher returned $PublisherResult \n";
    $status = 1;
}

$SubscriberResult = $Subscriber->WaitKill (15);
if ($SubscriberResult != 0) {
    print STDERR "ERROR: subscriber returned $SubscriberResult \n";
    $status = 1;
}

$Act->Kill();
RunImRUtil("shutdown InfoRepo");
$DCPSREPO->Kill();
$ImR->Kill();

CleanupOutput();

if ($status == 0) {
  print "test PASSED.\n";
} else {
  print STDERR "test FAILED.\n";
}

exit $status;
