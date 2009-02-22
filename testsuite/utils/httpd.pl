#!/usr/bin/perl

package MyServer;

use base qw(HTTP::Server::Simple::CGI);
use Cwd;
my $dir;
use lib './utils/';
use static_http;

sub handle_request {
    my ($self, $cgi) = @_;
    return $self->use_static_files($cgi, $dir);
}

package main;

if (!defined $ARGV[0]) {
    $dir = getcwd;
    print "no parameter given, using cwd\n";
} else {
    $dir = $ARGV[0];
}
print 'using directory '.$dir."\n";
my $web = MyServer->new(9999);
$web->host('127.0.0.1');
$web->run();
