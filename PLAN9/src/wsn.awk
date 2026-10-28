# wsn.awk - one file to Wirth syntax notation, meaning kept line for line.
#
#	awk -v top=NAME -f wsn.awk file
#
# Every non-blank line is one LITERAL, in order ("" for a quote inside).
# A line "// ── title ──" starts a production named by the title's letters,
# upper case; the lines before the first one are HEAD. The first production,
# named top, is the sequence of all the others.
function ident(t,	s) {
	s = t
	gsub(/[^A-Za-z]/, "", s)
	s = toupper(s)
	if (s == "") s = "SECTION"
	if (s == top || s in used) {
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
BEGIN { n = 0 }
/^[ \t]*$/ { next }
{
	if ($0 ~ /^\/\/ ── / || n == 0) {
		name = ($0 ~ /^\/\/ ── /) ? ident($0) : ident("HEAD")
		order[++n] = name
		count[n] = 0
	}
	line = $0
	gsub(/"/, "\"\"", line)
	body[n, ++count[n]] = line
}
END {
	if (n == 0) { print top " = \"\" ."; exit 1 }
	printf "%s =", top
	for (i = 1; i <= n; i++) printf " %s", order[i]
	print " ."
	for (i = 1; i <= n; i++) {
		print ""
		print order[i] " ="
		for (j = 1; j <= count[i]; j++) print "  \"" body[i, j] "\""
		print "  ."
	}
}
