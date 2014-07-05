#!/bin/sh 

log() {
	printf "${GREEN}do%$(((REDO_DEPTH+1)*2))s${BOLD}%s${PLAIN}\\n" "" "$1" >&2
}

err() {
	printf "${RED}do%$(((REDO_DEPTH+1)*2))s${BOLD}%s${PLAIN}\\n" "" "$1" >&2
}

timestamp() {
	date +%s || \
		echo $((`date -u \
		+"((%Y-1600)*365+(%Y-1600)/4-(%Y-1600)/100+(%Y-1600)/400+%-j-135140) \
		*86400+%-H*3600+%-M*60+%-S"`))
}

find_dofile_pwd() {
	local dofile="$1.do"

	if [ -e "$dofile" ]; then
		printf %s\\n "$dofile"
		return
	fi

	dofile="default.$1.do"
	until [ "$dofile" = "default.do" ]; do
		dofile="default.${dofile#default.*.}"
		if [ -e "$dofile" ]; then
			printf %s\\n "$dofile"
			return
		fi
	done 

	if [ -e "$dofile" ]; then
		printf %s\\n "$dofile"
		return
	fi

	return 1
}

find_dofile() {
	local targetfile="$1"
	local dofile

	cd "${targetfile%/*}"
	until [ "$PWD" = "/" ]; do
		dofile="$(find_dofile_pwd ${targetfile##*/})"
		if [ $? -eq 0 ]; then
			printf %s/%s\\n "$PWD" "$dofile"
			return
		fi
		cd ..
	done

	return 1
}

run_dofile() {
	local dofile="$1"
	local line1

	shift

	read line1 < $dofile
	if [ "$line1" != "${line1#\#!}" ]; then
		${line1#\#!} "$dofile" "$1" "$2" "$3"
	else
		sh "$dofile" "$1" "$2" "$3"
	fi
}

if [ ${REDO_DEPTH:=0} -eq 0 ]; then
	# if no target is given on the command line
	# default to target "all"
	[ $# -eq 0 ] && set -- all 

	if [ "$TERM" != "dumb" -a -t 2 ]; then
		export GREEN='\033[32m'
		export RED='\033[31m'
		export BOLD='\033[1m'
		export PLAIN='\033[m'
	fi

	export REDO_TIMESTAMP=$(timestamp)

	# FIXME
	export PATH="$PATH:$(dirname $0)"

#	for d in .redo .redo/bin
#	do [ -d $d ] || mkdir $d
#	done
#
#	REDO=$(realpath $0)
#	ln -s $REDO .redo/bin/redo
#	ln -s $REDO .redo/bin/redo-ifchange
#	ln -s $(which true) .redo/bin/redo-ifcreate
#	export PATH="$PATH:.redo/bin/"
fi

for target in "$@"; do
	if [ -e "$target" ] && [ "$(stat --format %Y $target)" -ge $REDO_TIMESTAMP ]; then
		# target has been created during redo run
		continue
	fi

	if [ "$target" != "${target#/}" ]; then
		targetfile="$target"
	else
		targetfile="$PWD/$target"
	fi
	dofile="$(find_dofile $targetfile)"
	
	if [ -z "$dofile" ]; then
		# no dofile to build target
		if [ -e "$target" ]; then
			# target exists -> assume target is a source file
			continue
		else
			err "No rule to make target '$target'. Stop."
			exit 2
		fi
	fi

	if [ -z "${dofile##*/default.*.do}" ]; then
		ext="${dofile##*/}"
		ext="${ext##default.}"
		ext="${ext%.do}"
	else
		ext="${target##*.}"
	fi
	basename="${target##*/}"
	basename="${basename%.$ext}"
	tmpfile="${targetfile}.part"

	log "${dofile##*/}:$target"
	(
	export REDO_DEPTH=$((REDO_DEPTH + 1))

	# ------------- HUP INT QUIT TERM
	trap "exit 1"   1   2   3    15
	trap "rm -f -- $tmpfile" 0 # EXIT 

	cd "${dofile%/*}"

	# targetfile relative to dofile
	targetfile="${targetfile#$PWD/}"

	set -o noclobber
	# if this fails some other process is working on this target already.
	if ! echo -n "" 2> /dev/null > "$tmpfile"; then
		# wait and exit with success
		while [ -e "$tmpfile" ]; do
			sleep 1
		done
		exit 0
	fi

	run_dofile "$dofile" "$targetfile" "$basename" "$tmpfile" >| "$tmpfile"
	rc=$?
	if [ $rc -ne 0 ]; then
		err "$(basename $dofile) returned $rc"
		exit $rc
	fi

	# dofile did not write anything to tmpfile
	# this happens with targets that do not create files
	# such as 'all' or 'clean'
	[ -s "$tmpfile" ] || exit 0

	mv "$tmpfile" "$targetfile"
	) || exit $?
done
