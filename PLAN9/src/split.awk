# split.awk - cut the roll-up (I-122.ts) into its records.
#
#	awk -v out=DIR -f split.awk I-122.ts
#
# A record is the run of comment lines just above a line record("I-...",
# through the next line that starts with "}". Each goes to DIR/I-....ts;
# every other line, in order, goes to DIR/I-122.ts.
{ l[NR] = $0 }
END {
	for (i = 1; i <= NR; i++) {
		if (l[i] !~ /^record\("/) continue
		id = l[i]; sub(/^record\("/, "", id); sub(/".*/, "", id)
		s = i; while (s > 1 && l[s - 1] ~ /^\/\//) s--
		e = i; while (e < NR && l[e] !~ /^}/) e++
		for (j = s; j <= e; j++) { who[j] = id }
	}
	for (i = 1; i <= NR; i++) {
		f = (i in who) ? out "/" who[i] ".ts" : out "/I-122.ts"
		if (f != last && last != "") close(last)
		last = f
		print l[i] >> f
	}
}
