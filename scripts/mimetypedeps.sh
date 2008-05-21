#!/bin/sh

case $1 in
-P|--provides)
	while read filename; do
	case "$filename" in
	*.desktop)
		mime=$(awk -F= '/^MimeType=/{print $2}' "$filename")
		IFS=';'
		for type in $mime; do
		if [ -n "$type" ]; then
			echo "mimetype($type)"
		fi
		done
		;;
	esac
	done
	;;
esac

exit 0
