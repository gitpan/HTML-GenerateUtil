
#########################

use Test::More tests => 1016;
BEGIN { use_ok('HTML::GenerateUtil') };
use HTML::GenerateUtil qw(escape_html generate_attributes generate_tag);
use strict;

my $border_size = 100;
my @border = 'x' x $border_size;

is ('a="abc"', generate_attributes({ a => 'abc' }));
is ('abc="abc"', generate_attributes({ AbC => 'abc' }));
is ('abc', generate_attributes({ AbC => undef }));
is ('a="1"', generate_attributes({ a => 1 }));
is ('a="1.25"', generate_attributes({ a => 1.25 }));

is ('a="&<>""', generate_attributes({ a => [ '&<>"' ] }));
is ('a=""', generate_attributes({ a => [ ] }));

push @border, 'x' x $border_size;

is ('a="abc"', generate_attributes({ -a => 'abc' }));
is ('abc="abc"', generate_attributes({ -AbC => 'abc' }));
is ('abc', generate_attributes({ -AbC => undef }));

push @border, 'x' x $border_size;

my $a = generate_attributes({ a => 'abc', d => 'efg' });
is ('a="abc" d="efg"', join(' ', sort split ' ', $a));
$a = generate_attributes({ a => 'abc', d => 'efg' . "\x{1234}" });
is ('a="abc" d="efg' . "\x{1234}" . '"', join(' ', sort split ' ', $a));
$a = generate_attributes({ a => 'abc', d => 'efg<>"&' });
is ('a="abc" d="efg&lt;&gt;&quot;&amp;"', join(' ', sort split ' ', $a));
$a = generate_attributes({ a => 'abc', d => 'efg<>"&', h => undef });
is ('a="abc" d="efg&lt;&gt;&quot;&amp;" h', join(' ', sort split ' ', $a));

push @border, 'x' x $border_size;

for (1 .. 1000) {

  my %attr;
  for (1 .. int(rand(10))) {
    $attr{RandStr(10)} = rand() < 0.2 ? undef : RandStr(20, 1);
  }

  my $a = generate_attributes(\%attr);
  my $sortattr = join(' ', sort grep { defined $_ } ($a =~ /([^= ]+)(?: |\z)|([^= ]+="[^"]+")(?: |\z)/g));

  my @attr;
  for (keys %attr) {
    my $val = $attr{$_};
    if (!defined $val) {
      push @attr, $_;
      next;
    }

    $val =~ s/&/&amp;/g;
    $val =~ s/</&lt;/g;
    $val =~ s/>/&gt;/g;
    $val =~ s/\"/&quot;/g;

    push @attr, $_ . '="' . $val . '"';
  }

  is($sortattr, join(' ', sort @attr));
}

sub RandStr {
  my ($MaxLen, $IncFunny) = @_;

  my $str = '';
  my $Len = int(rand($MaxLen-3)) + 3;
  while (length($str) < $Len) {
    my $rnd = rand();
    if ($rnd < 0.05 && $IncFunny)    { $str .= '<'; }
    elsif ($rnd < 0.10 && $IncFunny) { $str .= '>'; }
    elsif ($rnd < 0.15 && $IncFunny) { $str .= '&'; }
    elsif ($rnd < 0.20 && $IncFunny) { $str .= '"'; }
    elsif ($rnd < 0.98 || !$IncFunny) { $str .= chr(ord('a') + rand(26)); }
    else { $str .= chr(ord('a') + rand(10000)); }
  }

  return $str;
}

push @border, 'x' x $border_size;

is(join('', @border), 'x' x ($border_size * @border));

