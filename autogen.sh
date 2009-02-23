#!/bin/bash

aclocal &&
autoconf &&
libtoolize &&
automake --add-missing;

echo -n "generating metha.h installation header... "
metha_h_success="no"
pwd_backup=`pwd`
cd src/libmetha/;
if test -f "include/metha.h"; then
    chmod u+w include/metha.h 
fi
echo "#ifndef _METHA__H_" > include/metha.h &&
echo "#define _METHA__H_" >> include/metha.h &&
echo "/**" >> include/metha.h &&
echo " * metha.h - $(date)" >> include/metha.h &&
echo " * Automatically generated for modules and clients." >> include/metha.h &&
echo " * License: http://bithack.se/projects/methabot/license.html" >> include/metha.h &&
echo " **/" >> include/metha.h &&
echo "" >> include/metha.h &&
cat include/metha.h.in >> include/metha.h &&
cat *.h | grep -f include/functions.grep | grep -f include/filter.grep >> include/metha.h &&
echo "#endif" >> include/metha.h &&
metha_h_success="yes"
cd $pwd_backup

if test "x$metha_h_success" = "xyes"; then
    echo "ok"
else
    echo "failed"
fi
