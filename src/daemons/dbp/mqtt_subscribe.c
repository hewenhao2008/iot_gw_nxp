/*
 * MQTT subscribe
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include "mqtt/mqtt_client.h"//这个是我对mqtt_client封装后的头文件

int running = 1;

void stop_running(int sig)
{
	signal(SIGINT, NULL);
	running = 0;
}


int mqtt_subscribe_send(char *topic, int Qos)
{
	int ret; //返回值
	
	//subscribe
	Qos = QOS_EXACTLY_ONCE;
	ret = mqtt_subscribe(m, topic, Qos);//订阅消息
	printf("mqtt client subscribe %s,  return code = %d\n", topic, ret);
}

//-订阅消息和发布消息应该可以是同一个客户主体
int mqtt_Preceive_handler(void *arg) {

	//-需要先发布订阅,然后才会接收到命令,这里订阅什么主题需要考虑
	

	signal(SIGINT, stop_running);	//?发送信号量
	signal(SIGTERM, stop_running);	//-应该是设置处理信号量

	printf("wait for message of topic: %s ...\n", topic);

	//-从下面可以看出接收消息也是通过查询的方式的,不是中断处理
	//loop: waiting message
	while (running) {
		int timeout = 200;
		if ( mqtt_receive(m, timeout) == MQTT_SUCCESS ) { //recieve message，接收消息
			//-目前调试的时候先可以接收到消息,然后下面在进行分发
			printf("received Topic=%s, Message=%s\n", m->received_topic, m->received_message);
		}
		mqtt_sleep(200); //sleep a while
	}

	return 0;
}
