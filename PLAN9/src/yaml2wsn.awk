# yaml2wsn.awk - a flat catalog entry ("NAME": then   key: "value" lines) to
# Wirth syntax notation: NAME = KEY ... . then KEY = "value" . for each key,
# identifiers by their letters, upper case; a quote inside a value is "".
function id(s) { gsub(/[^A-Za-z]/, "", s); return toupper(s) }
NR == 1 { t = $0; sub(/^"/, "", t); sub(/":[ \t]*$/, "", t); top = id(t); next }
/^  [^ ].*: "/ {
	k = $0; sub(/^  /, "", k); sub(/: ".*$/, "", k)
	v = $0; sub(/^[^:]*: "/, "", v); sub(/"[ \t]*$/, "", v)
	gsub(/""/, "\"", v); gsub(/"/, "\"\"", v)
	key[++n] = id(k); val[n] = v
	next
}
/[^ \t]/ { print FILENAME ": line " NR " is not  key: \"value\"" > "/fd/2"; bad = 1 }
END {
	if (bad) exit 1
	printf "%s =", top
	for (i = 1; i <= n; i++) printf " %s", key[i]
	print " ."
	for (i = 1; i <= n; i++) print key[i] " = \"" val[i] "\" ."
}
