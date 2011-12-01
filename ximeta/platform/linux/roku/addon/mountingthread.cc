/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/
extern "C" {
#include <sys/types.h> //pid_t, mknod
#include <unistd.h> //fork,execl,mknod
#include <stdlib.h> // exit
#include <string.h> // strcpy
#include <sys/stat.h> // mknod
#include <fcntl.h> // mknod
#include <sys/mount.h>
#include <sys/wait.h> // wait
#include <dirent.h> // DIR
#include "ndasuser/ndasuser.h"
}
#include "mountingthread.h"
#include "devicewidget.h"
#include "defs.h"



class taskview_list {
private:    
    class taskview_node {
    public:
        pid_t pid;
        taskview_node* next;
    };
    int nr_taskview;
    taskview_node* m_first_node;
public:    
    taskview_list() : m_first_node(0), nr_taskview(0) {}
    ~taskview_list() 
    {
        while(m_first_node) {
            taskview_node* next = m_first_node->next;
            delete m_first_node;
            m_first_node = next;
        }
    }
    void add(pid_t pid) {
        taskview_node* node = new taskview_node();
        node->pid = pid;
        node->next = m_first_node;
        m_first_node = node;
    }
    void killall() {
        taskview_node* node = m_first_node;
        //debug("killing m_first_node=%p\n",m_first_node);
        while(node) {
            kill(node->pid,SIGTERM);
            node = node->next;
        }
    }

};


int reset_taskview()
{
    static DIR *dir;
    struct dirent *entry;
    //static procps_status_t ret_status;
    char *name;
    int n;
    char status[32];
    char buf[1024];
    char short_cmd[16];
    FILE *fp;
    //procps_status_t curstatus;
    int pid;
    long tasknice;
    struct stat sb;
    int errors = 0;
    
    if (!dir) {
        dir = opendir("/proc");
        if(!dir) {
            debug("Can't open /proc");
            return -1;
        }
    }
    taskview_list list;
    for(;;) {
        if((entry = readdir(dir)) == NULL) {
            closedir(dir);
            dir = 0;
            list.killall();
            return errors;
        }
        name = entry->d_name;
        if (!(*name >= '0' && *name <= '9'))
            continue;

//        memset(&curstatus, 0, sizeof(procps_status_t));
        memset(short_cmd,0,sizeof(short_cmd));
        pid = atoi(name);
//        curstatus.pid = pid;
//        debug("pid=%d",pid);

        sprintf(status, "/proc/%d/stat", pid);
        if((fp = fopen(status, "r")) == NULL)
            continue;

        if(fstat(fileno(fp), &sb))
            continue;
//        my_getpwuid(curstatus.user, sb.st_uid);
        name = fgets(buf, sizeof(buf), fp);
        fclose(fp);
        if(name == NULL)
            continue;
        name = strrchr(buf, ')'); /* split into "PID (cmd" and "<rest>" */
        if(name == 0 || name[1] != ' ')
            continue;
        *name = 0;
        sscanf(buf, "%*s (%15c", short_cmd);
//        debug("short_cmd=%s",short_cmd);
        if ( strcmp(short_cmd,"taskview") == 0 ) {
            list.add(pid);
        }
    }
    
}



/* Create device file, before enabling the disk*/

bool MountingThread::Unmount() 
{
    debug("ing");
    char buffer[30];
    int errors = 0;
    for (int i = 1 ; i < 5; i++ ) {
        struct stat st;
        sprintf(buffer, "/mnt/smb/ndas%d",i);
        if ( stat(buffer,&st) == -1 ) 
            continue;
        if ( st.st_size == 0 ) 
            continue;
        debug("umount2 %s MNT_FORCE",buffer);
        if ( umount2(buffer, MNT_FORCE) ) // since 2.1.116
            errors++;
    }
    return errors == 0;
}
bool MountingThread::UnregisterAll() 
{
    return NDAS::unregister_all();
}
void MountingThread::CreateNode(int slot) {
    debug("ing");
    char buffer[10];
    int i = 0;
    sprintf(buffer,"/dev/nd%c", 'a' + slot -NDAS_FIRST_SLOT_NR);
    mknod(buffer, S_IFBLK|0644, makedev(60, (slot-NDAS_FIRST_SLOT_NR)*8));
    
    
    for (i = 1 ; i < 5; i++ ) {
        sprintf(buffer,"/dev/nd%c%d", 'a' + slot -NDAS_FIRST_SLOT_NR, i);
        mknod(buffer, S_IFBLK|0644, makedev(60, (slot-NDAS_FIRST_SLOT_NR)*8 + i));
        sprintf(buffer, "/mnt/smb/ndas%d",i);
        mkdir(buffer, 0644);
    }
}


void MountingThread::run() {
	debug("unmounting");
    if ( !Unmount() ) {
        debug("Fail to Unmount the partition(s) on the NetDisk");
        m_win->CancelMounting();
        m_win->Redraw();
        // delete this;
        // display message of "Fail to Unmount the partition(s) on the NetDisk"
        return;
    }
	debug("UnregisterAll-ing");
    if ( !UnregisterAll() ) {
        debug("Fail to Unmount the partition(s) on the NetDisk");
        m_win->CancelMounting();
        m_win->Redraw();
        // delete this;
        // display message of "Fail to disable the NetDisk"
        return;
    }
	debug("enable-ing");
    if ( !Enable((*m_win->m_serial_list)[m_index]) ) {
        debug("EnableNetDisk-failed");
        m_win->CancelMounting();
        m_win->Redraw();
        // delete this;
        // display message of "Fail to disable the NetDisk"
        return;
    }
    reset_taskview();
    CascadeApp::GetApp()->Terminate(0);
}

bool MountingThread::Enable(string serial) 
{
    char buffer[10];
    char mount_dir[30];
    ndas_error_t err;
    debug("ing");
    
    
    debug("Register-ing serial %s", serial.c_str());

    err = NDAS::register_device(serial, serial);
    
    if ( !NDAS_SUCCESS(err) && err != NDAS_ERROR_ALREADY_REGISTERED_DEVICE) 
	{
        return false;
    }

	string slot; 
	sleep(1);
	int i = 0;
	for ( i = 0 ; i < 3 ; i++ ) {
		slot = NDAS::get_first_slot(serial);
		debug("slot %s", slot.c_str());
		sleep(1);
		if ( slot.size() > 0 ) break;
	}
	if ( slot.size() == 0 ) {
		debug("no slot for %s", serial.c_str());
		return false;
	}
    debug("enable-ing slot=%s %d", slot.c_str(), atoi(slot.c_str()));
	sleep(1);
	err = NDAS::enable(atoi(slot.c_str()), 0);
	debug("enable-ing err=%d", err);
    if ( !NDAS_SUCCESS(err) ) {
		if ( err != NDAS_ERROR_TRY_AGAIN )
	        NDAS::unregister_device((*m_win->m_serial_list)[m_index]);
        return false;
    }
    debug("Enable-ed");
    CreateNode(atoi(slot.c_str()));
    for (int i = 1; i < 5; i++) 
    { 
        struct stat st;
        sprintf(buffer,"/dev/nd%c%d", 'a' + atoi(slot.c_str()) - NDAS_FIRST_SLOT_NR, i);
        if ( stat(buffer, &st) == -1 )  {
            continue;
        }
        sprintf(mount_dir, "/mnt/smb/ndas%d",i);
        //if ( stat(mount_dir, &st) == -1 ) 
            //continue;
        debug("%s size=%ld",buffer, st.st_size);
        debug("mount %s %s -t %s", buffer,mount_dir,"vfat");
        if ( mount(buffer, mount_dir, "vfat", MS_NOSUID | MS_NODEV |MS_RDONLY|0xc0ed0000, 0) == 0 ) {
            continue;
        }
        debug("mount %s %s -t %s", buffer,mount_dir,"msdos");
        if ( mount(buffer, mount_dir, "msdos", MS_NOSUID | MS_NODEV |MS_RDONLY|0xc0ed0000, 0) == 0 ) {
            continue;
        }
        debug("fail to mount", buffer,mount_dir,"vfat");
    }
    return true;
}
