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

PerlDDS::add_lib_path('../TypeNoKeyBounded');


# single reader with single instances test
$num_messages=500;
$data_size=13;
$num_writers=1;
$num_readers=1;
$num_msgs_btwn_rec=20;
$pub_writer_id=0;
$repo_bit_conf = "-NOBITS ";
$app_bit_conf = "-DCPSBit 0 ";
$copy_sample=0;
$use_svc_config = !new PerlACE::ConfigList->check_config ('STATIC');

if ($ARGV[0] ne '') {
    $data_size = $ARGV[0];
}

if ($ARGV[1] ne '') {
    $copy_sample = $ARGV[1];
}

if ($ARGV[2] ne '') {
    $num_messages = $ARGV[2];
}

if ($ARGV[3] eq 'bit') {
  $repo_bit_conf = $use_svc_config ? "-ORBSvcConf ../../tcp.conf" : '';
  $app_bit_conf = $use_svc_config ? "-ORBSvcConf ../../tcp.conf" : '';
}
elsif ($ARGV[3] eq '' or $ARGV[3] eq 'nobit') {
  # default test with bit off
  $repo_bit_conf .= $use_svc_config ? "-ORBSvcConf ../../tcp.conf" : '';
  $app_bit_conf .= $use_svc_config ? "-ORBSvcConf ../../tcp.conf" : '';
}
else {
  print STDERR "ERROR: invalid parameter $ARGV[3] \n";
  exit 1;
}

# need $num_msgs_btwn_rec unread samples plus 20 for good measure
# (possibly allocated by not yet queue by the transport because of greedy read).
$num_samples=$num_msgs_btwn_rec + 20;

$domains_file = "domain_ids";
$dcpsrepo_ior = "repo.ior";

unlink $dcpsrepo_ior;

$DCPSREPO = PerlDDS::create_process ("$ENV{DDS_ROOT}/bin/DCPSInfoRepo",
                             "$repo_bit_conf -o $dcpsrepo_ior"
                             . " -d $domains_file");


print $DCPSREPO->CommandLine(), "\n";

$svc_config = ($use_svc_config && $app_bit_conf eq '')
    ? " -ORBSvcConf ../../tcp.conf " : '';
$sub_parameters = "$app_bit_conf -DCPSConfigFile conf.ini -p $num_writers"
#              . " -DCPSDebugLevel 6"
   . "$svc_config"
              . " -i $num_msgs_btwn_rec"
              . " -n $num_messages -d $data_size"
              . " -msi $num_samples -mxs $num_samples"
              . " -c $copy_sample";
#use -msi $num_messages to avoid rejected samples
#use -mxs $num_messages to avoid using the heap
#   (could be less than $num_messages but I am not sure of the limit).

$Subscriber = PerlDDS::create_process ("subscriber", $sub_parameters);
print $Subscriber->CommandLine(), "\n";


#NOTE: above 1000 queue samples does not give any better performance.
$pub_parameters = "$app_bit_conf -DCPSConfigFile conf.ini -p 1 -i $pub_writer_id"
#              . " -DCPSDebugLevel 6"
   . "$svc_config"
              . " -n $num_messages -d $data_size"
              . " -msi 1000 -mxs 1000";

$Publisher = PerlDDS::create_process ("publisher", $pub_parameters);
print $Publisher->CommandLine(), "\n";


$DCPSREPO->Spawn ();

if (PerlACE::waitforfile_timed ($dcpsrepo_ior, 30) == -1) {
    print STDERR "ERROR: waiting for DCPSInfo IOR file\n";
    $DCPSREPO->Kill ();
    exit 1;
}


$Publisher->Spawn ();

$Subscriber->Spawn ();

$PublisherResult = $Publisher->WaitKill (1200);

if ($PublisherResult != 0) {
    print STDERR "ERROR: publisher returned $PublisherResult \n";
    $status = 1;
}
$SubscriberResult = $Subscriber->WaitKill (1200);

if ($SubscriberResult != 0) {
    print STDERR "ERROR: subscriber returned $SubscriberResult \n";
    $status = 1;
}


$ir = $DCPSREPO->TerminateWaitKill(10);

if ($ir != 0) {
    print STDERR "ERROR: DCPSInfoRepo returned $ir\n";
    $status = 1;
}


exit $status;
