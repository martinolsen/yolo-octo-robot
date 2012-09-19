#!/usr/bin/env perl
use Modern::Perl;

use Time::HiRes;
use IO::Socket;

# Open connection to server
my $socket = IO::Socket::INET->new(
    PeerAddr => '127.0.0.1',
    PeerPort => 10001,
    Proto => 'tcp',
    Blocking => 0,
) or die "could not open socket: $!";

while(send $socket, "ping\n", 0) {
    my $tmp = undef;
    my $buf = undef;
    my $buf_sz = 512;

    while(my $sz = sysread $socket, $tmp, $buf_sz) {
        last unless($sz);

        print STDERR "sz: $sz\n";

        $buf .= $tmp;

        next if($sz == $buf_sz);

        print "response: $buf\n";
        $tmp = '';
    }

    Time::HiRes::usleep(2000);
}

close($socket);
