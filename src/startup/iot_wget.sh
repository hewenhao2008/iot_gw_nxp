#!/bin/sh

# First try WGET directly
#-首先尝试直接wget获得,如果不行,就从NXP网获得
echo "Wget 1, $*" >> /tmp/su.log
#-$*	传递给脚本或函数的所有参数。
#--q,--quiet 不显示输出信息；
#-$?	上个命令的退出状态，或函数的返回值。
/usr/bin/wget -q $*
err=$?

echo "Wget 2, $err" >> /tmp/su.log

if [ ! $err = 0 ]; then
	# If it fails, then try it via the NXP fire wall

	echo "Wget 2" >> /tmp/su.log
	#-export可新增，修改或删除环境变量，供后续执行的程序使用。
	export http_proxy="http://nww.nics.nxp.com:8080"
	/usr/bin/wget -q $*
	err=$?
fi

echo "Wget 3, $err" >> /tmp/su.log

