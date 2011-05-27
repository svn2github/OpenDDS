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

use Getopt::Long qw( :config bundling) ;

#
# Test parameters.
#
my $testTime   = 60 ;
my $iterations = 200 ;

#
# Publisher parameters.
#
my $publisherId = 1 ;
my $publisherHost = "224.0.0.1" ;
my $publisherPort = 10001 + PerlACE::uniqueid ();
my $localInterface = "localhost";

#
# Subscriber parameters.
#
my $subscriberId = 2 ;
my $subscriberHost = "localhost" ;
my $subscriberPort = 10002 + PerlACE::uniqueid ();
my $subreadyfile = "subready.txt";
unlink $subreadyfile;

#
# Parse the command line.
#
GetOptions(
  "iterations|n=i" => \$iterations, 
  "timeout|t=i"    => \$testTime,
  "publisher|p=i"  => \$publisherId,
  "subscriber|s=i" => \$subscriberId,
  "phost|h=s"      => \$publisherHost,
  "shost|o=s"      => \$subscriberHost,
  "pport|x=i"      => \$publisherPort,
  "sport|y=i"      => \$subscriberPort,
) ;

#
# Subscriber command and arguments.
#
my $subscriberCmd  = "./simple_subscriber" ;
my $subscriberArgs = "-ORBSvcConf mcast.conf -p $publisherId:$publisherHost:$publisherPort "
                   . "-s $subscriberId:$subscriberHost:$subscriberPort "
                   . "-n $iterations " ;

#
# Publisher command and arguments.
#
my $publisherCmd  = "./simple_publisher" ;
my $publisherArgs = "-ORBSvcConf mcast.conf -p $publisherId:$publisherHost:$publisherPort "
                  . "-s $subscriberId:$localInterface "
                   . "-n $iterations " ;

#
# Create the test objects.
#
$subscriber = PerlDDS::create_process( $subscriberCmd, $subscriberArgs) ;
$publisher  = PerlDDS::create_process( $publisherCmd,  $publisherArgs) ;

#
# Fire up the subscriber first.
#
print $subscriber->CommandLine() . "\n";
$subscriber->Spawn() ;
if (PerlACE::waitforfile_timed ($subreadyfile, 15) == -1) {
    print STDERR "ERROR: waiting for subscriber file\n";
    $subscriber->Kill ();
    exit 1;
}

print $publisher->CommandLine() . "\n";
$publisher->Spawn() ;

#
# Wait for the test to finish, or kill the processes.
#
die "*** ERROR: Subscriber timed out - $!" if $subscriber->WaitKill( $testTime) ;
die "*** ERROR: Publisher timed out - $!"  if $publisher->WaitKill( 5) ;

unlink $subreadyfile;

exit 0 ;

