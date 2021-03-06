eval '(exit $?0)' && eval 'exec perl -S $0 ${1+"$@"}'
    & eval 'exec perl -S $0 $argv:q'
    if 0;

# $Id$
# -*- perl -*-

use Env (DDS_ROOT);
use lib "$DDS_ROOT/bin";
use Env (ACE_ROOT);
use lib "$ACE_ROOT/bin";

use strict;
use warnings;

use Cwd;
use File::Path;
use File::Find;

my %excludes = ();
my $output = '$DDS_ROOT/coverage_report';
my $gcov_tool = 'gcov';
my $verbose = 0;
my $source_root = $DDS_ROOT;
my $run_dir = $DDS_ROOT;
my $limit;

sub traverse {
    my $params = shift;
    my $dir = shift;
    my $depth = shift;

    if (!defined($dir)) {
      $dir = "$run_dir";
    }
    if (!defined($depth)) {
      $depth = 1;
    }

    my $dh;
    opendir($dh, $dir);
    my @entries = grep { !/^\.\.?$/ } readdir($dh);
    closedir($dh);
    foreach my $entry (@entries) {
        my $full_entry = "$dir/$entry";
        $full_entry =~ s/\/\//\//g;
        if (-d $full_entry) {
            if ($excludes{$entry}) {
                next;
            }
            my $continue = 1;
            if ($params->{dir_function}) {
                $continue = &{$params->{dir_function}}($params, $full_entry);
            }

            if ($continue && (!defined($params->{depth}) || $params->{depth} > $depth)) {
                traverse($params, $full_entry, $depth + 1);
            }
        }
        elsif ($params->{file_function}) {
            &{$params->{file_function}}($params, $full_entry);
        }
    }
}

sub removeFiles
{
    my $params = shift;
    my $file = shift;

    my $file_pattern = ".+";
    if (defined($params->{file_pattern})) {
        $file_pattern = $params->{file_pattern};
    }
    my $extension = $params->{extension};
    if ($file =~ /$file_pattern\.$extension$/) {
        unlink($file);
    }
}

sub findNoCoverage
{
    my $params = shift;
    my $dir = shift;

    my $dh;
    opendir($dh, $dir);
    my @entries = readdir($dh);
    closedir($dh);
    # collect all *.gdno and *.gcda files
    my @gcno_entries = ();
    my %gcda_entries = ();
    foreach my $entry (@entries) {
        if ($entry =~ s/\.gcno$//) {
            push(@gcno_entries, $entry);
        }
        elsif ($entry =~ s/\.gcda$//) {
            $gcda_entries{$entry} = 1;
        }
    }
    if ((scalar(@gcno_entries) > 0) &&
        (scalar(keys(%gcda_entries)) == 0)) {
        # just record the whole directory
        print {$params->{no_cov_fh}} "$dir/\n";
    }
    else {
        foreach my $prefix (@gcno_entries) {
            if (!defined($gcda_entries{$prefix})) {
                print {$params->{no_cov_fh}} "$dir/$prefix\n";
            }
        }
    }
    return 1;
}

sub yesterday
{
    my $day = shift;

    if ($day eq "sun") { return "sat" };
    if ($day eq "mon") { return "sun" };
    if ($day eq "tue") { return "mon" };
    if ($day eq "wed") { return "tue" };
    if ($day eq "thu") { return "wed" };
    if ($day eq "fri") { return "thu" };
    if ($day eq "sat") { return "fri" };

    print STDERR "Failed to identify $day as day of the week, cannot determine yesterday\n";
    exit 0;
}

sub clean_path {
    my $path = shift;
    $path =~ s/\\+/\//g;
    $path =~ s/\/+/\//g;
    $path =~ s/\/\.\//\//g;
    $path =~ s/\/[^\/]+\/\.\.\//\//g;
    $path =~ s/\/\.$//g;
    $path =~ s/\/$//g;
    $path =~ s/^\.\///g;
    return $path;
}

my %days = ();
my $cleanup_only = 0;
for (my $i = 0; $i <= $#ARGV; ++$i) {
    my $arg = $ARGV[$i];
    if ($arg eq "-cleanup_only") {
        $cleanup_only = 1;
    }
    elsif ($arg eq "-v" || $arg eq "-verbose") {
        $verbose = 1;
    }
    elsif (($arg eq "-o" || $arg eq "-output") && (++$i <= $#ARGV)) {
        $output = $ARGV[$i];
    }
    elsif (($arg eq "-gcov_tool") && (++$i <= $#ARGV)) {
        $gcov_tool = $ARGV[$i];
    }
    elsif (($arg eq "-x" || $arg eq "-exclude") && (++$i <= $#ARGV)) {
        $excludes{$ARGV[$i]} = 1;
    }
    elsif (($arg eq "-limit") && (++$i <= $#ARGV)) {
        $limit = $ARGV[$i];
    }
    elsif (($arg eq "-source_root") && (++$i <= $#ARGV)) {
        $source_root = $ARGV[$i];
    }
    elsif (($arg eq "-run_dir") && (++$i <= $#ARGV)) {
        $run_dir = $ARGV[$i];
    }
    elsif (($arg eq "-day") && (++$i <= $#ARGV)) {
        my $day = lc($ARGV[$i]);
        $days{$day} = 1;
    }
    else {
        print "Ignoring unkown argument: $ARGV[$i]\n";
    }
}

if (keys(%days) > 0) {
    my $now_str = localtime;
    #                  day     month date   hour   min  sec    year
    if ($now_str !~ /^(\S+)\s+\S+\s+\S+\s+(\d\d?):\d\d:\d\d\s+\S+/) {
        print "ERROR: couldn't parse localtime string, skipping coverage\n";
        exit 0;
    }
    my $day = lc($1);
    my $hour = $2;
    # before 6pm builds start, shift to previous day
    if ($hour < 18) {
        $day = yesterday($day);
    }

    if (!$days{$day}) {
        print "Skipping coverage on $day (build time frame)\n";
        exit 1;
    }
}

if (!defined($source_root) || $source_root eq "") {
    $source_root = $run_dir;
}
elsif (!defined($run_dir) || $run_dir eq "") {
    $run_dir = $source_root;
}

if ($cleanup_only) {
    print "Coverage: removing *.gcda\n" if $verbose;
    # traverse $run_dir and remove all *.gcda files
    traverse( { 'file_function' => \&removeFiles, 'extension' => 'gcda' });

    exit 1;
}

if (defined($limit)) {
    $limit = "$source_root/$limit/*";
    clean_path($limit);
}

print "  source_root=$source_root\n" if $verbose;
print "  run_dir=$run_dir\n" if $verbose;
print "  verbose=$verbose\n" if $verbose;
print "  output=$output\n" if $verbose;
print "  limit=$limit\n" if $verbose && defined($limit);

print "Coverage: removing *.info\n" if $verbose;
# traverse $run_dir and remove all *.info files
traverse( { 'file_function' => \&removeFiles, 'extension' => 'info' });

print "Coverage: collect coverage data\n" if $verbose;

my $no_cov_filename = "$run_dir/file_dirs_with_no_coverage.lst";
if (-f $no_cov_filename) {
    # maintain the previous file so we know if new classes are added
    # to the list
    system("mv $no_cov_filename $no_cov_filename.bak");
}

if (!open(NO_COV_FILE, ">", "$no_cov_filename")) {
    print STDERR __FILE__, ": Cannot write to $no_cov_filename for indicating files with no coverage data\n";
    exit 1;
}
traverse({ 'dir_function' => \&findNoCoverage,
           'no_cov_fh' => \*NO_COV_FILE });
close(NO_COV_FILE);

# use lcov to convert *.gcda files into a *.info file
my $status = system("lcov --capture --gcov-tool $gcov_tool --base-directory $source_dir " .
                    " --directory $run_dir --output-file $run_dir/all_cov.info");
if (!$status && defined($limit)) {
    $status = system("lcov --gcov-tool $gcov_tool --output-file $run_dir/dds_cov.info " .
                     "--extract $run_dir/all_cov.info \"$limit\" ");
}

if (!open(NO_COV_FILE, "<", "$no_cov_filename")) {
    print STDERR __FILE__, ": Cannot open $no_cov_filename for limiting coverage to root\n";
    next;
}
if (<NO_COV_FILE>) {
    print "Coverage: see $no_cov_filename for files with no coverage data\n";
}
close(NO_COV_FILE);

if (!$status) {
    if (-d $output) {
        print "Coverage: removing old coverage at $output\n" if $verbose;
        rmtree($output, 0, 1);
    }
    my $prefix = "";
    if (defined($limit)) {
        $prefix = $limit;
        $prefix =~ s/\/[^\/]+\/\*$//;
        $prefix = "--prefix $prefix";
    }
    my $command = "genhtml $prefix --output-file $output $run_dir/dds_cov.info";
    print "Coverage: generating html <$command>\n" if $verbose;
    $status = system ($command) == 0;
}

exit $status;
