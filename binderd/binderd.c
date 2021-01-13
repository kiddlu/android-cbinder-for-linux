#include "binder.h"
#include "binder_ipc.h"
#include "binder_hal.h"
#include "binder_common.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct binder_death {
    void (*death_cb)(struct binder_ipc_tinfo *, void *);
    void *ptr;
};

struct svcinfo
{
    struct svcinfo *next;
    uint32_t handle;
    struct binder_death death;
    size_t len;
    char name[0];
};

struct svcinfo *svclist = NULL;

void svc_death(struct binder_ipc_tinfo *ti, void *ptr)
{
    struct svcinfo *si = (struct svcinfo* ) ptr;

    if (si->handle) {
        printf("----------svc_death handle:0x%08x-------------\n",si->handle);
        binder_cmd_release(ti, si->handle);
        flush_commands(ti);
        si->handle = 0;
    }
}


struct svcinfo *find_svc(const char *name)
{
    struct svcinfo *si;

    for (si = svclist; si; si = si->next) {
        if (strncmp(si->name, name, si->len) == 0) {
            return si;
        }
    }
    return NULL;
}

struct svcinfo * get_svc_idx(int index){
    struct svcinfo *si;
    int i = 0;

    for (si = svclist; si; si = si->next) {
        if(i++ == index) return si;
    }
    return NULL;

}

struct svcinfo * create_svc(const char *name, uint32_t hdl){
    int len = 0;
    len = strlen(name);
    
    struct svcinfo * new_svc = (struct svcinfo*)malloc(sizeof(struct svcinfo) + len + 1);
    if(new_svc){
        new_svc->handle = hdl;
        new_svc->len = len+1;
        new_svc->death.death_cb = (void*) svc_death;
        new_svc->death.ptr = new_svc;
        snprintf(new_svc->name, len+1, "%s", name);
        new_svc->next = svclist;
        svclist = new_svc;
    }
    return new_svc;
}


int svcmgr_handler(struct binder_ipc_tinfo *ti,
                   struct binder_transaction_data *txn,
                   struct binder_io * reply){
    struct binder_io msg;
    char *name, *svc;
    uint32_t handle, strict_policy;
    struct svcinfo * si = NULL;
    binder_io_init_from_txn(&msg, txn);
    if(txn->target.ptr != DEFAULT_SRV_MANAGER_HANDLE) return -1;

    strict_policy = binder_io_get_uint32(&msg);

    if(strict_policy != 0) return -1;
    
    name = binder_io_get_string(&msg, NULL);

    if(!name || strncmp(name,DEFAULT_SRV_MANAGER_NAME,strlen(DEFAULT_SRV_MANAGER_NAME))){
        printf("Error: invalid service name !!!\n");
        return -1;
    }
    
    switch(txn->code){
        case SVC_MGR_GET_SERVICE:
        case SVC_MGR_CHECK_SERVICE:
            svc = binder_io_get_string(&msg, NULL);
            if(!svc) return -1;
            si = find_svc(svc);
            if(!si) return -1;
            binder_io_append_ref(reply, si->handle);
            break;
        case SVC_MGR_ADD_SERVICE:
            svc = binder_io_get_string(&msg, NULL);
            if(!svc) return -1;
            printf("get svc name:%s\n",svc);
            handle = binder_io_get_ref(&msg,0);
            if(!!(si = find_svc(svc))){
                printf("Warning: service exist! override it...\n");
                svc_death(ti, (void*)si);
                si->handle = handle;
            }else{
                si = create_svc(svc, handle);
            }
            binder_cmd_acquire(ti, handle);
            binder_cmd_link_to_death(ti,handle,(void*)&(si->death));
            flush_commands(ti);
            binder_io_append_uint32(reply,0);
            break;
        case SVC_MGR_LIST_SERVICES:
            {
                uint32_t n = binder_io_get_uint32(&msg);
                si = get_svc_idx(n);
                if(!si) return -1;
                binder_io_append_string(reply, si->name);
            }
            break;
        default:
            printf("Error: unknown code for service manager\n");
            return -1;
    }

    return 0;
}

void svcmgr_send_reply(struct binder_ipc_tinfo *ti, struct binder_io * reply, int res){
    struct binder_transaction_data tr;
    uint32_t cmd = BC_REPLY;
    memset(&tr, 0, sizeof(tr));
    if(res){
        tr.flags = TF_STATUS_CODE;
        tr.data_size = sizeof(int);
        tr.offsets_size = 0;
        tr.data.ptr.buffer = (binder_uintptr_t)&res;
    }else{
        tr.data_size = reply->data - reply->data0;
        tr.data.ptr.buffer = (binder_uintptr_t)reply->data0;
        tr.offsets_size = (char*)reply->offs - (char*)reply->offs0;
        tr.data.ptr.offsets= (binder_uintptr_t)reply->offs0;
    }
    ti_write_outbuf(ti, &cmd, sizeof(cmd));
    ti_write_outbuf(ti, &tr, sizeof(tr));
    //flush_commands(ti);
}

#define BINDERFS_MAX_NAME 255

/**
 * struct binderfs_device - retrieve information about a new binder device
 * @name:   the name to use for the new binderfs binder device
 * @major:  major number allocated for binderfs binder devices
 * @minor:  minor number allocated for the new binderfs binder device
 *
 */
struct binderfs_device {
	char name[BINDERFS_MAX_NAME + 1];
	__u32 major;
	__u32 minor;
};

/**
 * Allocate a new binder device.
 */
#define BINDER_CTL_ADD _IOWR('b', 1, struct binderfs_device)

void init_binderfs(void)
{
    int fd, ret, saved_errno;
    struct binderfs_device device = {0};

	if (access(DEFAULT_BINDER_DEV, F_OK) == 0) {
		//do not need init binderfs
		return;
	}

	ret = mkdir("/dev/binderfs", 0755);
	if (ret < 0 && errno != EEXIST) {
		fprintf(stderr, "%s - Failed to create binderfs mountpoint\n",
			strerror(errno));
	}

	ret = mount(NULL, "/dev/binderfs", "binder", 0, 0);
	if (ret < 0) {
		fprintf(stderr, "%s - Failed to mount binderfs\n",
			strerror(errno));
	}

    memcpy(device.name, "cbinder", strlen("cbinder"));

    fd = open("/dev/binderfs/binder-control", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        printf("%s - Failed to open binder-control device\n",
               strerror(errno));
    }

    ret = ioctl(fd, BINDER_CTL_ADD, &device);
    saved_errno = errno;
    close(fd);
    errno = saved_errno;
    if (ret < 0)
    {
        //printf("%s\n",
        //      explain_errno_ioctl(errno, fd, BINDER_CTL_ADD, &device));
        printf("%s - Failed to allocate new binder device\n",
               strerror(errno));
    }

    printf("Allocated new binder device with major %d, minor %d, and "
           "name %s\n",
           device.major, device.minor,
           device.name);

    char device_name[128] = "/dev/binderfs/cbinder";

    struct stat sb;
    mode_t mode;
    if (stat(device_name, &sb) == -1) {
        printf("stat %s failed\n", device_name);
    }
    // o+rw
    mode = sb.st_mode | S_IWOTH | S_IROTH;
    if (chmod(device_name, mode) == -1) {
        printf("chmod o+rw %s failed\n", device_name);
    }
}

int main(int argc, char * argv[]){

	init_binderfs();

    struct binder_ipc_tinfo * ti = binder_get_thread_info();
    struct binder_thread_data t_data;
    struct binder_buf rbuf;
    struct binder_io reply;
    uint32_t cmd = 0;
    int ret = 0, result = 0;
    char reply_data[128] = {0};
    struct binder_transaction_data *tr = NULL;

    memset(&t_data, 0 ,sizeof(t_data));
    t_data.isMain = 1;
    snprintf(t_data.t_name, sizeof(t_data.t_name),DEFAULT_SRV_MANAGER_NAME);
    if(ti){
        if(binder_request_context_manager(ti->bs) < 0) return -1;

        /*set to zero to avoid received event BR_SPAWN_LOOPER from driver*/
        binder_set_max_threads(ti->bs, 0);
        /*
        talk with driver and parse cmds
        */
        cmd = BC_ENTER_LOOPER;
        ti_write_outbuf(ti, &cmd, sizeof(cmd));
        while(ret >= 0){
            if(0 > talk_with_driver(ti, 1)){
                printf("Error:fail to talk with driver, exit!\n");
                break;
            }

            binder_buf_init(&rbuf, ti->in_buf.ptr, ti->in_buf.consumed, 0);
            cmd = binder_buf_get_next_cmd(&rbuf);
            if(rbuf.consumed > 0){
                /*move the in buf first*/
                binder_buf_move_buffer(&(ti->in_buf), &rbuf);
            }
            if(cmd == 0){
                continue;
            }else{
                const void * data = (void *)(rbuf.ptr + sizeof(cmd));
                switch (cmd){
                    case BR_NOOP:
                        break;
                    case BR_TRANSACTION_COMPLETE:
                        break;
                    case BR_INCREFS:
                    case BR_ACQUIRE:
                    case BR_RELEASE:
                    case BR_DECREFS:
                        break;
                    case BR_TRANSACTION:
                        tr = (struct binder_transaction_data *) data;
                        binder_io_init(&reply, reply_data, sizeof(reply_data), DEFAULT_OFFSET_LIST_SIZE);
                        result = svcmgr_handler(ti, tr, &reply);
                        if(!(tr->flags & TF_ONE_WAY)){
                            svcmgr_send_reply(ti,&reply,result);
                        }
                        binder_cmd_freebuf(ti, (void *)tr->data.ptr.buffer);
                        break;
                    case BR_REPLY:
                        tr = (struct binder_transaction_data *) data;
                        binder_cmd_freebuf(ti, (void *)tr->data.ptr.buffer);
                        break;
                    case BR_DEAD_BINDER:
                        {
                            //uint32_t dead_cmd = BC_DEAD_BINDER_DONE;
                            binder_uintptr_t __cookie = *(binder_uintptr_t *)data;
                            struct binder_death *death = (struct binder_death *) __cookie;
                            if(death->death_cb) death->death_cb(ti, death->ptr);
                            
                            //ti_write_outbuf(ti, &dead_cmd, sizeof(dead_cmd));
                            //ti_write_outbuf(ti, (binder_uintptr_t *)&__cookie, sizeof(binder_uintptr_t));
                        }
                        break;
                    case BR_FAILED_REPLY:
                    case BR_DEAD_REPLY:
                        ret = -1;
                        break;
                    default:
                        printf("Error: OOPS in service manager, unknown command : %08x\n",cmd);
                        ret = -1;
                        break;
                }
            }
        }

        binder_threads_shutdown();
    }

    return 0;
}

