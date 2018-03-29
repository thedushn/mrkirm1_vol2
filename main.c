#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>








#include <linux/if_ether.h>
#include <features.h>
#include <time.h>
#include "dhcp.h"


#define HAVE_GETOPT_H


int process_arguments(int, char **);
int call_getopt(int, char **);
int validate_arguments(void);
void print_usage(void);
void print_help(void);






int get_results(void);




int main(int argc, char **argv){
	int dhcp_socket;
	int result;

    if(argc<3){
        printf("interface name then options {1,2,3}");
        return -1;
    }
     interface_name(argv[1]);
    /* create socket for DHCP communications */
    dhcp_socket=create_dhcp_socket();

    /* get hardware address of client machine */
    get_hardware_address(dhcp_socket);

    if(strcmp(argv[2],"1")==0){
    /* send DHCPDISCOVER packet */
        send_dhcp_discover(dhcp_socket);
        /* wait for a DHCPOFFER packet */
        get_dhcp_offer(dhcp_socket);
        result=get_results();
        send_dhcp_request(dhcp_socket, received_address);
        get_dhcp_offer(dhcp_socket);
        send_dhcp_release(dhcp_socket);

        /* close socket we created */
        close_dhcp_socket(dhcp_socket);

        /* determine state/plugin output to return */


        /* free allocated memory */
        free_dhcp_offer_list();
        free_requested_server_list();

        return result;

    }

    if(strcmp(argv[2],"2")==0){
        /* send DHCPDISCOVER packet */
        send_dhcp_discover(dhcp_socket);
        /* wait for a DHCPOFFER packet */
        get_dhcp_offer(dhcp_socket);
        result=get_results();
        send_dhcp_request(dhcp_socket, received_address);
        get_dhcp_offer(dhcp_socket);
        send_dhcp_renew(dhcp_socket);
        /* close socket we created */
        close_dhcp_socket(dhcp_socket);

        /* determine state/plugin output to return */


        /* free allocated memory */
        free_dhcp_offer_list();
        free_requested_server_list();

        return result;

    }
    if(strcmp(argv[2],"3")==0){
        /* send DHCPDISCOVER packet */
        send_dhcp_discover(dhcp_socket);
        /* wait for a DHCPOFFER packet */
        get_dhcp_offer(dhcp_socket);
        result=get_results();
        send_dhcp_request(dhcp_socket, received_address);
        get_dhcp_offer(dhcp_socket);
        struct in_addr fake;
        inet_aton("10.2.4.2",&fake);
        send_dhcp_request(dhcp_socket,fake);
        /* close socket we created */
        close_dhcp_socket(dhcp_socket);

        /* determine state/plugin output to return */


        /* free allocated memory */
        free_dhcp_offer_list();
        free_requested_server_list();

        return result;


    }

    close_dhcp_socket(dhcp_socket);

    /* determine state/plugin output to return */


    /* free allocated memory */
    free_dhcp_offer_list();
    free_requested_server_list();
    return 0;

}



