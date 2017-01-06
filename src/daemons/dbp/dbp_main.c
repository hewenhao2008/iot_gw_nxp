/****************************************************************************
 *
 * PROJECT:             Wireless Sensor Network for Green Building
 *
 * AUTHOR:              Debby Nirwan
 *
 * DESCRIPTION:         DBP Server Implementation
 *
 ****************************************************************************
 *
 * This software is owned by NXP B.V. and/or its supplier and is protected
 * under applicable copyright laws. All rights are reserved.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Copyright NXP B.V. 2014. All rights reserved
 *
 ***************************************************************************/

/* standard include */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include <limits.h>

/* local include */
#include "queue.h"
#include "newLog.h"
#include "newDb.h"
#include "json.h"
#include "jsonCreate.h"
#include "gateway.h"
#include "parsing.h"
#include "dbp.h"
#include "dbp_search.h"
#include "dbp_loc_receiver.h"
#include "mqtt_client.h"

/* version */
#define DBP_MAJOR_VERSION    "0"
#define DBP_MINOR_VERSION    ".9"

/* defines */
#define DBP_PORT     "2002"
#define INPUTBUFFERLEN  MAXMESSAGESIZE
#define MAX_NUM_OF_CONN    100

/* struct */
typedef struct{
    bool connected;
    uint8_t conn_number;
    int sock_fd[MAX_NUM_OF_CONN];
}connection_t;

/* global variable */
pthread_t zb_msg_thread, dbp_msg_thread;
pthread_mutex_t dbp_mutex = PTHREAD_MUTEX_INITIALIZER;
int dbpQueue = -1;
char inputBuffer[INPUTBUFFERLEN+2];
connection_t connection;
char version[13] = {0};

#define NUMINTATTRS  9
char * intAttrs[NUMINTATTRS] = {"tmp", "hum", "als", "lqi", "bat", "batl", "xloc", "yloc", "zloc"};

#define NUMSTRINGATTRS  3
char * stringAttrs[NUMSTRINGATTRS] = {"mac", "nm", "par"};
int    stringMaxlens[NUMSTRINGATTRS] = {16, 16, 16};

//-MQTT����
mqtt_client *m; //mqtt_client ����ָ��
char *topic = "iot_td/ZigBee/HA/NXP/"; //����
int topic_flags = 0;	//-0 û�гɹ���������,	1 ������������
int mqtt_client_connect_flags = 0;	//-0 ����������,1 ��������
char *host = "messagesight.demos.ibm.com:1883";//���Է�����
char *client_id = "clientid33883";//�ͻ���ID�� �Բ��Է��������������д
char *username = NULL;//�û�����������֤���ݡ��Բ��Է��������ޡ�
char *password = NULL;//���룬������֤���ݡ��Բ��Է��������ޡ�
	

void dbp_external_send_data_to_clients(char *data);

/* local function prototypes */
static void *zigbee_msg_receiver(void *arg);
static void *dbp_client_handler(void *arg);
static void dbp_msg_receiver(int socket_fd);
static void dbp_init(void);
static int dbp_update_battery(char *mac);
static int dbp_update_battery_lvl(char *mac);
static int dbp_update_lqi(char *mac);
static int dbp_update_location(char *mac);
static int dbp_update_parent_address(char *mac);
static int dbp_update_illuminance(char *mac);
static int dbp_update_humidity(char *mac);
static int dbp_update_temperature(char *mac);
static int dbp_get_index_number(void);
static void dbp_add_socket(int sock_fd, int index);
static void dbp_remove_socket(int index);
static int dbp_get_socket(int index);
static int dbp_send_data_to_clients(char *data);
static void dbp_location_info(char *mac);

static void mqtt_Pclient_connect(void);
int dbp_send_data_to_clients_mqtt(char *buf,int data);

int supplement_topic(void)
{
	uint16_t  shortAddress = 0;
	//-route010203040506/dongle010203040506/device0102030405060708/tmp
	//-Coordinator0102030405060708/
	//-DEBUG_PRINTF( "Get node extended address for 0x%04x\n", (int)shortAddress );
    
  newdb_zcb_t zcb;
  if ( newDbGetZcbSaddr( shortAddress, &zcb ) ) {
      strcat(topic,"Coordinator");  
			strcat(topic,zcb.mac);
			strcat(topic,"/");
			//-DEBUG_PRINTF( "sectional topic = %s\n", topic );
			topic_flags = 1;
  }
}

static void dbp_onError(int error, char * errtext, char * lastchars) {
    printf("onError( %d, %s ) @ %s\n", error, errtext, lastchars);
}

static void dbp_onObjectStart(char * name) {
    printf("onObjectStart( %s )\n", name);
    parsingReset();
}

static void dbp_onObjectComplete(char * name) {
    
    char *mac = NULL;
    bool status = false;
    
    printf("onObjectComplete\n");
    
    pthread_mutex_lock(&dbp_mutex);
    status = connection.connected;
    pthread_mutex_unlock(&dbp_mutex);
    
    if(strcmp(name, "sensor") == 0){//-�ж��Ƿ��Ǵ���������Ϣ,������ܾ�����ʪ�ȴ�������
        
        /* check location info first */
        mac = parsingGetStringAttr("mac");	//-������ʵ����ͨ��mac�ַ����ҵ��˾����mac��ֵ,�����һ�ִ洢����
        
        if( (isEmptyString(parsingGetStringAttr("nm")) == 0) ||
			(parsingGetIntAttr("xloc") != INT_MIN) ||
			(parsingGetIntAttr("yloc") != INT_MIN) ||
			(parsingGetIntAttr("zloc") != INT_MIN) ){
            
            printf("%s: location information received\n", __func__);
            
            /* location information */
            dbp_location_info(mac);	//-��ȡָ���豸��λ����Ϣ,ͨ��Mac��ַ�������豸
            
        }else{
            
            if(status == true){//-true˵����������״̬
                
                printf("%s: ok. status = %d\n", __func__, status);
                
                /* dummy calls */
                dbp_update_parent_address(mac);	//-ͨ���������������,�������������������ݿ�
                dbp_update_location(mac); 
                dbp_update_lqi(mac);	//-ò�������������ʾ�������Ŀ��,�ܶ�ʵ�ʲ�����û������
                
                dbp_update_battery(mac);	//-Ŀǰ������������ݵĽ�����
                if(dbp_update_battery_lvl(mac) == 0) return;
                if(dbp_update_illuminance(mac) == 0) return;
                if(dbp_update_humidity(mac) == 0) return;
                if(dbp_update_temperature(mac) == 0) return;	//-��������˿��ܵĵײ����ݷ���
                
            }else{
                printf("%s: error. status = %d\n", __func__, status);
            }
            
        }
        
    }else{
        printf("%s: error. name = %s\n", __func__, name);
    }
    
}

static void dbp_onArrayStart(char * name) {
    printf("onArrayStart( %s )\n", name);
}

static void dbp_onArrayComplete(char * name) {
    printf("onArrayComplete( %s )\n", name);
}

static void dbp_onString(char * name, char * value) {
    printf("onString( %s, %s )\n", name, value);
    parsingStringAttr( name, value );
}

static void dbp_onInteger(char * name, int value) {
    printf("onInteger( %s, %d )\n", name, value);
    parsingIntAttr( name, value );
}

static void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static void dbp_location_info(char *mac)
{
    char send_buf[512] = {0};
    
    if(isEmptyString(mac) == 0){
        
        if(isEmptyString(parsingGetStringAttr("nm")) == 0){
            if(dbp_report_location(parsingGetStringAttr("mac"), send_buf) == 0){
				/* send to clients */
                dbp_send_data_to_clients(send_buf);
            }
        }
        
    }else{
        printf("location information is empty");
    }
}

/*
������������һ�����߳�.
Ȼ����ÿ�οͻ������Ӻ�����һ���µ��߳�.
���ݿ��������洴���������߳�:
search_thread	���������߳�
listen_thread �����߳�
���ݿⱾ�ؽ��մ�����һ���߳�:
loc_receiver_thread	�������ӻ�ȡ��ַ��Ϣ
�������д�����һ��message�����߳�:
zb_msg_thread ���Ӧ�������ݿ�������

����߳���Ҫ��������ݿ�Ĺ�������,����Ĳ�����û��
*/
int main(int argc, char *argv[])
{
    int sockfd, new_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr;
    socklen_t sin_size;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    
    /* version number */
    sprintf(version, "Version %s%s", DBP_MAJOR_VERSION, DBP_MINOR_VERSION);
    
    newLogAdd( NEWLOG_FROM_DBP, "DBP started");
    newLogAdd( NEWLOG_FROM_DBP, version);
    
    /* init dbp */
    dbp_init();
    
    /* init dbp search */
    dbp_search_init();
    
    /* init dbp location receiver */
    dbp_loc_receiver_init();
    
    /* create thread for message receiver */
    if(pthread_create(&zb_msg_thread, NULL, &zigbee_msg_receiver, NULL) != 0){
        perror("Failed to create zigbee message receiver thread");
        return 1;
    }
        
    /* setup socket */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    if( (rv = getaddrinfo(NULL, DBP_PORT, &hints, &servinfo)) != 0 ) {
        printf("getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    for(p = servinfo; p != NULL; p = p->ai_next) {
        
        if( (sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1 ) {//-����һ���׽���
            perror("server: socket");
            continue;
        }
        
        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {//-�����������͡�����״̬�׽ӿڵ�����ѡ��ֵ��
            perror("setsockopt");
            exit(1);
        }
        
        /* disable Nagle's Algorithm to allow direct send  */
        if(setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int)) == -1) {
            perror("TCP_NODELAY");
        }
        //-bind���ǰ�һ�������ķ����ַ��ֻ�������ͻ��˲����ҵ��㡣����ȷ�������
        //-�ǲ����и�Ϊ������֪�ĵ�ַ�����ͻ�ȴ����Ҫ�Ǹ���ַ����Ϊ��ʱ���ǿͻ��Լ�
        //-�������еĵ�ַ�ҵ����еġ�
        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        
        break;
    }
    
    if(p == NULL) {
        newLogAdd( NEWLOG_FROM_DBP,"server: failed to bind");
        return 2;
    }
    
    freeaddrinfo(servinfo);
    
    if(listen(sockfd, 10) == -1) {//-��listen()�����׽ӿڲ�Ϊ�����������ӽ���һ������־��Ȼ������accept()���������ˡ�
        perror("listen");
        exit(1);
    }
    
    //-������������������溯���Ĺ���,��ʹ�ü򵥵�TCP��ֱ����MQTT������
    //create new mqtt client object
		m = mqtt_new(host, MQTT_PORT, client_id); //��������MQTT_PORT = 1883
		if ( m == NULL ) {
			printf("mqtt client create failure, return  code = %d\n", errno);
			//-return 1;
		} else {
			printf("mqtt client created\n");
		}
    
    mqtt_Pclient_connect();	//-����ά���ͷ�����������
    
    //-mqtt_Sclient_receiver();	//-���ն���
    
    while(1){//-������û�пͻ�������,����еĻ�ר�Ŵ���һ�����������߳�
        //-��������s�ĵȴ����Ӷ����г�ȡ��һ�����ӣ�����һ����sͬ����µ��׽ӿڲ����ؾ����
        //-����������޵ȴ����ӣ����׽ӿ�Ϊ������ʽ����accept()�������ý���ֱ���µ����ӳ��֡�
        //-����׽ӿ�Ϊ��������ʽ�Ҷ������޵ȴ����ӣ���accept()����һ������롣�ѽ������ӵ�
        //-�׽ӿڲ������ڽ����µ����ӣ�ԭ�׽ӿ��Ա��ֿ��š�
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *) &their_addr, &sin_size);	//-��һ���׽ӿڽ���һ�����ӡ�
        if(new_fd == -1) {
            perror("accept");
            continue;
        }
        
        inet_ntop(their_addr.ss_family, get_in_addr( (struct sockaddr *) &their_addr ), s, sizeof s);	//-���������������� ��> �����ʮ���ơ�
        printf("server: got connection from %s\n", s);
        
        dbp_msg_receiver(new_fd);
        
    }
    
    //-�����Ǻͷ������Ͽ�����,�������Ϊ�����Ӷ�������ϵ�л���
		mqtt_disconnect(m); //disconnect
		mqtt_delete(m);  //delete mqtt client object
    
    return 0;
}

static void *zigbee_msg_receiver(void *arg)	//-zigbee��Ϣ���մ���
{
    int i;
#if 0
    int cnt = 0;
#endif
    
    /* receive message */
    printf("Message Receiver's Thread is running\n");
    
    if( (dbpQueue = queueOpen(QUEUE_KEY_DBP, 0)) != -1 ){//-����Ϣ���е�ԭ�����оͷ��ض��б�ʶ��,���򴴽�һ��
    
        jsonSetOnError(dbp_onError);
        jsonSetOnObjectStart(dbp_onObjectStart);
        jsonSetOnObjectComplete(dbp_onObjectComplete);	//-�����ű������
        jsonSetOnArrayStart(dbp_onArrayStart);
        jsonSetOnArrayComplete(dbp_onArrayComplete);	//-�����ű�������
        jsonSetOnString(dbp_onString);
        jsonSetOnInteger(dbp_onInteger);
        jsonReset();	//-�Խ������ṹ�帳��ֵ
        
        int numBytes = 0;
        
        while(1){
        
            numBytes = queueRead(dbpQueue, inputBuffer, INPUTBUFFERLEN);
            printf("Message received. numBytes = %d\n", numBytes);
            
            jsonReset();	//-��λ������
            
            for(i=0; i<numBytes; i++){
                jsonEat(inputBuffer[i]);	//-һ���ֽ�һ���ֽڵĽ���������
            }
            
#if 0
            if(cnt++ > 10){
                cnt = 0;
                checkOpenFiles(4);
            }
#endif
        
        }
        
        queueClose(dbpQueue);
        
    }else{
        newLogAdd( NEWLOG_FROM_DBP,"Could not open dbp-queue");
    }
    
    return NULL;
}

static void *dbp_client_handler(void *arg)
{
    int byte_count;
    char buf[512];
    char output[512];
    int bytes_sent;
    int index = *((int *) arg);
    int socket_fd = dbp_get_socket(index);
    char *node_cmd = NULL;
    
    memset(buf, 0, sizeof(buf));
    memset(output, 0, sizeof(output));
    
    /* receive data from client here */
    while(1){
    
        byte_count = recv(socket_fd, buf, sizeof buf, 0);
        
        if(byte_count == 0){
            newLogAdd( NEWLOG_FROM_DBP,"Connection has been closed");
            dbp_remove_socket(index);
            break;
        }else if(byte_count < 0){
            /* something wrong has happened */
            newLogAdd( NEWLOG_FROM_DBP,"Connection error");
            dbp_remove_socket(index);
            break;
        }else{
            printf("Message received from client\n");
            /* pass it to dpb parser */
            dbp_parse_data(buf, sizeof(buf), output, 512, &node_cmd);	//-�Խ��յ�����Ϣ,�÷���������
            /* if it is close link command stop here */
            if(strcmp(output, "dbp close") == 0){//-���������ֱ�ӵ�һ���ͻ��������
                memset(buf, 0, sizeof(buf));
                memset(output, 0, sizeof(output));
                close(socket_fd);
                dbp_remove_socket(index);
                break;
            }else if(strcmp(output, "dbp node") == 0){
                if(node_cmd != NULL){
                    bytes_sent = send(socket_fd, node_cmd, strlen(node_cmd), 0);	//-�Կͻ��˽�����Ϣ��Ӧ,����ͷ����˳�ȥ
                }
            }else{
                bytes_sent = send(socket_fd, output, strlen(output), 0);
                memset(buf, 0, sizeof(buf));
                memset(output, 0, sizeof(output));
            }
        }
        
    }
    
    pthread_exit(NULL);
    
    return NULL;
}

void *mqtt_Pclient_handler(void *arg)	//-����̴߳�������һֱά���ͷ�����������,ֻҪ���������о���Ҫ����
{
		int byte_count;
    char buf[512];
    char output[512];
    int bytes_sent;
    int index = *((int *) arg);
    int ret; //����ֵ
    
    
    memset(buf, 0, sizeof(buf));
    memset(output, 0, sizeof(output));
    
    /* receive data from client here */
    while(1){
    
        if(mqtt_is_connected(m) == 0)
        {
        	if ( m == NULL ) 
        	{
        		//create new mqtt client object
						m = mqtt_new(host, MQTT_PORT, client_id); //��������MQTT_PORT = 1883
						if ( m == NULL ) {
							printf("mqtt client create failure, return  code = %d\n", errno);
							//-return 1;
						} else {
							printf("mqtt client created\n");
						}
        	}
        	else
        	{
	        	//connect to server
						ret = mqtt_connect(m, username, password); //���ӷ�����,���ﲻ������Ӳ�����ӻ���MQTTЭ������
						if (ret != MQTT_SUCCESS ) {
							printf("mqtt client connect failure, return code = %d\n", ret);
							usleep(100*1000);
						} else {
							printf("mqtt client connect\n");
							
							//-��������
							supplement_topic();
						}
						//-����һ�����̽����������������,Э�������ӿ���������һ���߳���ʵ�ֵ�
					}
        }
        	
        
    }
    
    pthread_exit(NULL);
    
    return NULL;
}

static void mqtt_Pclient_connect(void)
{
		pthread_t client_handler_thread;
        
    
    //-�����߳� thread��¼���̵߳�ID�� attr�����߳����� fn�߳�ִ�е������� ���к����Ĳ���
    if(pthread_create(&client_handler_thread, NULL, &mqtt_Pclient_handler, NULL) != 0){
        perror("Failed to create zigbee mqtt connect thread");
        return;
    }
    
    //-����һ���߳�Ĭ�ϵ�״̬��joinable��
    //- ���һ���߳̽������е�û�б�join,������״̬�����ڽ����е�Zombie Process,������һ��
    //-����Դû�б����գ��˳�״̬�룩�����Դ����߳���Ӧ��pthread_join���ȴ��߳����н�������
    //-�ɵõ��̵߳��˳����룬��������Դ��������wait,waitpid)
    //-���ǵ���pthread_join(pthread_id)��������߳�û�����н����������߻ᱻ����������Щ��
    //-�������ǲ���ϣ����ˣ�������Web�������е����߳�Ϊÿ�����������Ӵ���һ�����߳̽��д���
    //-��ʱ�����̲߳���ϣ����Ϊ����pthread_join����������Ϊ��Ҫ��������֮���������ӣ���
    //-��ʱ���������߳��м������
		//-pthread_detach(pthread_self())
		//-���߸��̵߳���
		//-pthread_detach(thread_id)�������������������أ�
		//-�⽫�����̵߳�״̬����Ϊdetached,����߳����н�������Զ��ͷ�������Դ��
    pthread_detach(client_handler_thread);	//-����Ϊ�����̲߳�����

}

#if 0
static void mqtt_Sclient_receiver(int socket_fd)
{
		pthread_t client_handler_thread;
        
    
    //-�����߳� thread��¼���̵߳�ID�� attr�����߳����� fn�߳�ִ�е������� ���к����Ĳ���
    if(pthread_create(&client_handler_thread, NULL, &mqtt_Sreceive_handler, NULL) != 0){
        perror("Failed to create zigbee mqtt receiver thread");
        return;
    }
    
    //-����һ���߳�Ĭ�ϵ�״̬��joinable��
    //- ���һ���߳̽������е�û�б�join,������״̬�����ڽ����е�Zombie Process,������һ��
    //-����Դû�б����գ��˳�״̬�룩�����Դ����߳���Ӧ��pthread_join���ȴ��߳����н�������
    //-�ɵõ��̵߳��˳����룬��������Դ��������wait,waitpid)
    //-���ǵ���pthread_join(pthread_id)��������߳�û�����н����������߻ᱻ����������Щ��
    //-�������ǲ���ϣ����ˣ�������Web�������е����߳�Ϊÿ�����������Ӵ���һ�����߳̽��д���
    //-��ʱ�����̲߳���ϣ����Ϊ����pthread_join����������Ϊ��Ҫ��������֮���������ӣ���
    //-��ʱ���������߳��м������
		//-pthread_detach(pthread_self())
		//-���߸��̵߳���
		//-pthread_detach(thread_id)�������������������أ�
		//-�⽫�����̵߳�״̬����Ϊdetached,����߳����н�������Զ��ͷ�������Դ��
    pthread_detach(client_handler_thread);	//-����Ϊ�����̲߳�����

}
#endif

static void dbp_msg_receiver(int socket_fd)
{
    pthread_t client_handler_thread;
    int index = dbp_get_index_number();
    
    if(index < 0){
        printf("%s: index = %d. error, reached max number of connection\n", __func__, index);
        return;
    }
    
    dbp_add_socket(socket_fd, index);	//-�������м�¼����
    //-�����߳� thread��¼���̵߳�ID�� attr�����߳����� fn�߳�ִ�е������� ���к����Ĳ���
    if(pthread_create(&client_handler_thread, NULL, &dbp_client_handler, (void *) &index) != 0){
        perror("Failed to create zigbee message receiver thread");
        return;
    }
    
    //-����һ���߳�Ĭ�ϵ�״̬��joinable��
    //- ���һ���߳̽������е�û�б�join,������״̬�����ڽ����е�Zombie Process,������һ��
    //-����Դû�б����գ��˳�״̬�룩�����Դ����߳���Ӧ��pthread_join���ȴ��߳����н�������
    //-�ɵõ��̵߳��˳����룬��������Դ��������wait,waitpid)
    //-���ǵ���pthread_join(pthread_id)��������߳�û�����н����������߻ᱻ����������Щ��
    //-�������ǲ���ϣ����ˣ�������Web�������е����߳�Ϊÿ�����������Ӵ���һ�����߳̽��д���
    //-��ʱ�����̲߳���ϣ����Ϊ����pthread_join����������Ϊ��Ҫ��������֮���������ӣ���
    //-��ʱ���������߳��м������
		//-pthread_detach(pthread_self())
		//-���߸��̵߳���
		//-pthread_detach(thread_id)�������������������أ�
		//-�⽫�����̵߳�״̬����Ϊdetached,����߳����н�������Զ��ͷ�������Դ��
    pthread_detach(client_handler_thread);	//-����Ϊ�����̲߳�����
    
    usleep(100*1000);
}
 
static void dbp_init(void)
{
    int i;
    
    for(i=0; i<NUMINTATTRS; i++){
        parsingAddIntAttr(intAttrs[i]);	//-����һ�����������ֵ���������,�������Ӧ�ø����ݿ�û�й�ϵ,��Ϊ�˲���׼����
    }
    
    for(i=0; i<NUMSTRINGATTRS; i++) {
        parsingAddStringAttr(stringAttrs[i], stringMaxlens[i]);	//-����һ���ַ������ֵ���������,�����Ŀ����ʲô?
    }
    
    connection.connected = false;	//-Ӧ���ǻ�û�пͻ������ӵ����ݿ�
    connection.conn_number = 0;
    memset(connection.sock_fd, 0, sizeof(connection.sock_fd));
}

static int dbp_update_battery(char *mac)
{
    int value;
    char output[100];
//    char output_capa[100];
    
    value = parsingGetIntAttr("bat");
    printf("%s: value = %d\n", __func__, value);
    if(value >= 0){
        if(dbp_report_bat((uint16_t) value, mac, output) == 0){
            dbp_send_data_to_clients(output);
            printf("bat voltage sent to client");
        }
    }else{
        printf("bat parsing failed");
        return -1;
    }
    
    return 0;
}

static int dbp_update_battery_lvl(char *mac)
{
    int value;
    char output[100];
    
    value = parsingGetIntAttr("batl");
    printf("%s: value = %d\n", __func__, value);
    if(value >= 0){
        if(dbp_report_bat_lvl((uint16_t) value, mac, output) == 0){
            dbp_send_data_to_clients(output);
            printf("bat level sent to client");
        }
    }else{
        printf("bat parsing failed");
        return -1;
    }
    
    return 0;
}

static int dbp_update_lqi(char *mac)
{
    int value;
    
    value = parsingGetIntAttr("lqi");
    if(value >= 0){
        //printf("lqi sent to client");
    }else{
        printf("lqi parsing failed");
        return -1;
    }
    
    return 0;
}

static int dbp_update_location(char *mac)
{
    char *loc = NULL;
    
    loc = parsingGetStringAttr("loc");
    if(loc != NULL){
        //if(dbp_report_location(0, 0, 0,  loc_id, mac, output) == 0){
        //    bytes_sent = send(connection.sock_fd, output, strlen(output), 0);
        //    printf("loc sent to client");
        //}
    }else{
        printf("location parsing failed");
        return -1;
    }
    
    return 0;
}

static int dbp_update_parent_address(char *mac)
{
    char *par = NULL;
    
    par = parsingGetStringAttr("par");
    if(par != NULL){
        //printf("par sent to client");
    }else{
        printf("parent parsing failed");
        return -1;
    }
    
    return 0;
}

static int dbp_update_illuminance(char *mac)
{
    int value;
    char output[100];
    
    value = parsingGetIntAttr("als");
    if(value >= 0){
        if(dbp_report_als((uint16_t) value, mac, output) == 0){
            dbp_send_data_to_clients(output);
            printf("als sent to client");
        }
    }else{
        printf("als parsing failed");
        return -1;
    }
    
    return 0;
}

static int dbp_update_humidity(char *mac)
{
    int value;
    char output[100];
    
    value = parsingGetIntAttr("hum");
    if(value >= 0){
        if(dbp_report_rh((uint16_t) value, mac, output) == 0){
            dbp_send_data_to_clients(output);
            printf("hum sent to client");
        }
    }else{
        printf("hum parsing failed");
        return -1;
    }
    
    return 0;
}

static int dbp_update_temperature(char *mac)
{
    int value;
    char output[100];
    
    value = parsingGetIntAttr("tmp");	//-ͨ���Ƚ����ֻ�ȡ�ڲ���Ӧ����,��������Ŀ�����ⲿ����,�ڲ�����ʹ��
	if(value != INT_MIN){
		if(dbp_report_temp(value, mac, output) == 0){//-������������ݽ�������֯,Ϊ������׼��
#if DBP_SUPPORT_clients_mode			
			dbp_send_data_to_clients(output);	//-������¶ȷ��͸��˿ͻ���,��������ײ�
#else
			dbp_send_data_to_clients_mqtt(output, value);
#endif			
			printf("temp sent to client");
		}
	}
    
    return 0;
}

static int dbp_get_index_number(void)	//-����һ���յ�������Ϣ������
{
    int i;
    int index = 0;
    
    pthread_mutex_lock(&dbp_mutex);
    
    for(i=0; i<MAX_NUM_OF_CONN; i++){
        if(connection.sock_fd[i] == 0){//-Ϊ0˵������ϻ�����,û�м�¼������Ϣ
            index = i;
            break;
        }
    }
    
    if(i >= MAX_NUM_OF_CONN){
        index = -1;
    }
    
    pthread_mutex_unlock(&dbp_mutex);
    
    return index;
}

static void dbp_add_socket(int sock_fd, int index)	//-�Ѷ�Ӧ���׽�����Ϣ��һ�������еĽṹ���¼����
{
    pthread_mutex_lock(&dbp_mutex);
    
    if(connection.conn_number <= MAX_NUM_OF_CONN){
        connection.sock_fd[index] = sock_fd;
        connection.conn_number++;	//-��¼��һ�����ڵ�������
        connection.connected = true;	//-˵���Ѿ�����������
    }else{
        printf("%s: conn_number = %d. error, reached max number of connection\n", __func__, connection.conn_number);
    }
    
    pthread_mutex_unlock(&dbp_mutex);
}

static void dbp_remove_socket(int index)
{
    pthread_mutex_lock(&dbp_mutex);
    
    connection.sock_fd[index] = 0;
    if(connection.conn_number > 0){
        connection.conn_number--;
        if(connection.conn_number <= 0){
            connection.connected = false;
        }
    }else{
        printf("%s: conn_number = %d. error, no connection\n", __func__, connection.conn_number);
    }
    
    pthread_mutex_unlock(&dbp_mutex);
}

static int dbp_get_socket(int index)	//-��ö�Ӧ�̵߳��׽���������
{
    return connection.sock_fd[index];
}

void dbp_external_send_data_to_clients(char *data)
{
    dbp_send_data_to_clients(data);
}

static int dbp_send_data_to_clients(char *data)	//-���ݿ�����ݷ��͸��ͻ���ͨ���׽���
{
    int i, j;
    int bytes_sent = 0;
    
    if(data == NULL) return -1;
    
    if(connection.connected == false) return -1;
    
    pthread_mutex_lock(&dbp_mutex);
    
    for(i=0, j=0; i<MAX_NUM_OF_CONN; i++){
        if(connection.sock_fd[i] != 0){
            j++;
            bytes_sent = send(connection.sock_fd[i], data, strlen(data)+1, 0);
            printf("%s: %d bytes sent\n", __func__, bytes_sent);
            usleep(1000); /* 1ms delay */
            if(j >= connection.conn_number){
                break;
            }
        }
    }
    
    pthread_mutex_unlock(&dbp_mutex);
    
    return 0;
}

int dbp_send_data_to_clients_mqtt(char *buf,int data)
{
	int ret; //����ֵ
	int Qos; //Quality of Service
	
	if(topic_flags == 1)
	{
		//publish message
		Qos = QOS_EXACTLY_ONCE; //Qos
		strcat(topic,buf);
		if ( m != NULL )
			ret = mqtt_publish(m, topic, data, Qos);//������Ϣ
		printf("mqtt client publish,  return code = %d\n", ret);
	}
	else
	{//-û�к������峢�Բ���һ��
		supplement_topic();
		printf("mqtt topic is bad\n");
	}
	
}

