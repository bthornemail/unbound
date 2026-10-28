# back.awk - the LITERALs of a wsn.awk file, in order, one per line: the
# source's non-blank lines, if nothing was added or lost.
/^  "/ {
	s = substr($0, 4, length($0) - 4)
	gsub(/""/, "\"", s)
	print s
}
