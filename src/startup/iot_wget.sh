#!/bin/sh

# First try WGET directly
#-���ȳ���ֱ��wget���,�������,�ʹ�NXP�����
echo "Wget 1, $*" >> /tmp/su.log
#-$*	���ݸ��ű����������в�����
#--q,--quiet ����ʾ�����Ϣ��
#-$?	�ϸ�������˳�״̬�������ķ���ֵ��
/usr/bin/wget -q $*
err=$?

echo "Wget 2, $err" >> /tmp/su.log

if [ ! $err = 0 ]; then
	# If it fails, then try it via the NXP fire wall

	echo "Wget 2" >> /tmp/su.log
	#-export���������޸Ļ�ɾ������������������ִ�еĳ���ʹ�á�
	export http_proxy="http://nww.nics.nxp.com:8080"
	/usr/bin/wget -q $*
	err=$?
fi

echo "Wget 3, $err" >> /tmp/su.log

