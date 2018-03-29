//
// Created by root on 28.3.18..
//

#include <sys/time.h>
#include <time.h>
#include <linux/if_ether.h>
#include "dhcp.h"
int verbose=0;
char network_interface_name[30];
u_int32_t packet_xid=0;
int request_specific_address=FALSE;//TRUE
u_int32_t dhcp_lease_time=0;
u_int32_t dhcp_renewal_time=0;
u_int32_t dhcp_rebinding_time=0;

struct in_addr requested_address;

u_int32_t   offer_xid;

int dhcpoffer_timeout=2;
unsigned char client_hardware_address[MAX_DHCP_CHADDR_LENGTH]="";
unsigned int my_client_mac[MAX_DHCP_CHADDR_LENGTH];
int mymac = 0;



int valid_responses=0;     /* number of valid DHCPOFFERs we received */
int requested_servers=0;
int requested_responses=0;


int received_requested_address=FALSE;

dhcp_offer *dhcp_offer_list=NULL;
requested_server *requested_server_list=NULL;
void interface_name(char *argv){

    strncpy(network_interface_name,argv,sizeof(argv));
};
/* sends a DHCP packet */
int send_dhcp_packet(void *buffer, int buffer_size, int sock, struct sockaddr_in *dest){
    struct sockaddr_in myname;
    int result;

    result=(int)sendto(sock,(char *)buffer,(size_t)buffer_size,0,(struct sockaddr *)dest,sizeof(*dest));


    printf("send_dhcp_packet result: %d\n",result);

    if(result<0)
        return ERROR;

    return OK;
}

/* adds a requested server address to list in memory */
int add_requested_server(struct in_addr server_address){
    requested_server *new_server;

    new_server=(requested_server *)malloc(sizeof(requested_server));
    if(new_server==NULL)
        return ERROR;

    new_server->server_address=server_address;

    new_server->next=requested_server_list;
    requested_server_list=new_server;

    requested_servers++;


    printf("Requested server address: %s\n",inet_ntoa(new_server->server_address));

    return OK;
}
/* creates a socket for DHCP communication */
int create_dhcp_socket(void){
    struct sockaddr_in myname;
    struct ifreq interface;
    int sock;
    int flag=1;

    /* Set up the address we're going to bind to. */
    bzero(&myname,sizeof(myname));
    myname.sin_family=AF_INET;// AF_INET (IPv4 protocol)
    myname.sin_port=htons(DHCP_CLIENT_PORT);
    myname.sin_addr.s_addr=INADDR_ANY;                 /* listen on any address */
    bzero(&myname.sin_zero,sizeof(myname.sin_zero));

    /* create a socket for DHCP communications */
    sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(sock<0){
        printf("Error: Could not create socket!\n");
        exit(1);
    }


    printf("DHCP socket: %d\n",sock);

    /* set the reuse address flag so we don't get errors when restarting */
    flag=1;
    if(setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,(char *)&flag,sizeof(flag))<0){
        printf("Error: Could not set reuse address option on DHCP socket!\n");
        exit(1);
    }

    /* set the broadcast option - we need this to listen to DHCP broadcast messages */
    if(setsockopt(sock,SOL_SOCKET,SO_BROADCAST,(char *)&flag,sizeof flag)<0){
        printf("Error: Could not set broadcast option on DHCP socket!\n");
        exit(1);
    }

    /* bind socket to interface */

    strncpy(interface.ifr_ifrn.ifrn_name,network_interface_name,IFNAMSIZ);
    if(setsockopt(sock,SOL_SOCKET,SO_BINDTODEVICE,(char *)&interface,sizeof(interface))<0){
        printf("Error: Could not bind socket to interface %s.  Check your privileges...\n",network_interface_name);
        exit(1);
    }


    // bind the socket
    if(bind(sock,(struct sockaddr *)&myname,sizeof(myname))<0){
        printf("Error: Could not bind to DHCP socket (port %d)!  Check your privileges...\n",DHCP_CLIENT_PORT);
        exit(1);
    }

    return sock;
}
/* closes DHCP socket */
int close_dhcp_socket(int sock){

    close(sock);

    return OK;
}

int get_hardware_address(int sock){

    int i;


    struct ifreq ifr;

    strncpy((char *)&ifr.ifr_name,network_interface_name,sizeof(ifr.ifr_name));

    // Added code to try to set local MAC address just to be through
    // If this fails the test will still work since
    // we do encode the MAC as part of the DHCP frame - tests show it works
    if(mymac)
    {

        for(i=0;i<MAX_DHCP_CHADDR_LENGTH;++i)
            client_hardware_address[i] = my_client_mac[i];
        memcpy(&ifr.ifr_hwaddr.sa_data,&client_hardware_address[0],6);
        if(ioctl(sock,SIOCSIFHWADDR,&ifr)<0){
            printf("Error: Could not set hardware address of interface '%s'\n",network_interface_name);
            // perror("Error");
            // exit(STATE_UNKNOWN);
        }


    }
    else
    {
        /* try and grab hardware address of requested interface */
        if(ioctl(sock,SIOCGIFHWADDR,&ifr)<0){
            printf("Error: Could not get hardware address of interface '%s'\n",network_interface_name);
            exit(1);
        }
        memcpy(&client_hardware_address[0],&ifr.ifr_hwaddr.sa_data,6);
    }




        printf("Hardware address: ");
        for (i=0; i<6; ++i)
            printf("%2.2x", client_hardware_address[i]);
        printf( "\n");


    return OK;
}

int
fill_dhcp_option(char *packet, u_int8_t code, u_int8_t *data, u_int8_t len)
{
    packet[0] = code;
    packet[1] = len;
    memcpy(&packet[2], data, len);

    return len + (sizeof(u_int8_t) * 2);
}
int send_dhcp_discover(int sock){
    dhcp_packet discover_packet;
    struct sockaddr_in sockaddr_broadcast;

    struct in_addr address;

    /* clear the packet data structure */
    memset(&discover_packet,0,sizeof(discover_packet));

    u_int8_t parameter_req_list[] = {MESSAGE_TYPE_REQ_SUBNET_MASK, MESSAGE_TYPE_ROUTER, MESSAGE_TYPE_DNS,MESSAGE_TYPE_DOMAIN_NAME ,DHCP_OPTION_RENEWAL_TIME,DHCP_OPTION_REBINDING_TIME,DHCP_OPTION_REQUESTED_ADDRESS};

    /* boot request flag (backward compatible with BOOTP servers) */
    discover_packet.op=BOOTREQUEST;

    /* hardware address type */
    discover_packet.htype=ETHERNET_HARDWARE_ADDRESS;

    /* length of our hardware address */
    discover_packet.hlen=ETHERNET_HARDWARE_ADDRESS_LENGTH;

    discover_packet.hops=0;

    /* transaction id is supposed to be random */
    srand(time(NULL));
    packet_xid=random();
    discover_packet.xid=htonl(packet_xid);

    /**** WHAT THE HECK IS UP WITH THIS?!?  IF I DON'T MAKE THIS CALL, ONLY ONE SERVER RESPONSE IS PROCESSED!!!! ****/
    /* downright bizzarre... */
    ntohl(discover_packet.xid);

    /*discover_packet.secs=htons(65535);*/
    discover_packet.secs=0xFF;

    /* tell server it should broadcast its response */
    discover_packet.flags=htons(DHCP_BROADCAST_FLAG);

    /* our hardware address */
    memcpy(discover_packet.chaddr,client_hardware_address,ETHERNET_HARDWARE_ADDRESS_LENGTH);

    /* first four bytes of options field is magic cookie (as per RFC 2132) */
    discover_packet.options[0]='\x63';
    discover_packet.options[1]='\x82';
    discover_packet.options[2]='\x53';
    discover_packet.options[3]='\x63';
    int len=4;
    u_int8_t option=DHCPDISCOVER;
    len+=fill_dhcp_option(&discover_packet.options[len],DHCP_OPTION_MESSAGE_TYPE,&option,sizeof(option));


  discover_packet.options[len]= 50;
    len+=(sizeof(u_int8_t));
    discover_packet.options[len]= '\x04';
    len+=(sizeof(u_int8_t));
    memcpy(&discover_packet.options[len],&address,sizeof(address));
    len+=(sizeof(address));



    len += fill_dhcp_option(&discover_packet.options[len], MESSAGE_TYPE_PARAMETER_REQ_LIST, (u_int8_t *)&parameter_req_list, sizeof(parameter_req_list));
    option = 0;
    len += fill_dhcp_option(&discover_packet.options[len], DHCP_OPTION_END_MESSAGE, &option, sizeof(option));








    /* send the DHCPDISCOVER packet to broadcast address */
    sockaddr_broadcast.sin_family=AF_INET;
    sockaddr_broadcast.sin_port=htons(DHCP_SERVER_PORT);
    sockaddr_broadcast.sin_addr.s_addr=INADDR_BROADCAST;
    memset(&sockaddr_broadcast.sin_zero,0,sizeof(sockaddr_broadcast.sin_zero));



        printf("DHCPDISCOVER to %s port %d\n",inet_ntoa(sockaddr_broadcast.sin_addr),ntohs(sockaddr_broadcast.sin_port));
        printf("DHCPDISCOVER XID: %lu (0x%X)\n",(unsigned long) ntohl(discover_packet.xid),ntohl(discover_packet.xid));
        printf("DHCDISCOVER ciaddr:  %s\n",inet_ntoa(discover_packet.ciaddr));
        printf("DHCDISCOVER yiaddr:  %s\n",inet_ntoa(discover_packet.yiaddr));
        printf("DHCDISCOVER siaddr:  %s\n",inet_ntoa(discover_packet.siaddr));
        printf("DHCDISCOVER giaddr:  %s\n",inet_ntoa(discover_packet.giaddr));


    /* send the DHCPDISCOVER packet out */
    send_dhcp_packet(&discover_packet,sizeof(discover_packet),sock,&sockaddr_broadcast);


        printf("\n\n");

    return OK;
}
/* receives a DHCP packet */
int receive_dhcp_packet(void *buffer, int buffer_size, int sock, int timeout, struct sockaddr_in *address){
    struct timeval tv;
    fd_set readfds;
    int recv_result;
    socklen_t address_size;
    struct sockaddr_in source_address;


    /* wait for data to arrive (up time timeout) */
    tv.tv_sec=timeout;
    tv.tv_usec=0;
    FD_ZERO(&readfds);
    FD_SET(sock,&readfds);
    select(sock+1,&readfds,NULL,NULL,&tv);

    /* make sure some data has arrived */
    if(!FD_ISSET(sock,&readfds)){

            printf("No (more) data received\n");
        return ERROR;
    }

    else{

        /* why do we need to peek first?  i don't know, its a hack.  without it, the source address of the first packet received was
           not being interpreted correctly.  sigh... */
        bzero(&source_address,sizeof(source_address));
        address_size=sizeof(source_address);
        recv_result=recvfrom(sock,(char *)buffer,buffer_size,MSG_PEEK,(struct sockaddr *)&source_address,&address_size);

            printf("recv_result_1: %d\n",recv_result);
        recv_result=recvfrom(sock,(char *)buffer,buffer_size,0,(struct sockaddr *)&source_address,&address_size);
            printf("recv_result_2: %d\n",recv_result);

        if(recv_result==-1){

                printf("recvfrom() failed, ");
                printf("errno: (%d) -> %s\n",errno,strerror(errno));

            return ERROR;
        }
        else{

                printf("receive_dhcp_packet() result: %d\n",recv_result);
                printf("receive_dhcp_packet() source: %s\n",inet_ntoa(source_address.sin_addr));


            memcpy(address,&source_address,sizeof(source_address));
            return OK;
        }
    }

    return OK;
}

/* gets state and plugin output to return */
int get_results(void){
    printf("result");
    dhcp_offer *temp_offer;
    requested_server *temp_server;
    int result;
    u_int32_t max_lease_time=0;

    received_requested_address=FALSE;

    /* checks responses from requested servers */
    requested_responses=0;
    if(requested_servers>0){

        for(temp_server=requested_server_list;temp_server!=NULL;temp_server=temp_server->next){

            for(temp_offer=dhcp_offer_list;temp_offer!=NULL;temp_offer=temp_offer->next){

                /* get max lease time we were offered */
                if(temp_offer->lease_time>max_lease_time || temp_offer->lease_time==DHCP_INFINITE_TIME)
                    max_lease_time=temp_offer->lease_time;

                /* see if we got the address we requested */
                if(!memcmp(&requested_address,&temp_offer->offered_address,sizeof(requested_address)))
                    received_requested_address=TRUE;

                /* see if the servers we wanted a response from talked to us or not */
                if(!memcmp(&temp_offer->server_address,&temp_server->server_address,sizeof(temp_server->server_address))){
                    if (verbose) {
                        printf("DHCP Server Match: Offerer=%s",inet_ntoa(temp_offer->server_address));
                        printf(" Requested=%s\n",inet_ntoa(temp_server->server_address));
                    }
                    requested_responses++;
                }
            }
        }

    }

        /* else check and see if we got our requested address from any server */
    else{

        for(temp_offer=dhcp_offer_list;temp_offer!=NULL;temp_offer=temp_offer->next){

            /* get max lease time we were offered */
            if(temp_offer->lease_time>max_lease_time || temp_offer->lease_time==DHCP_INFINITE_TIME)
                max_lease_time=temp_offer->lease_time;

            /* see if we got the address we requested */
            if(!memcmp(&requested_address,&temp_offer->offered_address,sizeof(requested_address)))
                received_requested_address=TRUE;
        }
    }

    result=STATE_OK;
    if(valid_responses==0)
        result=STATE_CRITICAL;
    else if(requested_servers>0 && requested_responses==0)
        result=STATE_CRITICAL;
    else if(requested_responses<requested_servers)
        result=STATE_WARNING;



    printf("DHCP %s: ",(result==STATE_OK)?"ok":"problem");

    /* we didn't receive any DHCPOFFERs */
    if(dhcp_offer_list==NULL){
        printf("No DHCPOFFERs were received.\n");
        return result;
    }

    printf("Received %d DHCPOFFER(s)",valid_responses);

    if(requested_servers>0)
        printf(", %s%d of %d requested servers responded",
               ((requested_responses<requested_servers) && requested_responses>0)?"only ":"",requested_responses,requested_servers);

    if(request_specific_address==TRUE)
        printf(", requested address (%s) was %soffered",inet_ntoa(requested_address),(received_requested_address==TRUE)?"":"not ");

    printf(", max lease time = ");
    if(max_lease_time==DHCP_INFINITE_TIME)
        printf("Infinity");
    else
        printf("%lu sec",(unsigned long)max_lease_time);

    printf(".\n");

    return result;
}
/* waits for a DHCPOFFER message from one or more DHCP servers */
int get_dhcp_offer(int sock){
    dhcp_packet offer_packet;
    struct sockaddr_in source;
    int result=OK;
    int timeout=1;
    int responses=0;
    int x;
    time_t start_time;
    time_t current_time;

    time(&start_time);

    /* receive as many responses as we can */
    for(responses=0,valid_responses=0;;){

        time(&current_time);
        if((current_time-start_time)>=dhcpoffer_timeout)
            break;



        memset(&source,0,sizeof(source));
        memset(&offer_packet,0,sizeof(offer_packet));


        result=receive_dhcp_packet(&offer_packet,sizeof(offer_packet),sock,dhcpoffer_timeout,&source);

        if(result!=OK){

                printf("Result=ERROR\n");

            continue;
        }
        else{


            responses++;
        }


            printf("DHCPOFFER from IP address %s\n",inet_ntoa(source.sin_addr));
            printf("DHCPOFFER XID: %lu (0x%X)\n",(unsigned long) ntohl(offer_packet.xid),ntohl(offer_packet.xid));
            memcpy(&offer_xid,&offer_packet.xid,sizeof(offer_packet.xid));

        /* check packet xid to see if its the same as the one we used in the discover packet */
        if(ntohl(offer_packet.xid)!=packet_xid){
            if (verbose)
                printf("DHCPOFFER XID (%lu) did not match DHCPDISCOVER XID (%lu) - ignoring packet\n",(unsigned long) ntohl(offer_packet.xid),(unsigned long) packet_xid);

            continue;
        }

        /* check hardware address */
        result=OK;

        printf("DHCPOFFER chaddr: ");

        for(x=0;x<ETHERNET_HARDWARE_ADDRESS_LENGTH;x++){

            printf("%02X",(unsigned char)offer_packet.chaddr[x]);

            if(offer_packet.chaddr[x]!=client_hardware_address[x])
                result=ERROR;
        }

        printf("\n");

        if(result==ERROR){
            if (verbose)
                printf("DHCPOFFER hardware address did not match our own - ignoring packet\n");

            continue;
        }


        printf("DHCPOFFER ciaddr: %s\n",inet_ntoa(offer_packet.ciaddr));
        printf("DHCPOFFER yiaddr: %s\n",inet_ntoa(offer_packet.yiaddr));
        printf("DHCPOFFER siaddr: %s\n",inet_ntoa(offer_packet.siaddr));
        printf("DHCPOFFER giaddr: %s\n",inet_ntoa(offer_packet.giaddr));


        add_dhcp_offer(source.sin_addr,&offer_packet);
        /*requesting the offerd address*/
        memcpy(&received_address,&offer_packet.yiaddr,sizeof(offer_packet.yiaddr));
        valid_responses++;
    }


    printf("Total responses seen on the wire: %d\n",responses);
    printf("Valid responses for this machine: %d\n",valid_responses);


    return OK;
}


/* adds a DHCP OFFER to list in memory */
int add_dhcp_offer(struct in_addr source,dhcp_packet *offer_packet){
    dhcp_offer *new_offer;
    int x;
    int y;
    unsigned option_type;
    unsigned option_length;
    printf("offer\n");
    if(offer_packet==NULL)
        return ERROR;

    /* process all DHCP options present in the packet */
    for(x=4;x<MAX_DHCP_OPTIONS_LENGTH;){


        /* end of options (0 is really just a pad, but bail out anyway) */
        if((int)offer_packet->options[x]==-1 || (int)offer_packet->options[x]==0)
            break;

        /* get option type */
        option_type=offer_packet->options[x++];

        /* get option length */
        option_length=offer_packet->options[x++];

        printf("Option: %d (0x%02X)\n",option_type,option_length);


        /* get option data */
        if(option_type==DHCP_OPTION_LEASE_TIME) {
            memcpy(&dhcp_lease_time, &offer_packet->options[x],
                   sizeof(dhcp_lease_time));
            dhcp_lease_time = ntohl(dhcp_lease_time);
        }
        if(option_type==DHCP_OPTION_RENEWAL_TIME) {
            memcpy(&dhcp_renewal_time, &offer_packet->options[x],
                   sizeof(dhcp_renewal_time));
            dhcp_renewal_time = ntohl(dhcp_renewal_time);
        }
        if(option_type==DHCP_OPTION_REBINDING_TIME) {
            memcpy(&dhcp_rebinding_time, &offer_packet->options[x],
                   sizeof(dhcp_rebinding_time));
            dhcp_rebinding_time = ntohl(dhcp_rebinding_time);
        }

            /* skip option data we're ignoring */
        else
            for(y=0;y<option_length;y++,x++);
    }


        if(dhcp_lease_time==DHCP_INFINITE_TIME)
            printf("Lease Time: Infinite\n");
        else
            printf("Lease Time: %lu seconds\n",(unsigned long)dhcp_lease_time);
        if(dhcp_renewal_time==DHCP_INFINITE_TIME)
            printf("Renewal Time: Infinite\n");
        else
            printf("Renewal Time: %lu seconds\n",(unsigned long)dhcp_renewal_time);
        if(dhcp_rebinding_time==DHCP_INFINITE_TIME)
            printf("Rebinding Time: Infinite\n");
        printf("Rebinding Time: %lu seconds\n",(unsigned long)dhcp_rebinding_time);


    new_offer=(dhcp_offer *)malloc(sizeof(dhcp_offer));

    if(new_offer==NULL)
        return ERROR;

    new_offer->server_address=source;
    new_offer->offered_address=offer_packet->yiaddr;
    new_offer->lease_time=dhcp_lease_time;
    new_offer->renewal_time=dhcp_renewal_time;
    new_offer->rebinding_time=dhcp_rebinding_time;



    printf("Added offer from server @ %s",inet_ntoa(new_offer->server_address));
    printf(" of IP address %s\n",inet_ntoa(new_offer->offered_address));
    printf("time renewal %d\n",dhcp_renewal_time);


    /* add new offer to head of list */
    new_offer->next=dhcp_offer_list;
    dhcp_offer_list=new_offer;

    return OK;
}


/* frees memory allocated to DHCP OFFER list */
int free_dhcp_offer_list(void){
    dhcp_offer *this_offer;
    dhcp_offer *next_offer;

    for(this_offer=dhcp_offer_list;this_offer!=NULL;this_offer=next_offer){
        next_offer=this_offer->next;
        free(this_offer);
    }

    return OK;
}


/* frees memory allocated to requested server list */
int free_requested_server_list(void){
    requested_server *this_server;
    requested_server *next_server;

    for(this_server=requested_server_list;this_server!=NULL;this_server=next_server){
        next_server=this_server->next;
        free(this_server);
    }

    return OK;
}

int send_dhcp_renew(int sock){
    printf("request\n");
    dhcp_packet packet;
    struct sockaddr_in sockaddr_broadcast;


    /* clear the packet data structure */
    bzero(&packet,sizeof(packet));


    /* boot request flag (backward compatible with BOOTP servers) */
    packet.op=BOOTREQUEST;

    /* hardware address type */
    packet.htype=ETHERNET_HARDWARE_ADDRESS;

    /* length of our hardware address */
    packet.hlen=ETHERNET_HARDWARE_ADDRESS_LENGTH;

    packet.hops=0;


    memcpy(&packet.xid,&offer_xid,sizeof(offer_xid));

    /**** WHAT THE HECK IS UP WITH THIS?!?  IF I DON'T MAKE THIS CALL, ONLY ONE SERVER RESPONSE IS PROCESSED!!!! ****/
    /* downright bizzarre... */
    ntohl(packet.xid);

    /*discover_packet.secs=htons(65535);*/
    packet.secs=0x0;

    /* tell server it should broadcast its response */
    packet.flags=htons(DHCP_BROADCAST_FLAG);

    memcpy(&packet.ciaddr,&received_address,sizeof(received_address));


    /* our hardware address */
    memcpy(packet.chaddr,client_hardware_address,ETHERNET_HARDWARE_ADDRESS_LENGTH);


    /* first four bytes of options field is magic cookie (as per RFC 2132) */
    packet.options[0]='\x63';
    packet.options[1]='\x82';
    packet.options[2]='\x53';
    packet.options[3]='\x63';

    /* DHCP message type is embedded in options field */
    packet.options[4]=DHCP_OPTION_MESSAGE_TYPE;    /* DHCP message type option identifier */
    packet.options[5]='\x01';               /* DHCP message option length in bytes */
    packet.options[6]=DHCPRENEW;

    packet.options[7]='\xFF';





    /* send the DHCPRENEW packet to broadcast address */
    sockaddr_broadcast.sin_family=AF_INET;
    sockaddr_broadcast.sin_port=htons(DHCP_SERVER_PORT);
    sockaddr_broadcast.sin_addr.s_addr=INADDR_BROADCAST;
    bzero(&sockaddr_broadcast.sin_zero,sizeof(sockaddr_broadcast.sin_zero));



    printf("DHCPRENEW to %s port %d\n",inet_ntoa(sockaddr_broadcast.sin_addr),ntohs(sockaddr_broadcast.sin_port));
    printf("DHCPRENEW XID: %lu (0x%X)\n",(unsigned long) ntohl(packet.xid),ntohl(packet.xid));
    printf("DHCPRENEW ciaddr:  %s\n",inet_ntoa(packet.ciaddr));


    /* send the DHCRENEW packet out */
    send_dhcp_packet(&packet,sizeof(packet),sock,&sockaddr_broadcast);



    return OK;
}
int send_dhcp_request(int sock,struct in_addr offered_address){
    printf("request\n");
    dhcp_packet request_packet;
    struct sockaddr_in sockaddr_broadcast;
    u_int8_t parameter_req_list[] = {DHCP_OPTION_REQUESTED_ADDRESS};

    /* clear the packet data structure */
    bzero(&request_packet,sizeof(request_packet));


    /* boot request flag (backward compatible with BOOTP servers) */
    request_packet.op=BOOTREQUEST;

    /* hardware address type */
    request_packet.htype=ETHERNET_HARDWARE_ADDRESS;

    /* length of our hardware address */
    request_packet.hlen=ETHERNET_HARDWARE_ADDRESS_LENGTH;

    request_packet.hops=0;


    memcpy(&request_packet.xid,&offer_xid,sizeof(offer_xid));

    /**** WHAT THE HECK IS UP WITH THIS?!?  IF I DON'T MAKE THIS CALL, ONLY ONE SERVER RESPONSE IS PROCESSED!!!! ****/
    /* downright bizzarre... */
    ntohl(request_packet.xid);

    /*discover_packet.secs=htons(65535);*/
    request_packet.secs=0x0;

    /* tell server it should broadcast its response */
    request_packet.flags=htons(DHCP_BROADCAST_FLAG);




    /* our hardware address */
    memcpy(request_packet.chaddr,client_hardware_address,ETHERNET_HARDWARE_ADDRESS_LENGTH);
    memcpy(&request_packet.yiaddr,&offered_address,sizeof(offered_address));

    /* first four bytes of options field is magic cookie (as per RFC 2132) */
    request_packet.options[0]='\x63';
    request_packet.options[1]='\x82';
    request_packet.options[2]='\x53';
    request_packet.options[3]='\x63';

    /* DHCP message type is embedded in options field */
    int len=4;
    u_int8_t option=DHCPREQUEST;

    len+=fill_dhcp_option(&request_packet.options[len],DHCP_OPTION_MESSAGE_TYPE,&option,sizeof(option));   /* DHCP message type option identifier */


    request_packet.options[len]= DHCP_OPTION_REQUESTED_ADDRESS;
    len+=(sizeof(u_int8_t));
    request_packet.options[len]= '\x04';
    len+=(sizeof(u_int8_t));
    memcpy(&request_packet.options[len],&offered_address,sizeof(offered_address));
    len+=(sizeof(offered_address));
    len += fill_dhcp_option(&request_packet.options[len], MESSAGE_TYPE_PARAMETER_REQ_LIST, (u_int8_t *)&parameter_req_list, sizeof(parameter_req_list));

    option=0;
   // request_packet.options[MAX_DHCP_OPTIONS_LENGTH-1]='\xFF';
    len += fill_dhcp_option(&request_packet.options[len], DHCP_OPTION_END_MESSAGE, &option, sizeof(option));






    /* send the DHCPDISCOVER packet to broadcast address */
    sockaddr_broadcast.sin_family=AF_INET;
    sockaddr_broadcast.sin_port=htons(DHCP_SERVER_PORT);
    sockaddr_broadcast.sin_addr.s_addr=INADDR_BROADCAST;
    bzero(&sockaddr_broadcast.sin_zero,sizeof(sockaddr_broadcast.sin_zero));



    printf("DHCPREQUEST to %s port %d\n",inet_ntoa(sockaddr_broadcast.sin_addr),ntohs(sockaddr_broadcast.sin_port));
    printf("DHCPREQUEST XID: %lu (0x%X)\n",(unsigned long) ntohl(request_packet.xid),ntohl(request_packet.xid));
    printf("DHCPREQUEST ciaddr:  %s\n",inet_ntoa(request_packet.ciaddr));
    printf("DHCPREQUEST yiaddr:  %s\n",inet_ntoa(request_packet.yiaddr));


    /* send the DHCPREQUEST packet out */
    send_dhcp_packet(&request_packet,sizeof(request_packet),sock,&sockaddr_broadcast);



    return OK;
}
int send_dhcp_release(int sock){
    printf("release\n");
    dhcp_packet release_packet;
    struct sockaddr_in sockaddr_broadcast;


    /* clear the packet data structure */
    bzero(&release_packet,sizeof(release_packet));


    /* boot request flag (backward compatible with BOOTP servers) */
    release_packet.op=BOOTREQUEST;

    /* hardware address type */
    release_packet.htype=ETHERNET_HARDWARE_ADDRESS;

    /* length of our hardware address */
    release_packet.hlen=ETHERNET_HARDWARE_ADDRESS_LENGTH;

    release_packet.hops=0;


    /* transaction id is supposed to be random */
            srand(time(NULL));
    packet_xid=(uint32_t)random();
    release_packet.xid=htonl(packet_xid);

    /**** WHAT THE HECK IS UP WITH THIS?!?  IF I DON'T MAKE THIS CALL, ONLY ONE SERVER RESPONSE IS PROCESSED!!!! ****/
    /* downright bizzarre... */
   // ntohl(release_packet.xid);

    /*discover_packet.secs=htons(65535);*/
    release_packet.secs=0;

    /* tell server it should broadcast its response */
    release_packet.flags=0;

    memcpy(&release_packet.ciaddr,&received_address,sizeof(received_address));


    /* our hardware address */
    memcpy(release_packet.chaddr,client_hardware_address,ETHERNET_HARDWARE_ADDRESS_LENGTH);


    /* first four bytes of options field is magic cookie (as per RFC 2132) */
    release_packet.options[0]='\x63';
    release_packet.options[1]='\x82';
    release_packet.options[2]='\x53';
    release_packet.options[3]='\x63';
    release_packet.options[4]=DHCP_OPTION_END_MESSAGE;
    /* DHCP message type is embedded in options field */

    /* DHCP message type is embedded in options field */
    release_packet.options[4]=DHCP_OPTION_MESSAGE_TYPE;    /* DHCP message type option identifier */
    release_packet.options[5]='\x01';               /* DHCP message option length in bytes */
    release_packet.options[6]=DHCPRELEASE;
    release_packet.options[7]=DHCP_OPTION_END_MESSAGE;



    /* send the DHCPDISCOVER packet to broadcast address */
    sockaddr_broadcast.sin_family=AF_INET;
    sockaddr_broadcast.sin_port=htons(DHCP_SERVER_PORT);
    sockaddr_broadcast.sin_addr.s_addr=INADDR_BROADCAST;
    bzero(&sockaddr_broadcast.sin_zero,sizeof(sockaddr_broadcast.sin_zero));



    printf("DHCPRELEASE to %s port %d\n",inet_ntoa(sockaddr_broadcast.sin_addr),ntohs(sockaddr_broadcast.sin_port));
    printf("DHCPRELEASE XID: %lu (0x%X)\n",(unsigned long) ntohl(release_packet.xid),ntohl(release_packet.xid));
    printf("DHCPRELEASE ciaddr:  %s\n",inet_ntoa(release_packet.ciaddr));


    /* send the DHCPREQUEST packet out */
    send_dhcp_packet(&release_packet,sizeof(release_packet),sock,&sockaddr_broadcast);



    return OK;
}
void print_adress(){


    printf("ADRESSSSDASD %s\n",inet_ntoa(received_address));
    printf("DHCPDISCOVER XID: %lu (0x%X)\n",(unsigned long) ntohl(offer_xid),ntohl(offer_xid));
};