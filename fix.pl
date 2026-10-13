#!/usr/bin/perl -w
# fix.pl <file.c>: one-shot version-guarded in-place patches so a single Lua source compiles on 5.1..5.4 and passes the PHP x Lua matrix.
#   1) GLOBALSINDEX guard (additive, idempotent): prepend "#ifndef LUA_GLOBALSINDEX / #define (-10000) / #endif" at top of any file that
#      references the macro but is not already guarded by us; on >= lua5.2 the header lacks it so this keeps literal uses valid C.
#   2a) FILE4 dump: standalone "<tab>lua_dump(...)" at line start -> version-guarded statement (strip arg only added at >= lua5.3).
#   2b) strdump dump: "if (lua_dump(...) != 0)" mid-line wrapper -> both branches re-emitted as guarded if-statements so the condition
#      and control flow are preserved while arity changes per version. Writer/buffer identifiers are captured live from each file to stay
#      immune to source-name mangling; byte-exact <5.3 branch reproduces the original call, keeping every combo compilable.
use strict; use warnings;

my $f = shift @ARGV or die "usage: fix.pl <file.c>\n";
open(my $in, '<', $f) or die "cannot read $f: $!"; binmode($in); my $src = do { local $/; <$in> }; close($in);

# detect the dump writer identifier live from the call itself (FILE4 -> luasandbox_dump_writer ; strdump -> writer).
my ($dwn) = ($src =~ /lua_dump\s*\(\s*L\s*,\s*(\w+)/) or die "no lua_dump(L, <writer>) found in $f\n";

# 1) GLOBALSINDEX guard -- prepend at absolute top; #ifndef makes it safe even if an included header also defines it.
if ($src =~ /LUA_GLOBALSINDEX/ && $src !~ /\#\s*ifndef\s+LUA_GLOBALSINDEX/) {
    my $guard = "#ifndef LUA_GLOBALSINDEX\n#define LUA_GLOBALSINDEX (-10000)\n#endif";
    $src = $guard . "\n" . $src;
}

# 2a) standalone dump statement at a tab-indent line start. Walk the call's parentheses (depth-balanced) to get an exact span immune
#      to nested/escaped parens and arg style; capture the arg-list verbatim so the <503 branch reproduces the original byte-for-byte,
#      appending only the lua 5.4 strip argument at >=503. Trailing ";" (outside the call) is left untouched by both branches.
my ($count1) = 0; my @spans; { my $p=-1; while (($p=index($src,"lua_dump(",$p+1))>=0){ push(@spans,$p); } }
for my $S (reverse @spans) {
    my $o = index($src, "(", $S); die "no open paren in dump call at offset $S\n" unless defined $o && $o == $S+8;  # 'lua_dump(' is exactly 9 chars -> '(' sits at S+8
    my $d=0; my $en=-1; for(my $i=$o;$i<length($src);$i++){my $c=substr($src,$i,1); if($c eq "("){$d++} elsif($c eq ")"){if(--$d==0){$en=$i;last}}}
    die "unbalanced dump call at offset $S\n" unless $en>=0;
    my $nl=-1; my $i=$S-1; while($i>=0 && substr($src,$i,1) ne "\n"){$i--} my $ls = ($i<0)? 0 : $i+1;   # first char of the line containing S (walk back to preceding newline +1)
    my $lead=$ls; while(substr($src,$lead,1) eq " " || substr($src,$lead,1) eq "\t"){ $lead++ }
    next if ($lead==$ls);                                            # require an indented (tab/space) standalone statement
    next if (substr($src,$lead,$S-$lead) =~ /\W/);                   # dump call must be first token of its statement; skip inside-condition sites (handled by strdump branch below)
    my $indent = substr($src,$ls,$lead-$ls);                          # actual leading whitespace text (tab/space) verbatim, for byte-exact branch indentation
    my $dargs = substr($src,$S+9,$en-$S-9);                          # args between the dump parens: 'L, <writer>, ...' -> '<...>' verbatim
    die "empty arg-list in dump call at offset $S\n" unless length($dargs);
    my $hi = "#if LUA_VERSION_NUM >= 503\n";
    my $lo = "#else\n";
    my $endif = "#endif";
    my $f_hi = "${indent}lua_dump(${dargs}, 0);\n";   # lua 5.4+: strip argument appended at end of arg list
    my $f_lo = "${indent}lua_dump(${dargs});\n";   # <503 branch reproduces the original call verbatim (byte-exact, no stray chars)
    my $repl = "$hi$f_hi$lo$f_lo$endif";
    $src = substr($src, 0, $lead) . $repl . substr($src, $en+2);   # consume original ')' and terminating ';' (branch strings carry their own ");"); part C resumes at the newline after
    $count1++;
}

# 2b) strdump-style "if (lua_dump(...) != 0)": re-emit both branches as guarded if-statements; keep indentation ($lead) and buffer token.
my ($count2) = 0;
if ($src =~ /^(\s*)\bif\s*\(\s*lua_dump\(L,\s*\Q$dwn\E\s*,.*?\&(\w+)[^\n]*$/m) {
    my ($lead, $buff) = ($1, $2);
    my $repl = "#if LUA_VERSION_NUM >= 503\n${lead}if (lua_dump(L, ${dwn}, (${buff}), 0) != 0)\n#else\n" . "${lead}"."if (lua_dump(L, ${dwn}, (${buff})) != 0)\n#endif";
    my ($start, $end) = ($-[0], $+[0]);
    die "dump-wrap span lost\n" if $start < 0 || $end > length($src);
    $src = substr($src, 0, $start) . $repl . substr($src, $end);
    $count2++;
}

open(my $out, '>', $f) or die "cannot write $f: $!"; binmode($out); print {$out} $src; close($out);
print STDOUT "DONE dwn=$dwn dump_edits=" . ($count1 + $count2) . "\n";
