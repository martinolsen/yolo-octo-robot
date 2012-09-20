#!/usr/bin/env perl
use Modern::Perl;

use IO::Socket;
use IO::Select;
use Time::HiRes;

my $host = $ARGV[0] || 'localhost';
my $port = $ARGV[1] || 10001;

my $num_sockets = 1000;

my @sockets;
my $select = IO::Select->new;

say "creating $num_sockets connections to $host:$port";

for my $i (1 .. $num_sockets) {
    my $socket = IO::Socket::INET->new(
        PeerAddr => $host,
        PeerPort => $port,
        Blocking => 0,
    ) or die "IO::Socket->new(): $!";

    $select->add($socket);

    push @sockets, $socket;
}

say 'Sleeping...';
sleep 5;

my $total_rec = 0;

while(1) {
    for my $socket ($select->can_read(.2)) {
        my $buf = undef;

        say 'Receiving transmission...';

        if($socket->read($buf, 512)) {
            $total_rec += length($buf);

            say "length buf: ". length($buf)
                . "; total per client: " . $total_rec / $num_sockets;
        } else {
            die 'Could not read!';
        }

        say 'End of transmission!';
    }
}

for my $socket (@sockets) {
    $socket->close;
}
