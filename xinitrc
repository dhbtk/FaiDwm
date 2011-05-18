if [ "" == "$XNEST_APP" ]; then
	echo "Set XNEST_APP before running." > /dev/stderr
	exit 1
fi

xrdb ~/.Xdefaults

xterm &

exec $XNEST_APP
