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
use strict;

my $status = 0;

my $pub_opts = "";
my $sub_opts = "";
my $reliable = 1;

my $test = new PerlDDS::TestFramework();

# let TestFramework handle ini file, but also need to identify that
# we are using a non-reliable transport
if ($test->flag('udp')) {
    $reliable = 0;
}
# cannot use default ini for multicast_async
elsif ($test->flag('multicast_async')) {
    $pub_opts .= "-DCPSConfigFile pub_multicast_async.ini ";
    $sub_opts .= "-DCPSConfigFile multicast.ini ";
}

$pub_opts .= " -r $reliable";
$sub_opts .= " -r $reliable";

$test->report_unused_flags();
# use tcp if no transport is set on command line
$test->default_transport("tcp");
$test->setup_discovery("-ORBDebugLevel 1 -ORBLogFile DCPSInfoRepo.log");

$test->process("subscriber", "subscriber", $sub_opts);
$test->process("publisher #1", "publisher", $pub_opts);
$test->process("publisher #2", "publisher", $pub_opts);

$test->start_process("publisher #1");
$test->start_process("publisher #2");
$test->start_process("subscriber");

# ignore this issue that is already being tracked in redmine
$test->ignore_error("(Redmine Issue# 1446)");
exit $test->finish(100);
