eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
     & eval 'exec perl -S $0 $argv:q'
     if 0;

# $Id$
# -*- perl -*-

use lib '../../../../../bin';
use PerlACE::Run_Test;

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
my $publisherHost = "localhost" ;
my $publisherPort = 10001 + PerlACE::uniqueid ();

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
my $subscriberArgs = "-p $publisherId:$publisherHost:$publisherPort "
                   . "-s $subscriberId:$subscriberHost:$subscriberPort "
                   . "-n $iterations " ;

#
# Publisher command and arguments.
#
my $publisherCmd  = "./simple_publisher" ;
my $publisherArgs = "-p $publisherId:$publisherHost:$publisherPort "
                  . "-s $subscriberId:$subscriberHost:$subscriberPort "
                   . "-n $iterations " ;

#
# Create the test objects.
#
my $subscriber = new PerlACE::Process( $subscriberCmd, $subscriberArgs) ;
my $publisher  = new PerlACE::Process( $publisherCmd,  $publisherArgs) ;

#
# Fire up the subscriber first.
#
$subscriber->Spawn() ;
if (PerlACE::waitforfile_timed ($subreadyfile, 5) == -1) {
    print STDERR "ERROR: waiting for subscriber file\n";
    $subscriber->Kill ();
    exit 1;
}

$publisher->Spawn() ;

#
# Wait for the test to finish, or kill the processes.
#
die "*** ERROR: Subscriber timed out - $!" if $subscriber->WaitKill( $testTime) ;
die "*** ERROR: Publisher timed out - $!"  if $publisher->WaitKill( 5) ;

unlink $subreadyfile;

exit 0 ;
