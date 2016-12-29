/*
 * MQTT subscribe
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include "mqtt/mqtt_client.h"//������Ҷ�mqtt_client��װ���ͷ�ļ�

int running = 1;

void stop_running(int sig)
{
	signal(SIGINT, NULL);
	running = 0;
}


int mqtt_subscribe_send(char *topic, int Qos)
{
	int ret; //����ֵ
	
	//subscribe
	Qos = QOS_EXACTLY_ONCE;
	ret = mqtt_subscribe(m, topic, Qos);//������Ϣ
	printf("mqtt client subscribe %s,  return code = %d\n", topic, ret);
}

//-������Ϣ�ͷ�����ϢӦ�ÿ�����ͬһ���ͻ�����
int mqtt_Preceive_handler(void *arg) {

	//-��Ҫ�ȷ�������,Ȼ��Ż���յ�����,���ﶩ��ʲô������Ҫ����
	

	signal(SIGINT, stop_running);	//?�����ź���
	signal(SIGTERM, stop_running);	//-Ӧ�������ô����ź���

	printf("wait for message of topic: %s ...\n", topic);

	//-��������Կ���������ϢҲ��ͨ����ѯ�ķ�ʽ��,�����жϴ���
	//loop: waiting message
	while (running) {
		int timeout = 200;
		if ( mqtt_receive(m, timeout) == MQTT_SUCCESS ) { //recieve message��������Ϣ
			//-Ŀǰ���Ե�ʱ���ȿ��Խ��յ���Ϣ,Ȼ�������ڽ��зַ�
			printf("received Topic=%s, Message=%s\n", m->received_topic, m->received_message);
		}
		mqtt_sleep(200); //sleep a while
	}

	return 0;
}
