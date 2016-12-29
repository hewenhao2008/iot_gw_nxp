//-为了方便调试定义了系列变量以便调试输出

#ifndef MQTT_PUBLISH_H
#define MQTT_PUBLISH_H

int mqtt_publish_create(int argc, char ** argv);
int dbp_send_data_to_clients_mqtt(char *buf,int data);
int mqtt_publish_close(void);
void *mqtt_Pclient_handler(void *arg);

#endif /* MQTT_PUBLISH_H */
