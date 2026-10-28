# prec.awk - the Wirth-Weber precedence relations of every wsn.awk file read,
# taken together as one grammar.
#
#	awk -f prec.awk files... >relations
#
# A LITERAL is a terminal, the same symbol in every file; a production name
# is a nonterminal of its own file. For each production A -> ... X Y ...:
#	X ≐ Y,  X ⋖ Head+(Y),  Tail+(X) ⋗ Head*(Y)
# and for each file's start symbol S:  $ ⋖ Head+(S),  Tail+(S) ⋗ $.
# Each line out: X, the relation, Y, and the files it holds in (tab apart).
FNR == 1 { start[FILENAME] = ""; nf++ }
/^[A-Z]+ = .* \.$/ && start[FILENAME] == "" {
	split($0, w, " ")
	s = FILENAME ":" w[1]; start[FILENAME] = s
	n = 0
	for (i = 3; w[i] != "."; i++) rhs[s, ++n] = FILENAME ":" w[i]
	len[s] = n
	next
}
/^[A-Z]+ =$/ { cur = FILENAME ":" $1; len[cur] = 0; next }
/^  "/ { rhs[cur, ++len[cur]] = substr($0, 3); next }
function nt(x) { return (x in len) }
function head(x, plus,	h) { if (!nt(x)) return plus ? "" : x; h = rhs[x, 1]; return h (nt(h) ? SUBSEP head(h, 1) : "") }
function tail(x,	t) { if (!nt(x)) return ""; t = rhs[x, len[x]]; return t (nt(t) ? SUBSEP tail(t) : "") }
function terms(set,	a, k, i, r) { k = split(set, a, SUBSEP); r = ""; for (i = 1; i <= k; i++) if (a[i] != "" && !nt(a[i])) r = r (r == "" ? "" : SUBSEP) a[i]; return r }
function add(x, r, y, f,	k) { if (x == "" || y == "") return; k = x SUBSEP r SUBSEP y; if (!((k, f) in held)) { held[k, f] = 1; where[k] = where[k] (where[k] == "" ? "" : " ") f } }
function addset(xs, r, ys, f,	a, b, i, j, m, n) { m = split(xs, a, SUBSEP); n = split(ys, b, SUBSEP); for (i = 1; i <= m; i++) for (j = 1; j <= n; j++) add(a[i], r, b[j], f) }
END {
	for (p in len) {
		f = p; sub(/:[A-Z]+$/, "", f)
		for (i = 1; i < len[p]; i++) {
			x = rhs[p, i]; y = rhs[p, i + 1]
			add(x, "≐", y, f)
			addset(x, "⋖", head(y, 1), f)
			addset(tail(x), "⋗", terms(head(y, 0) SUBSEP head(y, 1)), f)
		}
	}
	for (f in start) { addset("$", "⋖", head(start[f], 1), f); addset(tail(start[f]), "⋗", "$", f) }
	for (k in where) { split(k, a, SUBSEP); print a[1] "\t" a[2] "\t" a[3] "\t" where[k] }
}
