#ifndef __BINDER_IPC_H__
#define __BINDER_IPC_H__

#include "binder_common.h"
#include "binder_io.h"
#include "binder_hal.h"

#define MAX_THREAD_BUF_LEN 256

typedef enum gbinder_status {
    BINDER_STATUS_OK = 0,
    BINDER_STATUS_IO_ERROR,
    BINDER_STATUS_AGAIN,
    BINDER_STATUS_UNKNOWN,
    BINDER_STATUS_TIMEOUT,
    BINDER_STATUS_FAILED,
    BINDER_STATUS_DEAD_OBJECT
} BINDER_STATUS;


struct binder_ipc_tinfo{
    struct binder_state *bs;
    struct binder_buf in_buf;
    struct binder_buf out_buf;
    int        reply_status_code;
    int        txn_status;
};


/**********binder service structions***********/
struct binder_service{
    int (*on_transact)(uint32_t, struct binder_io *, struct binder_io *, uint32_t);
    int (*link_to_death)(void *);
    int (*unlink_to_death)(void *);
    int (*death_notify)(void *);
};

struct _binder_thread_data{
    int isMain;
    char t_name[32];
};


/***************************************
* Binder command functions
* For the details information, refer to binder_driver_command_protocol
* The data struct for each cmd, please also refer to binder_driver_command_protocol
*******************************************/
/*
* call the target functions according to the code in sync mode
* For BC_TRANSACTION cmd type
*/
int binder_cmd_sync_call(struct binder_ipc_tinfo *ti,struct binder_io *bio, struct binder_io *msg,uint32_t target, uint32_t code);

/*
* call the target functions according to the code in async mode
* For BC_TRANSACTION cmd type
*/

int binder_cmd_async_call(struct binder_ipc_tinfo *ti,struct binder_io *bio, struct binder_io *msg,uint32_t target, uint32_t code);


/*
* For BC_ACQUIRE, just write buffer into thread out buffer
*/
int binder_cmd_acquire(struct binder_ipc_tinfo *ti, uint32_t target);


/*
* For BC_ACQUIRE, just write buffer into thread out buffer
*/

int binder_cmd_release(struct binder_ipc_tinfo *ti, uint32_t target);


/*
* For BC_REQUEST_DEATH_NOTIFICATION, just write buffer into thread out buffer
*/

int binder_cmd_link_to_death(struct binder_ipc_tinfo *ti, uint32_t target, void * death);


/*
* For BC_FREE_BUFFER
* ptr is the transaction data in struct binder_transaction_data 
* All the transaction buffer should be free after handle done!!!
* just write buffer into thread out buffer
*/
int binder_cmd_freebuf(struct binder_ipc_tinfo *ti, void * ptr);


struct binder_ipc_tinfo * binder_get_thread_info();

/*
* for transaction data, we need to notice that:
* txn data point to "another buffer", so after writed into outbuf, 
we need to talk with driver immediately if the "another buffer" in stack area. 
*/
int ti_write_outbuf(struct binder_ipc_tinfo *ti, void * buf, size_t sz);


/*
* send the buffer in thread info (in_buf and out_buf) to driver
*/
int talk_with_driver(struct binder_ipc_tinfo *ti,int read_flag);

/*
* send the cmds to driver
*/
void flush_commands(struct binder_ipc_tinfo *ti);


/*binder services functions*/
int binder_thread_enter_loop(int isMain, int pending);

void binder_threads_shutdown();

int binder_add_service(const char * name, struct binder_service * b_srv);


/*binder client functions*/
/*return the handle of the service*/
uint32_t binder_get_service(const char * service_name);

size_t binder_list_service(int idx, char* s_name, int len);


#endif
