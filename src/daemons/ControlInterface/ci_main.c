// ------------------------------------------------------------------
// Control Interface socket program
// ------------------------------------------------------------------
// IoT program that handles control requests from the LAN, e.g.
// DBGET, control and topology
// ------------------------------------------------------------------
// Author:    nlv10677
// Copyright: NXP B.V. 2014. All rights reserved
// ------------------------------------------------------------------

/** \file
 * \section controlinterface Control Interface
 * \brief Control Interface socket program that handles control requests from the LAN
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <mqueue.h>
#include <limits.h>
#include <time.h>
#include <pthread.h>

#include "parsing.h"
#include "json.h"
#include "jsonCreate.h"
#include "socket.h"
#include "systemtable.h"
#include "newDb.h"
#include "dump.h"
#include "newLog.h"

#include "topo.h"
#include "dbget.h"
#include "dbEdit.h"
#include "ctrl.h"
#include "sensor.h"
#include "cmd.h"
#include "lmp.h"
#include "grp.h"
#include "proc.h"
#include "iotError.h"
#include "iotSleep.h"
#include "gateway.h"

#include "mqtt_client.h"

#define SOCKET_HOST         "0.0.0.0"
#define SOCKET_PORT         "2001"

#define INPUTBUFFERLEN      200

 #define MAIN_DEBUG

#ifdef MAIN_DEBUG
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif /* MAIN_DEBUG */

#define MAXCHILDREN   10

// -------------------------------------------------------------
// Globals
// -------------------------------------------------------------

int globalClientSocketHandle;

// -------------------------------------------------------------
// Globals, local
// -------------------------------------------------------------

static int running = 1;
static int parent  = 1;

static pid_t children[MAXCHILDREN];

static char socketHost[80];
static char socketPort[80];

static mqtt_client *m; //mqtt_client 对象指针
//-char *host = "messagesight.demos.ibm.com:1883";//测试服务器
//-char *topic = "iot_td/ZigBee/HA/NXP/"; //主题
static char host[80] = "iot.eclipse.org:1883";
static char topic[80] = "iot_td/ZigBee/HA/NXP/";
static char *client_id = "clientid33883";//客户端ID； 对测试服务器，可以随便写
static char *username = NULL;//用户名，用于验证身份。对测试服务器，无。
static char *password = NULL;//密码，用于验证身份。对测试服务器，无。
static int Qos; //Quality of Service

//-mqtt

int supplement_topic(char *buff)
{
	uint16_t  shortAddress = 0;
	//-route010203040506/dongle010203040506/cmd
	//-Coordinator0102030405060708/cmd
	DEBUG_PRINTF( "Get node extended address for 0x%04x\n", (int)shortAddress );
    
  newdb_zcb_t zcb;
  if ( newDbGetZcbSaddr( shortAddress, &zcb ) ) {
      strcat(buff,"Coordinator");  
			strcat(buff,zcb.mac);
			strcat(buff,"/cmd");
			DEBUG_PRINTF( "sectional topic = %s\n", buff );
			//-topic_flags = 1;
  }
}


// -------------------------------------------------------------
// Child handling
// -------------------------------------------------------------

static int addChild( int pid ) {
    int i;
    for ( i=0; i<MAXCHILDREN; i++ ) {
        if ( children[i] == (pid_t)0 ) {
            DEBUG_PRINTF( "Add child %d to %d\n", pid, i );
            children[i] = pid;
            return( i );
        }
    }
    printf( "** ERROR: Add child %d\n", pid );
    return( -1 );
}

static int remChild( int pid ) {
    int i;
    for ( i=0; i<MAXCHILDREN; i++ ) {
        if ( children[i] == pid ) {
            DEBUG_PRINTF( "Rem child %d from %d\n", pid, i );
            children[i] = (pid_t)0;
            return( i );
        }
    }
    printf( "** ERROR: rem child %d\n", pid );
    return( -1 );
}

static int numChildren( void ) {
    int i, cnt = 0;
    for ( i=0; i<MAXCHILDREN; i++ ) {
        if ( children[i] != (pid_t)0 ) cnt++;
    }
    return( cnt );
}

static void killAllChildren( void ) {
    DEBUG_PRINTF( "Kill all children\n" );
    int i;
    for ( i=0; i<MAXCHILDREN; i++ ) {
        kill( children[i], SIGKILL );
    }
}

static int killOldest( void ) {
    int i, oldest = INT_MAX;
    for ( i=0; i<MAXCHILDREN; i++ ) {
        if ( children[i] < oldest ) oldest = children[i];
    }
    if ( oldest != INT_MAX ) {
        DEBUG_PRINTF( "Kill oldest child %d from %d\n",
                      children[oldest], oldest );
        kill( children[oldest], SIGKILL );
    } else {
        printf( "** ERROR: kill oldest child\n" );
    }
    return( oldest );
}

static void initChildren( void ) {
    int i;
    for ( i=0; i<MAXCHILDREN; i++ ) {
        children[i] = (pid_t)0;
    }
}

// -------------------------------------------------------------
// Send socket response
// -------------------------------------------------------------

void sendResponse( char * jsonResponseString ) {
    if ( jsonResponseString != NULL ) {
        DEBUG_PRINTF( "jsonResponse = %s, globalSocket = %d\n",
               jsonResponseString, globalClientSocketHandle );
#ifdef MAIN_DEBUG
        sprintf( logbuffer, "Response - %s", jsonResponseString );
        newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, logbuffer );
#endif
        if ( socketWrite( globalClientSocketHandle,
                     jsonResponseString, strlen(jsonResponseString) ) < 0 ) {
            if ( errno != EPIPE ) {   // Broken pipe
                sprintf( logbuffer, "Socket write failed(%s)",
                         strerror(errno));
                newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, logbuffer );
            }
        }
    }
}

// -------------------------------------------------------------
// Parsing
// -------------------------------------------------------------

static void ci_onError(int error, char * errtext, char * lastchars) {
    printf("onError( %d, %s ) @ %s\n", error, errtext, lastchars);
}

static void ci_onObjectStart(char * name) {
    DEBUG_PRINTF("onObjectStart( %s )\n", name);
    parsingReset();	//-每次开始解析一个新的输入语句之前都需要服务下解析器,把前面的数据抹掉
}

static void ci_onObjectComplete(char * name) {
    DEBUG_PRINTF("onObjectComplete( %s )\n", name);
    char * jsonResponseString = NULL;
    int ret, error = IOT_ERROR_NONE;
    
    sprintf( logbuffer, "Command: %s", name );
    newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, logbuffer );

    if ( strcmp( name, "dbget" ) == 0 ) {             // From phone
        if ( !dbgetHandle() ) {
            error = iotError;
        }

    } else if ( strcmp( name, "dbedit" ) == 0 ) {     // From NFC reader
        if ( !dbEditHandle() ) {	//-这里组织了各种设备之间的协调处理
            error = iotError;
        }

    } else if ( strcmp( name, "ctrl" ) == 0 ) {       // From phone or website
        if ( ctrlHandle() < 0 ) {
            error = iotError;
        }

    } else if ( strcmp( name, "cmd" ) == 0 ) {        // From control website
        if ( cmdHandle() < 0 ) {
            error = iotError;
        }

    } else if ( strcmp( name, "lmp" ) == 0 ) {        // From phone or website
        if ( lmpHandle() < 0 ) {
            error = iotError;
        }

    } else if ( strcmp( name, "grp" ) == 0 ) {        // From phone or website
        if ( grpHandle() < 0 ) {
            error = iotError;
        }

    } else if ( strcmp( name, "sensor" ) == 0 ) {     // From WSN phone (DBP)
        if ( sensorHandle() < 0 ) {
            error = iotError;
        }

    } else if ( ( strcmp( name, "tp" ) == 0 ) ||
                ( strcmp( name, "tp+" ) == 0 ) ) {        // From phone
        ret = topoHandle();
        if ( ret < 0 ) {
            error = iotError;
        } else if ( ret ) {
            jsonResponseString = jsonTopoStatus( 1 );
        }

    } else if ( strcmp( name, "proc" ) == 0 ) {     // From website
        if ( !procHandle() ) {
            error = iotError;
        }

    }

    if ( ( jsonResponseString == NULL ) && ( error != IOT_ERROR_NONE ) ) {
        jsonResponseString = jsonError( error );
    }

    sendResponse( jsonResponseString );

    // Reset global error after each completed object
    iotError = IOT_ERROR_NONE;
}

static void ci_onArrayStart(char * name) {
    // printf("onArrayStart( %s )\n", name);
}

static void ci_onArrayComplete(char * name) {
    // printf("onArrayComplete( %s )\n", name);
}

static void ci_onString(char * name, char * value) {
    DEBUG_PRINTF("onString( %s, %s )\n", name, value);
    parsingStringAttr( name, value );	//-根据接收到的内容设值
}

static void ci_onInteger(char * name, int value) {
    DEBUG_PRINTF("onInteger( %s, %d )\n", name, value);
    parsingIntAttr( name, value );
}

// -------------------------------------------------------------
// Byte convertion before processing
// -------------------------------------------------------------

static char convertPrintable( char c ) {
    if ( c >= ' ' && c <= '~' ) return( c );
    if ( c == '\n' ) return( c );
    if ( c == '\r' ) return( c );
    return( '.' );
}

// ------------------------------------------------------------------------
// Handle client
// ------------------------------------------------------------------------

/**
 * \brief Client child process to parse and handle the JSON commands.
 * \param socketHandle Handle to the client's TCP socket
 */

static void handleClient( int socketHandle ) {//-负责对客户端发来的消息进行解析处理

    newDbOpen();
    
    // printf( "Handling client %d\n", socketHandle );
    globalClientSocketHandle = socketHandle;

    // Start with clean sheet
    iotError = IOT_ERROR_NONE;
    int ok = 1;

    char socketInputBuffer[INPUTBUFFERLEN + 2];

    jsonReset();

    while( ok ) {
        int len;
        // len = socketRead( socketHandle, socketInputBuffer, INPUTBUFFERLEN ); 
        len = socketReadWithTimeout( socketHandle, 
                  socketInputBuffer, INPUTBUFFERLEN, 10 ); 
        if ( len <= 0 ) {//-连接断开了进程就可以退出了
            ok = 0;
        } else {
            int i;
            for ( i=0; i<len; i++ ) {//-把读取的网络套接字转存到待处理的地方
                // Transfer all non-printable bytes into '.'
                socketInputBuffer[i] = convertPrintable( socketInputBuffer[i] );
            }

#ifdef MAIN_DEBUG
            // printf( "Incoming packet '%s', length %d\n",
            //          socketInputBuffer, len );
            dump( socketInputBuffer, len );
#endif
            sprintf( logbuffer, "Socket - %s", socketInputBuffer );
            newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, logbuffer );
            
            for ( i=0; i<len; i++ ) {
                jsonEat( socketInputBuffer[i] );
            }
        }
    }

    newDbClose();
}

static void handleMqttMessage( mqtt_client *m) {//-对接收到的一条有效消息进行处理
	
		//-m->received_topic; 
		char *mqttInput_pt = m->received_message;
		int len = m->received_message_len;
	
		int i;
            for ( i=0; i<len; i++ ) {//-把读取的网络套接字转存到待处理的地方
                // Transfer all non-printable bytes into '.'
                mqttInput_pt[i] = convertPrintable( mqttInput_pt[i] );
            }

#ifdef MAIN_DEBUG
            // printf( "Incoming packet '%s', length %d\n",
            //          socketInputBuffer, len );
            dump( mqttInput_pt, len );
#endif
            sprintf( logbuffer, "MQTT - %s", mqttInput_pt );
            newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, logbuffer );
            
            for ( i=0; i<len; i++ ) {
                jsonEat( mqttInput_pt[i] );
            }
}
// -------------------------------------------------------------
// DB saver
// -------------------------------------------------------------

/**
 * \brief Background thread that saves the IoT Database from shared-memory to file 
 * when updates are available in a rate of once per 10 seconds.
 * \param arg Not used
 * \retval NULL Null pointer
 */

static void * dbSaveThread( void * arg ) {//-此线程实现周期备份数据库的功能
    int running = *((int *)arg);
    sleep( 5 );
    while ( running ) {
        newDbFileLock();
        newDbSave();
        newDbFileUnlock();
        sleep( 10 );
    }
    return NULL;
}

// -------------------------------------------------------------
// Signal handler to kill all children
// -------------------------------------------------------------

static void vQuitSignalHandler (int sig) {

    int pid, status;
    DEBUG_PRINTF("Got signal %d (%d)\n", sig, parent);

    switch ( sig ) {
    case SIGTERM:
    case SIGINT:
    case SIGKILL:
        DEBUG_PRINTF("Switch off (%d)\n", parent);
        running = 0;
        if ( parent ) {
            killAllChildren();
        }
        exit(0);
        break;
    case SIGCHLD:
        while ( ( pid = waitpid( -1, &status, WNOHANG ) ) > 0 ) {
            remChild( pid );
            DEBUG_PRINTF("Child %d stopped with %d\n", pid, status);
        }
        break;
    }

    signal (sig, vQuitSignalHandler);
}

// ------------------------------------------------------------------------
// Mqtt
// ------------------------------------------------------------------------
static void * MqttHandleThread( void * arg ) {//-此线程实现周期备份数据库的功能
    int running = *((int *)arg);
		int ret; //返回值    
    sleep( 5 );
    jsonReset();
    while ( running ) {
        //-持续保持MQTT连接
				if(mqtt_is_connected(m) == 0)
				{//-判断连接是否存在,如果不再了,下面尝试再连
					//connect to server
					ret = mqtt_connect(m, username, password); //连接服务器
					if (ret != MQTT_SUCCESS ) {
						printf("mqtt client connect failure, return code = %d\n", ret);
						newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, "mqtt client connect failure\n" );
					} else {
						printf("mqtt client connect\n");
						newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, "mqtt client connect\n" );
						
						//subscribe
						Qos = QOS_EXACTLY_ONCE;
						//-补充主题
						supplement_topic(topic);
						ret = mqtt_subscribe(m, topic, Qos);//订阅消息
						printf("mqtt client subscribe %s,  return code = %d\n", topic, ret);
					}
					mqtt_sleep(10000); //sleep a while
				}
				else
				{
					int timeout = 200;
					if ( mqtt_receive(m, timeout) == MQTT_SUCCESS ) { //recieve message，接收消息
						printf("received Topic=%s, Message=%s\n", m->received_topic, m->received_message);
						handleMqttMessage(m);
					}
					mqtt_sleep(200); //sleep a while
				}
    }
    return NULL;
}
///////////////////////////////////////////////////////////////////////////

// ------------------------------------------------------------------------
// Main
// ------------------------------------------------------------------------

/**
 * \brief Control Interface's main entry point: opens the IoT database,
 * sets a start marker in the IoT database (for the SW update mechanism),
 * starts the IoT Database Save thread,
 * initializes the JSON parsers and opens the server socket to wait for clients.
 * Each client gets its own (forked) child process to handle the commands.
 * When there are too many clients, then the oldest one is killed (could be a hangup).
 * \param argc Number of command-line parameters
 * \param argv Parameter list (-h = help, -H <ip> is IP address, -P <port> = TCP port, -c = empty DB and exit immediately)
 */

int main( int argc, char * argv[] ) {
    int skip;
    signed char opt;
		
    
    initChildren();	//-给一个结构赋初值

    // Install signal handlers
    signal(SIGTERM, vQuitSignalHandler);
    signal(SIGINT,  vQuitSignalHandler);
    signal(SIGCHLD, vQuitSignalHandler);

    printf( "\n\nControl Interface Started\n" );

    strcpy( socketHost, SOCKET_HOST );
    strcpy( socketPort, SOCKET_PORT );

    while ( ( opt = getopt( argc, argv, "hH:P:c" ) ) != -1 ) {//-根据启动时候的参数进行部分操作
        switch ( opt ) {
        case 'h':
            printf( "Usage: ci [-H host] [-P port] [-c]\n\n");
            exit(0);
        case 'H':
            strcpy( socketHost, optarg );
            strcpy( host, optarg );
            break;
        case 'P':
            strcpy( socketPort, optarg );
            break;
        case 'c':
            printf( "Empty DB\n\n" );
            newDbOpen();
            newDbEmptySystem();
            newDbEmptyDevices( MODE_DEV_EMPTY_ALL );
            newDbEmptyPlugHist();
            newDbEmptyZcb();
            newDbClose();
            exit( 0 );
            break;
        }
    }

    newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, "Control Interface Started" );
    
    newDbOpen();	//-有就打开,没有就创建
    
    // Install DB save thread
    pthread_t tid;
    int err = pthread_create( &tid, NULL, &dbSaveThread, (void *)&running);
    if (err != 0) {
        printf("Can't create DB thread :[%s]", strerror(err));
    } else {
        DEBUG_PRINTF("DB thread created successfully\n");
    }
    
#if 0
    // Test
    int lup;
    lup = newDbGetLastupdateClimate();
    printf( "lup %d\n", lup );
#endif

    // Add sysstart timestamp in system dB
    newDbSystemSaveIntval( "sysstart", (int )time(NULL) );	//-实实在在向数据库中填写了数据,但是仅仅是填写到了共享内存中,到文件中还需要其他写
    
    newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, "Opening host socket" );

    int serverSocketHandle = socketOpen( socketHost, socketPort, 1 );
    //-下面这个函数代替上面函数的功能,不使用简单的TCP而直接走MQTT服务器
    //create new mqtt client object
		m = mqtt_new(host, MQTT_PORT, client_id); //创建对象，MQTT_PORT = 1883
		if ( m == NULL ) {
			printf("mqtt client create failure, return  code = %d\n", errno);
			//-return 1;
		} else {
			printf("mqtt client created\n");
		}
	
    //-if ( serverSocketHandle >= 0 ) {
    if (( serverSocketHandle >= 0 ) || ( m != NULL )) {
        //-好多程序里面都使用了这样的解析器,但是实际就定义了两个,所以应该是局部变量,这样每个程序段中都有,内部保持不变
        jsonSetOnError(ci_onError);
        jsonSetOnObjectStart(ci_onObjectStart);
        jsonSetOnObjectComplete(ci_onObjectComplete);	//-这里的一切是控制解析器的
        jsonSetOnArrayStart(ci_onArrayStart);
        jsonSetOnArrayComplete(ci_onArrayComplete);
        jsonSetOnString(ci_onString);
        jsonSetOnInteger(ci_onInteger);
        jsonReset();

        // printf( "Init parser ...\n" );

        topoInit();
        dbgetInit();
        dbEditInit();
        ctrlInit();
        sensorInit();
        cmdInit();
        lmpInit();
        grpInit();
        procInit();
        
        printf( "Waiting for incoming requests from socket %s/%s ...\n",
            socketHost, socketPort );
            
        // Install MQTT handle thread
        pthread_t ci_mqtt_handle_tid;
        int err = pthread_create( &ci_mqtt_handle_tid, NULL, &MqttHandleThread, (void *)&running);
        if (err != 0) {
            printf("Can't create MQTT handle thread :[%s]", strerror(err));
        } else {
            DEBUG_PRINTF("MQTT handle thread created successfully\n");
        }

        while ( running ) {

            iotError = IOT_ERROR_NONE;

            int clientSocketHandle = socketAccept( serverSocketHandle );
            // printf( "Sockethandle = %d\n", clientSocketHandle );
            if ( clientSocketHandle >= 0 ) {
                if ( running ) {
                    
#if 1
                    pid_t childPID;
                    childPID = fork();	//-创建一个与原来进程几乎完全相同的进程

                    if ( childPID < 0 ) {
                        newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, "Error creating child process\n" );

                    } else if ( childPID == 0 ) {
                        // Child
                        parent = 0;
                        errno  = 0;
                        printf( "+++ Client %d - %d\n", getpid(), errno );
                        socketClose( serverSocketHandle );
                        handleClient( clientSocketHandle );
                        // printf( "============================ %d\n", getpid() );
                        socketClose( clientSocketHandle );
                        char * reason = strerror(errno);
                        if      ( errno == EPIPE ) reason = "client left";
                        else if ( errno == EAGAIN ) reason = "inactive timeout";
                        printf( "--- Client %d - %s\n", getpid(), reason );
                        exit( 0 );

                    } else {
                        // Parent
                        socketClose( clientSocketHandle );
                        if ( addChild( childPID ) >= 0 ) {//-把进程ID存储起来
                            int num = numChildren();
                            if ( num >= MAXCHILDREN ) {
                                // Max number of children received: wait for
                                // at least one to finish within 10 secs
                                int cnt = 50;
                                do {
                                    IOT_MSLEEP( 200 );
                                } while ( ( cnt-- > 0 ) &&
                                          ( numChildren() >= MAXCHILDREN ) );
                                if ( cnt < 0 ) {
                                    // No child stopped within 10 secs:
                                    // - kill oldest
                                    killOldest();
                                }
                            } else if ( num > ( MAXCHILDREN / 2 ) ) {
                                IOT_MSLEEP( 200 );
                            }
                        } else {
                            // HAZARD - Too many children: kill the new one
                            kill( childPID, SIGKILL );
                        }
                    }
#else
                    handleClient( clientSocketHandle );
                    socketClose( clientSocketHandle );
#endif
                } else {
                    socketClose( clientSocketHandle );
                }
            }

            if ( skip++ > 10 ) {//-最大10个子进程
                skip = 0;
                checkOpenFiles( 5 );   // STDIN,STDOUT,STDERR,ServerSocket,DB
            }
        }

        // printf( "Control Interface exit\n");
        socketClose( serverSocketHandle );
        
        mqtt_disconnect(m); //disconnect
				printf("mqtt client disconnect");
				mqtt_delete(m);  //delete mqtt client object

    } else {
        sprintf( logbuffer, "Error opening Control Interface socket %s/%s",
                 socketHost, socketPort );
        newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, logbuffer );
    }
    
    newDbClose();

    newLogAdd( NEWLOG_FROM_CONTROL_INTERFACE, "Exit" );
    
    return( 0 );
}
