// ------------------------------------------------------------------
// IOT Semaphore
// ------------------------------------------------------------------
// Author:    nlv10677
// Copyright: NXP B.V. 2014. All rights reserved
// ------------------------------------------------------------------

/** \file
 * \brief Friendly sempahore interface
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <unistd.h>

#include "iotSemaphore.h"

// #define SEM_DEBUG

#ifdef SEM_DEBUG
#define DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#define DEBUG_PRINTF(...)
#endif /* SEM_DEBUG */

// #define AUTO_DEADLOCK_SOLVE   1

// ------------------------------------------------------------------
// Fix: Proto missing in system header files
// ------------------------------------------------------------------

extern int semtimedop(int semid, struct sembuf *sops, unsigned nsops,
               struct timespec *timeout);

// ------------------------------------------------------------------
// Get the semaphore, create when not yet exists
// ------------------------------------------------------------------

/**
 * \brief Get/create a semaphore
 * \param startval Start value (0 or 1), depending on the first use
 * \param key Unique (random) number that identifies this semaphore
 * \return semId on success, 0 on error
 */
int semInit( int startval, int key ) {
    int semId = semget( key, 1, 0666 | IPC_CREAT );
    if ( semId < 0 ) {
        printf( "Unable to obtain semaphore\n" );
    } else {

        union semun {
            int val;
            struct semid_ds * buf;
            ushort * array;
        } argument;

        argument.val = startval;

        if ( semctl( semId, 0, SETVAL, argument ) < 0 ) {
            printf( "Cannot set semaphore value\n" );
        } else {
            DEBUG_PRINTF( "Semaphore initialized\n" );
        }
        return semId;
    }
    return 0;
}

// ------------------------------------------------------------------
// P/V operations
// ------------------------------------------------------------------

/**
 * \brief P-operation on semaphore
 * \param key Key to the semaphore
 */
void semP( int key ) {
    int semId = semget( key, 1, 0666 );
    if ( semId < 0 ) {
        // Semaphore does not exist yet: create
        semId = semInit( 1, key );
        // Return on failure
        if ( !semId ) return;
    }

    struct sembuf operations[1];
    operations[0].sem_num = 0;
    operations[0].sem_op  = -1;
    operations[0].sem_flg = 0;

#ifdef AUTO_DEADLOCK_SOLVE
    struct timespec ts;
    ts.tv_sec = 10;
    ts.tv_nsec = 0;

    int retval = semtimedop( semId, operations, 1, &ts );
    if ( retval == 0 ) {
        DEBUG_PRINTF( "Successful P-operation %d (PID=%d)\n", key, getpid() );
    } else {
        printf( "P-operation did not succeed %d\n", key );
        // Recreate semaphore
        semId = semInit( 1, key );
    }
#else
    int retval = semop( semId, operations, 1 );
    if ( retval == 0 ) {
        DEBUG_PRINTF( "Successful P-operation %d (PID=%d)\n", key, getpid() );
    } else {
        printf( "P-operation did not succeed %d\n", key );
    }
#endif
}

/**
 * \brief P-operation on semaphore, time-s out after <sec> seconds
 * \param key Key to the semaphore
 * \param secs Seconds to wait
 * \returns 1 on success, 0 on timeout or error
 */
int semPtimeout( int key, int secs ) {
		//-当调用semget创建一个信号量时，他的相应的semid_ds结构被初始化。
    int semId = semget( key, 1, 0666 );	//-获取与某个键关联的信号量集标识
    if ( semId < 0 ) {//-如果成功，则返回信号量集的IPC标识符。如果失败，则返回-1
        // Semaphore does not exist yet: create
        semId = semInit( 1, key );	//-初始化一个定位在 sem 的匿名信号量。
        // Return on failure
        if ( !semId ) return 0;
    }

    struct sembuf operations[1];
    operations[0].sem_num = 0;
    operations[0].sem_op  = -1;
    operations[0].sem_flg = 0;

    struct timespec ts;
    ts.tv_sec = secs;
    ts.tv_nsec = 0;

    int retval = semtimedop( semId, operations, 1, &ts );	//-这个应该不是最顶层的库函数,但是被成功封装了,可以作为库来使用
    if ( retval == 0 ) {
        DEBUG_PRINTF( "Successful P-operation %d (PID=%d)\n", key, getpid() );
	return( 1 );
    }
    return( 0 );
}

/**
 * \brief P-operation on semaphore, time-s out after <sec> seconds. When timed-out, recreate the
 * semaphore to auto-unlock the system
 * \param key Key to the semaphore
 * \param secs Seconds to wait
 */
void semPautounlock( int key, int secs ) {

    if ( !semPtimeout( key, secs ) ) {
        printf( "DB P-operation did not succeed: perform auto-unlock %d\n", key );
        // Recreate semaphore
        semInit( 1, key );
    }
}

/**
 * \brief V-operation on semaphore
 * \param key Key to the semaphore
 */
void semV( int key ) {//-PV 操作通过调用semop函数来实现。
    int semId = semget( key, 1, 0666 );
    if ( semId < 0 ) {
        // Semaphore does not exist yet: create
        semId = semInit( 0, key );
        // Return on failure
        if ( !semId ) return;
    }

    struct sembuf operations[1];
    operations[0].sem_num = 0;
    operations[0].sem_op  = 1;
    operations[0].sem_flg = 0;

#ifdef AUTO_DEADLOCK_SOLVE
    struct timespec ts;
    ts.tv_sec = 10;
    ts.tv_nsec = 0;

    int retval = semtimedop( semId, operations, 1, &ts );
    if ( retval == 0 ) {
        DEBUG_PRINTF( "Successful V-operation %d\n", key );
    } else {
        printf( "V-operation did not succeed %d\n", key );
        // Recreate semaphore
        if ( !semInit( 0, key ) ) return;
    }
#else
    int retval = semop( semId, operations, 1 );	//-信号量的值与相应资源的使用情况有关，当它的值大于 0 时，表示当前可用的资源数的数量；当它的值小于 0 时，其绝对值表示等待使用该资源的进程个数。
    if ( retval == 0 ) {
        DEBUG_PRINTF( "Successful V-operation %d\n", key );
    } else {
        printf( "V-operation did not succeed %d\n", key );
    }
#endif
}

// ------------------------------------------------------------------
// Clear the semaphore (to be used in case of a deadlock)
// ------------------------------------------------------------------

/**
 * \brief P-operation on semaphore
 * \param key Key to the semaphore
 */
void semClear( int key ) {
    int semId = semget( key, 1, 0666 );
    int semnum = 0;
    semctl( semId, semnum, IPC_RMID );
}

