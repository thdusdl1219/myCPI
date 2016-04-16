#!/usr/bin/perl
# 
# Generate html version of nightly test report by diffing two text reports.
#
# Usage: htmlize.pl old_text_report new_text_report html_report
#
use File::Basename;

$#ARGV == 2 or die "Usage: $0 old_report new_report output\n";

$old_rpt = $ARGV[0];
$new_rpt = $ARGV[1];
$output = $ARGV[2];

print "Reading old report $old_rpt...\n";

open OLD, $old_rpt;
while (<OLD>) {
  if (/([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([-\d:\.]+)\s+([-\d:\.]+)\s+([-\d:\.]+)\s+(.*)/) {
    $str = "$1$2$4";
    $ctime{$str} = $5;
    $rtime{$str} = $6;
    $stime{$str} = $7;
    $status{$str} = $8;
  }
  elsif (/([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+(.*)/) {
    # older format without single run time
    #$str = "$1$2$3$4";
    $str = "$1$2$4";
    $ctime{$str} = $5;
    $rtime{$str} = $6;
    $stime{$str} = 0;
    $status{$str} = $7;
  }
}
close OLD;

print "Reading new report $new_rpt and generating $output...\n";

open NEW, $new_rpt;
open OUT, ">$output";

print OUT "<html><head>\n";
print OUT "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n";
print OUT "<title>$new_rpt</title>\n";
print OUT "</head><body>\n";

$in = 0;
$prev_link = basename($old_rpt) . ".html";

while (<NEW>) {
  if (/^Bench\s/) {
    $in = 1;
    print OUT "Colors are from diffing against <a href=\"$prev_link\">previous run</a>.<br><br>\n";
    print OUT "<table border=1>\n";
    print OUT "<tr><th>Benchmark</th><th>Function</th><th>Loop</th><th>Opti</th><th>Compile Time</th><th>Run Time</th><th>Speedup</th><th>Status</th></tr>\n";
  }
  if ($in && /^Finished/) {
    $in = 0;
    print OUT "</table>\n<br><br>\n";
  }
  if ($in && /([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([^\s]+)\s+([-\d:\.]+)\s+([-\d:\.]+)\s+([-\d:\.]+)\s+(.*)/) {
    #$str = "$1$2$3$4";
    $str = "$1$2$4";
    $new_ctime = $5;
    $new_rtime = $6;
    $new_stime = $7;
    $new_status = $8;

    $new_speedup = get_speedup($new_rtime, $new_stime);

    if (defined $status{$str}) {
      $c_color = get_color_str($ctime{$str}, $new_ctime);
      $r_color = get_color_str($rtime{$str}, $new_rtime);
      $s_color = get_color_str_status($status{$str}, $new_status);
    } else {
      $c_color = $r_color = $s_color = "";
    }

    print OUT "<tr><td>$1</td><td>$2</td><td>$3</td><td>$4</td><td$c_color>$5</td><td$r_color>$6</td><td>$new_speedup</td><td$s_color>$8</td></tr>\n";
  } elsif ($in && ( /^Bench\s/ || /^=====\s/ )) {
    next;
  } else {
    print OUT "$_<br>";
  }
}

print OUT "</body></html>\n";
close OUT;
close NEW;

exit;

sub get_seconds {
  my ($time_str) = @_;
  if ($time_str =~ /(\d+):(\d\d)\.(\d\d)/) {
    $sec = $2 + 60 * $1;
  } elsif ($time_str =~ /(\d+):(\d\d):(\d\d)/) {
    $sec = $3 + 60 * $2 + 3600 * $1;
  }
  return $sec;
}

sub get_hundredths {
  my ($time_str) = @_;
  if ($time_str =~ /(\d+):(\d\d)\.(\d\d)/) {
    $hs = $3 + 100 * $2 + 60 * 100 * $1;
  } elsif ($time_str =~ /(\d+):(\d\d):(\d\d)/) {
    $hs = 100 * $3 + 100 * 60 * $2 + 100 * 3600 * $1;
  }
  return $hs;
}

sub get_speedup {
  my ($par_time, $single_time) = @_;
  if ($par_time eq "-" or $single_time eq "-") {
    return "-";
  }
  $par_hs = get_hundredths($par_time);
  if ($par_hs == 0) {
    return "-";
  }
  $single_hs = get_hundredths($single_time);
  $speedup = $single_hs / $par_hs;
  $speedup = sprintf("%.2f", $speedup);
  return $speedup;
}

sub get_color_str {
  my ($old_time, $new_time) = @_;
  if ($old_time eq "-" or $new_time eq "-") {
    return "";
  }
  $old_sec = get_seconds($old_time);
  $new_sec = get_seconds($new_time);
  if ($old_sec == 0) {
    $denom = 5;
  } else {
    $denom = $old_sec;
  }
  $diff = ($new_sec - $old_sec) / $denom;

  #print "$old_time ($old_sec) => $new_time ($new_sec) diff: $diff\n";
  if ($diff > 0) {
    $hexcolor = sprintf("%.2X", 255 - ($diff > 1 ? 1 : $diff) * 255);
    $color = " bgcolor=\"#FF$hexcolor$hexcolor\"";
  } elsif ($diff < 0) {
    $hexcolor = sprintf("%.2X", 255 - (-$diff * 255));
    $color = " bgcolor=\"#" . $hexcolor . "FF$hexcolor\"";
  } else {
    $color = "";
  }
  return $color;
}

sub get_color_str_status {
  my ($old_stat, $new_stat) = @_;
  $old_success = ($old_stat =~ /SUCCESS/);
  $new_success = ($new_stat =~ /SUCCESS/);
  if ($old_success && !$new_success) {
    return " bgcolor=\"#FF0000\"";
  }
  if (!$new_success) {
    return " bgcolor=\"#FFC0C0\"";
  }
  if (!$old_success && $new_success) {
    return " bgcolor=\"#00FF00\"";
  }
  return "";
}

