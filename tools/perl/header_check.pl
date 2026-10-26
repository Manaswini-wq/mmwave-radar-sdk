#!/usr/bin/perl
# header_check.pl - Verify include-guard consistency across SDK headers.
#
# Checks:
#   1. Every .h file has an #ifndef / #define include guard pair.
#   2. Guard macro matches the filename (e.g. mmwave_api.h -> MMWAVE_API_H).
#   3. Each .h that references SDK types includes mmwave_types.h.
#
# Usage:
#   perl header_check.pl --src sdk

use strict;
use warnings;
use Getopt::Long;
use File::Find;

my $src_dir = 'sdk';
GetOptions('src=s' => \$src_dir) or die "Usage: $0 --src <dir>\n";

my @headers;
File::Find::find(sub {
    push @headers, $File::Find::name if /\.h$/;
}, $src_dir);

my $violations = 0;

for my $hfile (sort @headers) {
    open my $fh, '<', $hfile or do {
        warn "Cannot open $hfile: $!\n";
        next;
    };
    my @lines = <$fh>;
    close $fh;

    my $content = join('', @lines);

    # --- Check 1: include guard presence ---
    my ($guard_ifndef) = $content =~ /^\s*#ifndef\s+(\w+)/m;
    my ($guard_define) = $content =~ /^\s*#define\s+(\w+)/m;

    unless ($guard_ifndef && $guard_define && $guard_ifndef eq $guard_define) {
        print "VIOLATION [$hfile]: Missing or mismatched include guard.\n";
        $violations++;
        next;
    }

    # --- Check 2: guard naming convention ---
    (my $basename = $hfile) =~ s{.*[/\\]}{};   # strip path
    (my $expected_guard = uc($basename)) =~ s/\./_/g;

    if ($guard_ifndef ne $expected_guard) {
        print "VIOLATION [$hfile]: Guard '$guard_ifndef' != expected '$expected_guard'.\n";
        $violations++;
    }

    # --- Check 3: types usage without include ---
    if ($content =~ /\b(uint32|int32|float32|mmwave_handle_t)\b/ &&
        $content !~ /#include\s+["<]mmwave_types\.h[">]/) {
        # Skip mmwave_types.h itself
        unless ($basename eq 'mmwave_types.h') {
            print "VIOLATION [$hfile]: Uses SDK types but does not include mmwave_types.h.\n";
            $violations++;
        }
    }
}

if ($violations == 0) {
    print "All headers OK (${\scalar @headers} files checked).\n";
} else {
    print "\n$violations violation(s) found in ${\scalar @headers} files.\n";
}

exit($violations > 0 ? 1 : 0);
