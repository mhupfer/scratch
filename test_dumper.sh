#!/bin/sh
# run as root

if [ $(whoami) == "root" ]; then
    #determine the pid of an existing process
    PNAME=qconn
    CDIR=/tmp
    CNAME=$CDIR/$PNAME.core
    PID=$(pidin ar | grep -E "\s+\d+\s+$PNAME" | cut -d ' ' -f 3)


    # restart dumper without -a
    echo Test without -a
    slay dumper 2> /dev/null
    dumper -d $CDIR

    rm $CNAME 2> /dev/null
    echo $PID > /proc/dumper

    if [ -e $CNAME ]; then
        echo Test failed, core file should not be created
    fi


    # restart dumper with -a
    echo Test with -a as root
    slay dumper  2> /dev/null
    dumper -d $CDIR -a -o0666

    rm $CNAME 2> /dev/null
    echo $PID > /proc/dumper

    if [ ! -e $CNAME ]; then
        echo Test failed, core file should be created
    fi

    # as nonroot
    echo Test with -a as non-root
    rm $CNAME 2> /dev/null
    su qnxuser -c echo $PID > /proc/dumper

    if [ ! -e $CNAME ]; then
        echo Test failed, core file should be created
    fi
else
    echo run as root
fi
