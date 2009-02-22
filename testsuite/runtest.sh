#!/bin/bash

CHILD_PIDS=""

function die {
    c=$1
    set -- $CHILD_PIDS
    for a in $@; do
        kill $a > /dev/null
    done
    exit $c
}

function runtest {
    PREV="`pwd`"
    TEST="$1"
    source "${TEST}/DESC"
    set -- $REQUIRE
    for a in $@; do
        if test -f "utils/${a}.pl"; then
            utils/${a}.pl "`pwd`/${TEST}/files/" 2>&1 > /dev/null &
            PID=$!
            disown $PID
            if [ "x${PID}" = "x" ]; then
                echo could not start $a
                die 1;
            else
                CHILD_PIDS="${CHILD_PIDS} ${PID}"
                sleep 0.50
            fi
        else
            echo $TEST: required script not found: $a.pl
            die 1
        fi
    done
    cd $TEST;

    if test -d "conf"; then
        export MB_CONF_PATH="`pwd`/conf"
    else
        unset MB_CONF_PATH
    fi
    if test -d "scripts"; then
        export MB_SCRIPT_PATH="`pwd`/scripts"
    else
        unset MB_SCRIPT_PATH
    fi

    echo Running $TEST \(${INFO}\)... 
    START=$(date +%s.%N)
    if test -f "INPUT"; then
        cat INPUT | ${RUN} 2> output.err.log > output.log
    else
        ${RUN}  2> output.err.log > output.log
    fi
    END=$(date +%s.%N)
    if [ "x${ORDERED}" == "x1" ]; then
        cp output.log output.log.sorted
        cp OUTPUT OUTPUT.sorted
    else
        LC_ALL=C sort output.log -o output.log.sorted
        LC_ALL=C sort OUTPUT -o OUTPUT.sorted
    fi
    ERR="$(cat output.err.log | grep \"[E]\" | wc -l)"
    if [ "x${ERR}" != "x0" ] || [ "x$(diff output.log.sorted OUTPUT.sorted)" != "x" ]; then
        echo "- failed"
        echo "- time: $(echo "$END - $START" | bc)"
        echo "* STDERR LOG: ${TEST}/output.err.log";
        echo "* STDOUT LOG: ${TEST}/output.log";
    else
        rm output.log
        echo "- success"
        echo "- time: $(echo "$END - $START" | bc)"
        WARN="$(cat output.err.log | grep \"[W]\" | wc -l)"
        if [ "x$WARN" != "x0" ]; then
            echo "*   WARNINGS: ${WARN} in ${TEST}/output.err.log";
        else
            rm output.err.log
        fi
    fi
    echo
    rm output.log.sorted
    rm OUTPUT.sorted
    cd $PREV;

    set -- $CHILD_PIDS
    for a in $@; do
        kill $a > /dev/null
    done

    CHILD_PIDS=""
}

if ! echo | perl -MHTTP::Server::Simple 2>&1 > /dev/null; then
    echo perl module HTTP::Server::Simple not found
    exit 1
fi
if ! echo | perl -MFile::MMagic 2>&1 > /dev/null; then
    echo perl module File::MMagic not found
    exit 1
fi
if ! echo | perl -MMIME::Types 2>&1 > /dev/null; then
    echo perl module MIME::Types not found
    exit 1
fi

if test -z "$*"; then
    echo "usage: ./runtest.sh [testxxx] [--all]"
fi

for a in "$@"; do
    case "$a" in
        --all) for c in `ls -1 | grep "test*"`; do
                   if test -d "$c"; then
                       runtest $c
                   fi
               done;;
        *) if test -d "$a"; then
               runtest $a
           fi;;
    esac
done

die 0
