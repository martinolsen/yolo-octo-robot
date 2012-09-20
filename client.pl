#!/usr/bin/env perl
use Modern::Perl;

use Time::HiRes;
use IO::Socket;
use IO::Select;

my $host = '10.0.0.62';

# Open connection to server
my $socket = IO::Socket::INET->new(
    PeerAddr => $host,
    PeerPort => 10001,
    Proto => 'tcp',
) or die "could not open socket: $!";

my $tmp = undef;
my $buf = undef;
my $buf_sz = 512;

while(my $sz = read $socket, $tmp, $buf_sz) {
    last unless($sz);

    print STDERR "sz: $sz\n";

    $buf .= $tmp;

    next if($sz == $buf_sz);

    print "response: $buf\n";
    $tmp = '';
}

close($socket);
