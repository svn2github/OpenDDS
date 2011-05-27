eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
     & eval 'exec perl -S $0 $argv:q'
     if 0;

# $Id$
# -*- perl -*-

use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";
use PerlACE::Run_Test;

#
# Test parameters.
#
my $testTime = 60 ;
my $debug    = 1 ;

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
# Subscriber command and arguments.
#
my $subscriberCmd  = "./simple_subscriber" ;
my $subscriberArgs = "-ORBSvcConf mcast.conf -p $publisherId:$publisherHost:$publisherPort "
                   . "-s $subscriberId:$subscriberHost:$subscriberPort " ;

#
# Publisher command and arguments.
#
my $publisherCmd  = "./simple_publisher" ;
my $publisherArgs = "-ORBSvcConf mcast.conf -p $publisherId:$publisherHost:$publisherPort "
                  . "-s $subscriberId:$localInterface " ;

#
# Create the test objects.
#
my $subscriber = new PerlACE::Process( $subscriberCmd, $subscriberArgs) ;
my $publisher  = new PerlACE::Process( $publisherCmd,  $publisherArgs) ;

#
# Fire up the subscriber first.
#
print $subscriber->CommandLine() . "\n" if $debug ;
$subscriber->Spawn() ;
if (PerlACE::waitforfile_timed ($subreadyfile, 5) == -1) {
    print STDERR "ERROR: waiting for subscriber file\n";
    $subscriber->Kill ();
    exit 1;
}

#
# Don't start the publisher for a few seconds.  We are not generating
# anything in the file system here to wait for, so just use a delay - yuk.
#
print $publisher->CommandLine() . "\n" if $debug ;
$publisher->Spawn() ;

#
# Wait for the test to finish, or kill the processes.
#
die "*** ERROR: Subscriber timed out - $!" if $subscriber->WaitKill( $testTime) ;
die "*** ERROR: Publisher timed out - $!"  if $publisher->WaitKill( 5) ;

unlink $subreadyfile;

exit 0 ;
