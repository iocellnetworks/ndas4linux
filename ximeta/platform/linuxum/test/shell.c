#include <stdio.h>
#include <unistd.h> /* sleep */
#include <readline/readline.h>
#include <readline/history.h>
#include <ndasuser/ndasuser.h>

extern int self_test_main(int argv, char** argc);
static int ndc_start(char* argv)
{
    return ndas_start();
}
static int ndc_stop(char* argv)
{
    return ndas_stop();
}
static int ndc_restart(char* argv)
{
    return ndas_restart();
}
static int ndc_register(char *argv);
static int ndc_unregister(char *line)
{
	char *id, *key, *name;	
	name = strsep(&line, " \t\n\r");
    if ( name == NULL ) {
        goto usage;
    }
	
	return ndas_unregister_device(name);	
usage:
    printf("usage: unregister <name>\n");
    return -1;
}
static int ndc_enable(char *argv);
static int ndc_disable(char *argv);
static int ndc_edit(char *argv){}
static int ndc_request(char *argv);
static int ndc_probe(char *argv){}
static int ndc_help(char* argv){}
static int ndc_sleep(char *line)
{
    int secs;
	char *ptr;
	ptr = strsep(&line, " \t\n\r");
	secs = atoi(ptr);
	sleep(secs);    
}

#define NDAS_CMD_COUNT (sizeof(ndas_cmd)/sizeof(ndas_cmd[0]))

struct _ndas_cmd {
    char* str;
    int (*func)(char *);
    char* help;
} ndas_cmd[] = {
    {"start", ndc_start, 
        "\tStart NDAS driver. No operation is possible before start command\n"},
    {"restart", ndc_restart, 
        "\tRestart NDAS driver.\n"},
    {"stop", ndc_stop, "\tStop NDAS driver.\n"},
    {"register", ndc_register,  
        "\tRegister a new netdisk with ID and name.\n"
        "\tIf registration is successful, a slot number is returned.\n"
        "\tThe slot number  is used for enabling and disabling the device.\n"
        "\tUsage: register NDASName ID write-key\n"
        "\t       ex) register NAME XXXXXXXXXXXXXXXXXXXX WWWWW\n"},
    {"unregister", ndc_unregister, 
        "\tRemove a registered netdisk with name\n"
        "\tUsage: ndasadmin unregister -n|--name NDASName\n" },
    {"edit", ndc_edit,"\tUsage: ndasadmin edit -o NDASDeviceName -n NewNDASDeviceName\n" },
    {"enable", ndc_enable,
        "\tEnable netdisk Remove a registered netdisk with slot number.\n"
        "\tIf connected in shared write mode, multiple hosts can enable the same device in write mode.\n"
        "\tBut coordination between hosts are needed to prevent file system corruption.\n"
        "\tUsage: ndasadmin enable -s SlotNumber -o accessMode\n"
        "\t       - accessMode : \'r\'(read), \'w\'(read/write) or \'s\'(shared write).\n"
        },
    {"disable", ndc_disable,
        "\tDisable enabled device by slot number\n"
        "\tUsage: ndasadmin disable -s SlotNumber\n"},
#ifdef NDAS_MULTIDISKS    
    {"config", ndc_config,
        "\tUsage: ndasadmin config -s SlotNumber -t a -ps PeerSlotNumber\n"
        "\t       ndasadmin config -s SlotNumber -t m -ps PeerSlotNumber\n"
        "\t       ndasadmin config -s SlotNumber -t s \n"
        "\t       type 'a': aggregation, 'm': mirroring, 's': separation\n" },
#endif    
    {"request", ndc_request,
        "\tRequest other host in the network to give up write access of the device.\n"
        "\tUsage: ndasadmin request -s SlotNumber\n" },
    {"probe", ndc_probe,
        "\tShow the list of netdisks on the network.\n"
        "\tUsage: ndasadmin probe\n" },
    {"sleep", ndc_sleep,
        "\tSleep\n"
        "\tUsage: sleep [sec]\n" }
/*    {"help", ndc_help, NULL}*/
};

void perform() 
{
	char *cmd;
    int i;
	while(1) {
		char *line = readline("ndas> ");
		cmd = strsep(&line, " \t\n\r");
		if ( strncmp("exit", cmd, strlen("exit") + 1) == 0 ||
		    strncmp("quit", cmd, strlen("quit") + 1) == 0 ) 
        {
			free(cmd);
			break;
		}
        for(i=0;i<NDAS_CMD_COUNT;i++) {
            if(!strncmp(cmd,ndas_cmd[i].str, strlen(ndas_cmd[i].str) + 1)) {
                ndas_cmd[i].func(line);
            }
        }
		free(cmd);
	}	
}
int ndc_register(char *line)
{
	char *id, *key, *name;	
	name = strsep(&line, " \t\n\r");
    if ( name == NULL ) {
        goto usage;
    }
	id = strsep(&line, " \t\n\r");
    if ( id == NULL ) {
        goto usage;
    }
	key = strsep(&line, " \t\n\r");
	return ndas_register_device(name, id, key);	
usage:
    printf("usage: register <name> <ndas id> [ndas key]\n");
    return -1;
}
NDAS_CALL ndas_error_t perm(int slot) {
    printf("Somebody want to get the write permission of slot %d\n", slot);
}
int ndc_enable(char *line)
{
	int slot, mode;
	char *ptr;
	ptr = strsep(&line, " \t\n\r");
    if ( ptr == NULL ) {
        printf("usage: enable <slot> [w|s|r]\n");
        return;
    }
	slot = atoi(ptr);
	ptr = strsep(&line, " \t\r\n");
    if ( ptr ) {
    	mode = strncmp("w",ptr, 2) == 0;
    }
    mode ? ndas_enable_exclusive_writable(slot)
        : ndas_enable_slot(slot);
}
int ndc_request(char *line)
{
	int slot;
	char *ptr;
	ptr = strsep(&line, " \t\n\r");
	slot = atoi(ptr);
	ndas_request_permission(slot);
}
int ndc_disable(char *line)
{
	int slot;
	char *ptr;
	ptr = strsep(&line, " \t\n\r");
	slot = atoi(ptr);
	ndas_disable_slot(slot);
}
int main(int argc, char** argv)
{
	return self_test_main(argc, argv);
}
