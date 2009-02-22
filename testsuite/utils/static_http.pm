package static_http;
use strict;
use warnings;

use File::MMagic;
use MIME::Types;
use URI::Escape;
use IO::File;
use File::Spec::Functions qw(canonpath);
use base qw(Exporter);

our @EXPORT = qw(use_static_files);

my $mime  = MIME::Types->new();
my $magic = File::MMagic->new();

sub use_static_files
{
    my ($self, $cgi, $base ) = @_;
    my $p = $cgi->url(-absolute => 1, -path_info => 1);
    $p =~ s{^https?://([^/:]+)(:\d+)?/}{/};
    $p = $base . canonpath(URI::Escape::uri_unescape($p) );
    my $len = length $p;
    my $tmp = substr($p, 0, $len-1);

    if ($tmp eq $base) {
        $p = $tmp .= 'index.html';
    }


    my $f = IO::File->new();
    if (-f $p && $f->open($p)) {
        binmode $f;
        binmode $self->stdout_handle;

        my $content;
        {
            local $/;
            $content = <$f>;
        }
        $f->close;

        my $content_length;
        if ( defined $content ) {
            use bytes;
            $content_length = length $content;
        } else {
            $content_length = 0;
            $content        = q{};
        }

        my $mime_obj = $mime->mimeTypeOf($p);
        my $mime_type;
        if (defined $mime_obj) {
            $mime_type = $mime_obj->type;
        } else {
            if ($content_length) {
                $mime_type = $magic->checktype_contents($content);
            }
            else {
                $mime_type = 'text/plain';
            }
        }

        print "HTTP/1.1 200 OK\r\n";
        print 'Content-type: ' . $mime_type . "\r\n";
        print 'Content-length: ' . $content_length . "\r\n\r\n";
        print $content;
        return 1;
    } else {
        my $content;
        my $content_length;
        $content = '<html><head><title>404 Not found</title></head><body><h1>404 Not found</h1></body></html>';
        use bytes;
        $content_length = length $content;


        print "HTTP/1.1 404 Not found\r\n";
        print "Content-type: text/html\r\n";
        print 'Content-length: ' . $content_length . "\r\n\r\n";
        print $content;
        return 0;
    }
    return 0;
}
1;
