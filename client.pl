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
    Blocking => 1,
) or die "could not open socket: $!";

while($socket->connected) {
    if(send($socket, "ping\n", 0)) {
        my $tmp = undef;
        my $buf = undef;
        my $buf_sz = 512;

        say 'ping';

        if(IO::Select->new($socket)->can_read(.1)) {
            while(my $sz = recv $socket, $tmp, $buf_sz, MSG_DONTWAIT) {
                last unless($sz);

                print STDERR "sz: $sz\n";

                $buf .= $tmp;

                next if($sz == $buf_sz);

                print "response: $buf\n";
                $tmp = '';
            }
        }
    }

    Time::HiRes::usleep(100_000);
}

close($socket);
