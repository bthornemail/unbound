# coq2wsn.awk - one Coq module to Wirth syntax notation, with where it breaks.
#
#	awk -v top=NAME -v breakline=N -f coq2wsn.awk module.v
#
# Every non-blank line is one LITERAL, in order ("" for a quote inside). A
# line that begins a top-level Coq sentence (Definition, Theorem, Lemma, ...)
# starts a production named by the declared identifier's letters, upper case;
# the lines before the first are HEAD. breakline is the line coqc stopped at
# (0 when the module compiles): the declaration holding it BREAKS, the ones
# before are CHECKED, the ones after are UNCHECKED, and the top production is
#	top = CHECKED [ BREAKS UNCHECKED ] .
function ident(t,	s, k, t2) {
	s = t
	gsub(/[^A-Za-z]/, "", s)
	s = toupper(s)
	if (s == "") s = "SECTION"
	if (s == top || s in used || s == "CHECKED" || s == "BREAKS" || s == "UNCHECKED") {
		k = 0
		do { k++; t2 = s suffix(k) } while (t2 in used || t2 == top)
		s = t2
	}
	used[s] = 1
	return s
}
function suffix(k,	r) {
	r = ""
	while (k > 0) { r = substr("ABCDEFGHIJKLMNOPQRSTUVWXYZ", (k - 1) % 26 + 1, 1) r; k = int((k - 1) / 26) }
	return r
}
BEGIN {
	KW = "^(Local |Global |#\\[[^]]*\\] )?(Definition|Theorem|Lemma|Corollary|Proposition|Remark|Fact|Example|Fixpoint|CoFixpoint|Inductive|CoInductive|Record|Structure|Class|Instance|Notation|Ltac|Axiom|Parameter|Hypothesis|Variable|Module|Section|End|Require|From|Import|Export|Open|Close|Set|Unset|Arguments|Coercion|Let|Program|Compute|Check|Eval|Print|Opaque|Transparent|Hint|Create|Scheme|Derive)[ .]"
	n = 0
}
/^[ \t]*$/ { next }
{
	if ($0 ~ KW || n == 0) {
		if ($0 ~ KW) {
			w = $0; sub(/^(Local |Global |#\[[^]]*\] )/, "", w)
			split(w, p, /[ \t(:.]+/)
			name = ident(p[2] != "" ? p[2] : p[1])
		} else
			name = ident("HEAD")
		order[++n] = name; count[n] = 0; start[n] = NR
	}
	line = $0
	gsub(/"/, "\"\"", line)
	body[n, ++count[n]] = line
}
END {
	if (n == 0) { print top " = \"\" ."; exit 1 }
	brk = 0
	if (breakline > 0) for (i = 1; i <= n; i++) if (start[i] <= breakline) brk = i
	printf "%s = CHECKED", top
	if (brk) printf " [ BREAKS UNCHECKED ]"
	print " ."
	print ""
	printf "CHECKED ="
	for (i = 1; i <= (brk ? brk - 1 : n); i++) printf " %s", order[i]
	if (brk == 1) printf " { \" \" }"
	print " ."
	if (brk) {
		print "BREAKS = " order[brk] " ."
		printf "UNCHECKED ="
		for (i = brk + 1; i <= n; i++) printf " %s", order[i]
		if (brk == n) printf " { \" \" }"
		print " ."
	}
	for (i = 1; i <= n; i++) {
		print ""
		print order[i] " ="
		for (j = 1; j <= count[i]; j++) print "  \"" body[i, j] "\""
		print "  ."
	}
}
