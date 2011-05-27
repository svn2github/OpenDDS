eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlACE::Run_Test;

$status = 0;

PerlACE::add_lib_path('../MultiTopicTypes');
PerlACE::add_lib_path('../common');

$subscriber1_completed = PerlACE::LocalFile ("T1_subscriber_finished.txt");
$subscriber2_completed = PerlACE::LocalFile ("T2_subscriber_finished.txt");
$subscriber3_completed = PerlACE::LocalFile ("T3_subscriber_finished.txt");
#$subscriber_ready = PerlACE::LocalFile ("subscriber_ready.txt");

$publisher1_completed = PerlACE::LocalFile ("T1_publisher_finished.txt");
$publisher2_completed = PerlACE::LocalFile ("T2_publisher_finished.txt");
$publisher3_completed = PerlACE::LocalFile ("T3_publisher_finished.txt");
#$publisher_ready = PerlACE::LocalFile ("publisher_ready.txt");

unlink $subscriber1_completed; 
unlink $subscriber2_completed; 
unlink $subscriber3_completed; 
#unlink $subscriber_ready; 
unlink $publisher1_completed; 
unlink $publisher2_completed; 
unlink $publisher3_completed; 
#unlink $publisher_ready; 

# single reader with single instances test
$multiple_instance=0;
$num_samples_per_reader=10;
$num_readers=1;
$use_take=0;
$use_udp = 0;

$arg_idx = 0;

if ($ARGV[0] eq 'udp') {
  $use_udp = 1;
  $arg_idx = 1;
}


# multiple instances test
if ($ARGV[$arg_idx] eq 'mi') { 
  $multiple_instance=1;
  $num_samples_per_reader=10;
  $num_readers=1; 
}
# multiple datareaders with single instance test
elsif ($ARGV[$arg_idx] eq 'mr') {  
  $multiple_instance=0;
  $num_samples_per_reader=5;
  $num_readers=2; 
}
# multiple datareaders with multiple instances test
elsif ($ARGV[$arg_idx] eq 'mri') {  
  $multiple_instance=1;
  $num_samples_per_reader=4;
  $num_readers=3; 
}
# multiple datareaders with multiple instances test
elsif ($ARGV[$arg_idx] eq 'mrit') {  
  $multiple_instance=1;
  $num_samples_per_reader=4;
  $num_readers=3; 
  $use_take=1;
}
elsif ($ARGV[$arg_idx] eq '') { 
  #default test - single datareader single instance.
}
else {
  print STDERR "ERROR: invalid parameter $ARGV[$arg_idx] $arg_idx\n";
  exit 1;
}

$domains_file = PerlACE::LocalFile ("domain_ids");
$dcpsrepo_ior = PerlACE::LocalFile ("repo.ior");

unlink $dcpsrepo_ior; 

$DCPSREPO = new PerlACE::Process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                             "-ORBSvcConf ../../tcp.conf -ORBDebugLevel 1 "
                           . "-o $dcpsrepo_ior");


print $DCPSREPO->CommandLine(), "\n";
# test multiple cases
$sub1_parameters = "-t 1" ;
$sub2_parameters = "-t 2" ;
$sub3_parameters = "-t 3" ;

$Subscriber1 = new PerlACE::Process ("subscriber", $sub1_parameters);
print $Subscriber1->CommandLine(), "\n";

$Subscriber2 = new PerlACE::Process ("subscriber", $sub2_parameters);
print $Subscriber2->CommandLine(), "\n";

$Subscriber3 = new PerlACE::Process ("subscriber", $sub3_parameters);
print $Subscriber3->CommandLine(), "\n";

$pub1_parameters = " -t 1 " ;
$pub2_parameters = " -t 2 " ;
$pub3_parameters = " -t 3 " ;

$Publisher1 = new PerlACE::Process ("publisher", $pub1_parameters);
print $Publisher1->CommandLine(), "\n";

$Publisher2 = new PerlACE::Process ("publisher", $pub2_parameters);
print $Publisher2->CommandLine(), "\n";

$Publisher3 = new PerlACE::Process ("publisher", $pub3_parameters);
print $Publisher3->CommandLine(), "\n";


$DCPSREPO->Spawn ();

if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}

$Publisher1->Spawn ();
$Publisher2->Spawn ();
$Publisher3->Spawn ();

$Subscriber1->Spawn ();
$Subscriber2->Spawn ();
$Subscriber3->Spawn ();

$Publisher1Result = $Publisher1->WaitKill (300);
$Publisher2Result = $Publisher2->WaitKill (10);
$Publisher3Result = $Publisher3->WaitKill (10);

if ($Publisher1Result != 0) {
    print STDERR "ERROR: publisher 1 returned $PublisherResult1 \n";
    $status = 1;
}
if ($Publisher2Result != 0) {
    print STDERR "ERROR: publisher 2 returned $PublisherResult2 \n";
    $status = 1;
}
if ($Publisher3Result != 0) {
    print STDERR "ERROR: publisher 3 returned $PublisherResult3 \n";
    $status = 1;
}

$Subscriber1Result = $Subscriber1->WaitKill (30);
if ($Subscriber1Result != 0) {
    print STDERR "ERROR: subscriber 1 returned $Subscriber1Result \n";
    $status = 1;
}

$Subscriber2Result = $Subscriber2->WaitKill (10);
if ($Subscriber2Result != 0) {
    print STDERR "ERROR: subscriber 2 returned $Subscriber2Result \n";
    $status = 1;
}

$Subscriber3Result = $Subscriber3->WaitKill (10);
if ($Subscriber3Result != 0) {
    print STDERR "ERROR: subscriber 3 returned $Subscriber3Result \n";
    $status = 1;
}


$ir = $DCPSREPO->TerminateWaitKill(5);

if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}

unlink $subscriber1_completed; 
unlink $subscriber2_completed; 
unlink $subscriber3_completed; 
#unlink $subscriber_ready; 
unlink $publisher1_completed; 
unlink $publisher2_completed; 
unlink $publisher3_completed; 
#unlink $publisher_ready; 

if ($status == 0) {
  print "test PASSED.\n";
}
else {
  print STDERR "test FAILED.\n";
}

exit $status;
