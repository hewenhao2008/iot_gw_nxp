/*
 * MQTT publish
 *
 * Author: JoStudio
 */
/*
主程序分为三个步骤：

1， 调用 mqtt_new()创建 客户端对象

2， 调用 mqtt_connect() 连接服务器

3， 调用 mqtt_publish() 发布消息

*/


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "mqtt_client.h"//这个是我对mqtt_client封装后的头文件



mqtt_client *m; //mqtt_client 对象指针
char *topic = "iot_td/ZigBee/HA/NXP/"; //主题

int mqtt_publish_sub(int argc, char ** argv) {

	
	int ret; //返回值
	char *host = "messagesight.demos.ibm.com:1883";//测试服务器
	
	char *client_id = "clientid33883";//客户端ID； 对测试服务器，可以随便写
	char *username = NULL;//用户名，用于验证身份。对测试服务器，无。
	char *password = NULL;//密码，用于验证身份。对测试服务器，无。
	int Qos; //Quality of Service

	//create new mqtt client object
	m = mqtt_new(host, MQTT_PORT, client_id); //创建对象，MQTT_PORT = 1883
	if ( m == NULL ) {
		printf("mqtt client create failure, return  code = %d\n", errno);
		return 1;
	} else {
		printf("mqtt client created\n");
	}
	//-上面由于没有套接字的操作,所以即使我没有联网也是可以创建成功的,就是为了以后的操作准备了大量的信息和空间
	//-就是一次创建的,没有周期函数在里面
	//connect to server
	ret = mqtt_connect(m, username, password); //连接服务器,这里不仅仅有硬件连接还有MQTT协议连接
	if (ret != MQTT_SUCCESS ) {
		printf("mqtt client connect failure, return code = %d\n", ret);
		return 1;
	} else {
		printf("mqtt client connect\n");
	}
	//-上面一个流程仅仅完成了连接请求,协议层的连接可能在另外一个线程中实现的
	
	//-补充主题
	//-route010203040506/dongle010203040506/device0102030405060708/tmp
	
	return 0;
}



static int dbp_send_data_to_clients_mqtt(char *data)
{
	//publish message
	Qos = QOS_EXACTLY_ONCE; //Qos
	ret = mqtt_publish(m, topic, data, Qos);//发布消息
	printf("mqtt client publish,  return code = %d\n", ret);
	//-下面是和服务器断开连接,并且清除为了连接而建立的系列环境
	mqtt_disconnect(m); //disconnect
	mqtt_delete(m);  //delete mqtt client object
}

