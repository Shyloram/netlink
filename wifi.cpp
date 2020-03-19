#include <stdio.h>                                                                                                                                   
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h> 
#include "wifi.h"
#include "wpaclient.h"

static int wifi_status;
static int wifi_changebyinterface;
static int g_factoryflag;
static int g_processAlive;
#ifdef EN_ZINK
static int g_sapAlive;
#endif
static char g_ssid[100];
static char g_pawd[100];
pthread_mutex_t g_mutex;

static int parseFile_getSSIDPWD(std::string & ssid,std::string & passwd,std::string & userid)
{
	FILE *fp;
	int i;
	char buff[3][100] = {};

	fp = fopen(AP_CONF_PATH,"r");
	if(fp == NULL)
	{
		perror("fopen error:");
		return -1;
	}
	for(i = 0;i < 3;i++)
	{
		fscanf(fp,"%*[^=]=%[^\n]",buff[i]);
	}
	ssid = buff[0];
	passwd = buff[1];
	userid = buff[2];
	fclose(fp);
	unlink(AP_CONF_PATH);
    return 0;
}

int check_wpa_status(wifi::WPAClient& wpa_client)
{
	int i;
	for(i = 0;i < 3;i++)
	{
		if(wpa_client.GetWPAStatus())
		{
			return WIFI_STEP_DHCP_IP;
		}
	}
	return WIFI_STEP_SCAN;
}

#ifdef EN_ZINK
void start_sap()
{
	g_sapAlive = 1;
	char cmd[256] = {0};
	//open soft AP
	NOTICE("StartSoftAP_Mode Starting \n");
	wifi::wifi_system("ifconfig wlan1 down");
	usleep(100000);
	wifi::wifi_system("ifconfig wlan1 up");
	usleep(200000);
	sprintf(cmd, "%s wlan1 mode 3", IWCONFIG_TOOL);
	wifi::wifi_system(cmd);
	usleep(200000);
	wifi::wifi_system("ifconfig wlan1 192.168.10.1");
	usleep(100000);
	// soft AP: ZMD_SAP
	sprintf(cmd, "%s -B %s", HOSTAPD_TOOL, HOSTAPD_CONF_FILE);
	wifi::wifi_system(cmd);
	//udhcpd
	sprintf(cmd, "%s %s", UDHCPD_TOOL, DHCPD_CONF_FILE);
	wifi::wifi_system(cmd);
	//httpd
	sprintf(cmd, "%s -p 8087 -h %s", HTTPD_TOOL, HTTP_DIR);
	wifi::wifi_system(cmd);
	sprintf(cmd, "%s -p 8086 -h %s", HTTPD_TOOL, HTTP_TIMEOUT_DIR);
	wifi::wifi_system(cmd);
}

void stop_sap()
{
	char cmd[256] = {0};
	if(g_sapAlive)
	{
		g_sapAlive = 0;
		wifi::wifi_system("killall -9 hostapd");
		usleep(200000);
		wifi::wifi_system("killall -9 udhcpd");
		wifi::wifi_system("killall -9 httpd");

		NOTICE("<WLAN1> Stop ZMD_SAP Server OK:");
		wifi::wifi_system("ifconfig wlan1 0.0.0.0");
		usleep(100000);
		sprintf(cmd, "%s wlan1 mode 2", IWCONFIG_TOOL);
		wifi::wifi_system(cmd);
		usleep(100000);
		wifi::wifi_system("ifconfig wlan1 down");
	}
}
#endif

int scan_ssid_password(wifi::WPAClient& wpa_client)
{
	int flag = 10;

#ifdef EN_ZINK
	int first_in_sap = 1;
#endif

	while(1)
	{
		if(!g_factoryflag)
		{
			//check ZMD_AP 
			if(flag++ % 5 == 0)
			{
				NOTICE("check ZMD_AP(times:%d)!\n",flag);
				if(wpa_client.ScanAP("ZMD_AP"))
				{
					NOTICE("scan ap found ZMD_AP\n");
					return WIFI_STEP_GET_FROM_AP;
				}
				sleep(1);
			}

			//check FILE
			NOTICE("check FILE(times:%d)!\n",flag);
			if(access(AP_CONF_PATH, F_OK) == 0)
			{
				return WIFI_STEP_GET_FROM_FILE;
			}
		}

		//check Interface
		NOTICE("check Interface(times:%d)!\n",flag);
		if(wifi_changebyinterface)
		{
			return WIFI_STEP_GET_FROM_INTERFACE;
		}

		//check status
		NOTICE("check status(times:%d)!\n",flag);
		if(wpa_client.GetWPAStatus())
		{
			return WIFI_STEP_DHCP_IP;
		}

		sleep(1);

#ifdef EN_ZINK
		if(first_in_sap)
		{
			start_sap();
			first_in_sap = 0;
		}
#endif
	
		//reconfig wifi  
		if(flag > 30)
		{
			flag = 1;
			NOTICE("scan 30 times over,reconfig wifi!\n");
			wpa_client.ReconfigureWiFi();
			sleep(1);
		}
	}
}

int get_ssid_passwd_from_file(wifi::WPAClient& wpa_client)
{
	std::string ssid;
	std::string passwd;
	std::string userid;

	if(!parseFile_getSSIDPWD(ssid,passwd,userid))
	{
		NOTICE("get from file success ssid:%s,passwd:%s,userid:%s\n",ssid.data(),passwd.data(),userid.data());
		wpa_client.SetSsidPassword(ssid, passwd, userid, wifi::GET_FROM_FILE);
		return WIFI_STEP_CONNECTING;
	}
	return WIFI_STEP_SCAN;
}

void Set_Static_Address(void)
{
	char cmdstr[128] = {0};
	struct ifreq ifVec;
	int sock,i;
	unsigned int seed = 0;
	unsigned int static_ap;

	do
	{
		if((sock=socket(AF_INET,SOCK_STREAM,0)) <0)
		{
			perror( "socket ");
			break;
		}
		strcpy(ifVec.ifr_name,"wlan0");
		if(ioctl(sock,SIOCGIFHWADDR,&ifVec) <0)
		{
			perror( "ioctl ");
			break;
		}
		for(i = 3; i < 6; i++)
		{
			seed += ifVec.ifr_hwaddr.sa_data[i];
		}
	}while(0);

	srand(seed);
	static_ap = (unsigned int)(rand()%240 + 2);
	sprintf(cmdstr, "ifconfig wlan0 192.168.10.%d",static_ap);
	wifi::wifi_system(cmdstr);
}

int client_ap(std::string & ssid,std::string & password,std::string & userid)
{
    int					sockfd;
    int					recvbytes;
    char				buf[1024];
    struct sockaddr_in	serv_addr;
    char				msg[32] = {0};
    int					times = 0;
    struct timeval		timeout = {3,0};
    char				tssid[100] = {0};
    char				tpwd[100] = {0};
    char				tuserid[100] = {0};

    /* 客户程序开始建立sockfd描述符 */
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket initialization failure.\n");
        return -1;
    }

    /* 客户程序填充服务端的资料 */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVPORT);
    serv_addr.sin_addr.s_addr = inet_addr("192.168.10.1");
    bzero(&(serv_addr.sin_zero), 8);

    /*客户程序发起连接请求, 设置发送接收超时时限为3秒*/
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char * )&timeout, sizeof(struct timeval));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char * )&timeout, sizeof(struct timeval));

    while(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(struct sockaddr)) == -1)
    {
        if (times++ == 20)
        {
            close(sockfd);
            return -1;
        }
        perror("connectc Error!");
		usleep(100 * 1000);
    }

	/* 将设置sockfd的阻塞操作放到connect()后, 否则容易出现错误<Operation now in progress> */
	/* Set socket fd IO mode to NONBLOCK */
	fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);

    /*连接成功了*/
    //send message
	times = 0;
    strncpy(msg, "Get ssid, password.", sizeof(msg));
    while(send(sockfd, msg, strlen(msg), 0) == -1)	//设置为阻塞
    {
        if(times++ == 20)
        {
            close(sockfd);
            return -1;
        }
        perror("send error!");
		usleep(100 * 1000);
    }

	times = 0;
	while(1)
	{
		//设置为非阻塞,防止IPC虚连接在ZMD_AP时，获取不到服务端的数据，一直阻塞于此
		recvbytes = recv(sockfd, buf, 1024, MSG_DONTWAIT);
		if(recvbytes == -1)
		{
			perror("recv Error!");
			if(times++ == 50)
			{
				close(sockfd);
				return -1;
			}
			usleep(100 * 1000);
		}
		else if(recvbytes == 0)
		{
			ERROR("connection is terminated\n");
			close(sockfd);
			return -1;
		}
		else
		{
			break;
		}
	}

    buf[recvbytes] = '\0';

    /* Receive content is "SSID=xx&PWD=xx&Userid=" */
	NOTICE("recv buf is %s\n",buf);

	memcpy(tssid, buf + strlen("SSID="), strstr(buf,"&PWD=") - buf - strlen("SSID="));
	memcpy(tpwd, strstr(buf,"&PWD=") + strlen("&PWD="), strstr(buf,"&Userid=") - strstr(buf,"&PWD=") - strlen("&PWD="));
	memcpy(tuserid, strstr(buf,"&Userid=") + strlen("&Userid="), strlen(buf) - (strstr(buf,"&Userid=") - buf) - strlen("&Userid="));

	ssid = tssid;
	password = tpwd;
	userid = tuserid;

    /*结束通讯*/
    close(sockfd);

    return 0;
}

int connect_wifi(wifi::WPAClient& wpa_client)
{
	int timeout = 200;

	if(!wpa_client.ConnectWiFi())
	{
		ERROR("connectwifi failed!\n");
		return WIFI_STEP_SCAN;
	}
	while(!wpa_client.GetWPAStatus())
	{
		if(timeout-- <= 0)
		{
			ERROR("connect wifi time out\n");
			sleep(1);
			wpa_client.ReconfigureWiFi();
			return WIFI_STEP_SCAN;
		}
		NOTICE("wifi connecting [%d00ms]\n",200 - timeout);
		usleep(100 * 1000);
	}
	if(wpa_client.GetFlag())
	{
		return WIFI_STEP_DHCP_IP;
	}
	else
	{
		return WIFI_STEP_CONNECT_AP_OK;
	}
}

int get_ssid_passwd_from_AP(wifi::WPAClient& wpa_client)
{
	int ret;
	std::string ssid;
	std::string passwd;
	std::string userid;

	//connect zmd_ap
	wpa_client.SetSsidPassword("ZMD_AP", "", "", wifi::GET_FROM_AP);
	ret = connect_wifi(wpa_client);
	if(ret != WIFI_STEP_DHCP_IP)
	{
		ERROR("connect ZMD_AP failed!\n");
		return WIFI_STEP_SCAN;
	}

	//static ip
	Set_Static_Address();

	//get ssid passwd
	if(client_ap(ssid,passwd,userid) == -1)
	{
		ERROR("get ssid password from ap server failed!\n");
		wpa_client.ReconfigureWiFi();
		return WIFI_STEP_SCAN;
	}
	
	//set ssid passwd
	NOTICE("get from Ap success ssid:%s,passwd:%s,userid:%s\n",ssid.data(),passwd.data(),userid.data());
	wpa_client.SetSsidPassword(ssid, passwd, userid, wifi::GET_FROM_AP);
	return WIFI_STEP_CONNECTING;
}

int get_ssid_passwd_from_interface(wifi::WPAClient& wpa_client)
{
	std::string ssid;
	std::string passwd;
	std::string userid;

	pthread_mutex_lock(&g_mutex);
	ssid = g_ssid;
	passwd = g_pawd;
	pthread_mutex_unlock(&g_mutex);

	//set ssid passwd
	NOTICE("get from Interface success ssid:%s,passwd:%s,userid:%s\n",ssid.data(),passwd.data(),userid.data());
	wpa_client.SetSsidPassword(ssid, passwd, userid, wifi::GET_FROM_AP);
	wifi_changebyinterface = 0;
	return WIFI_STEP_CONNECTING;
}

void *SendConnectInfo_Process(void *arg)
{
	int					recvbytes;
	unsigned char 		buf[1024];
	CONNECT_INFO 		*connect_info = (CONNECT_INFO *)arg;

	if((recvbytes = recv(connect_info->fd, buf, 1024, 0)) == -1)
	{
		perror("recv Error!");
		close(connect_info->fd);
		return NULL;
	}

	buf[recvbytes] = '\0';
	NOTICE("Received: %s\n", buf);

	if(send(connect_info->fd, connect_info->msg, strlen(connect_info->msg), 0) == -1)
	{
		perror("send Error");
		close(connect_info->fd);
		return NULL;
	}

	sleep(5);
	NOTICE("send SSID to cli_fd: %d\n",connect_info->fd);
	close(connect_info->fd);
	pthread_exit(NULL);
}

void *SoftAPServer_Thread(void *para)
{
	int					sockfd = -1;
	int					client_fd;
    int                 cli_count = 0;
	int					on = 1;
	int					timeout_cnt = 300;
	struct sockaddr_in	host_addr;
	struct sockaddr_in	remote_addr;
	char 				send_msg[256] = {0};
	char				cmd[256] = {0};
	std::string         ssid;
	std::string         password;
	std::string         userid;
	wifi::WPAClient *wpa_client = (wifi::WPAClient *)para;

	//open soft AP
	NOTICE("StartSoftAP_Mode Starting \n");
	wifi::wifi_system("ifconfig wlan1 down");
	usleep(100000);
	wifi::wifi_system("ifconfig wlan1 up");
	usleep(200000);
	sprintf(cmd, "%s wlan1 mode 3", IWCONFIG_TOOL);
	wifi::wifi_system(cmd);
	usleep(200000);
	wifi::wifi_system("ifconfig wlan1 192.168.10.1");
	usleep(100000);
	// soft AP: ZMD_AP
	sprintf(cmd, "%s -B %s", HOSTAPD_TOOL, HOSTAPD_CONF_FILE2);
	wifi::wifi_system(cmd);
	sleep(1);

	//start AP server
	NOTICE("ZMD_AP_Server Starting \n");
	do
	{
		if((sockfd = socket(AF_INET, SOCK_STREAM, 0))  == -1)
		{
			perror("socket initialization Error.");
			break;
		}

		host_addr.sin_family = AF_INET;
		host_addr.sin_port = htons(SERVPORT);
		host_addr.sin_addr.s_addr = INADDR_ANY;
		bzero(&(host_addr.sin_zero), 8);

		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0 )
		{
			ERROR( "sockfd SO_REUSEADDR failed !\n");
		}

		if(bind(sockfd, (struct sockaddr *)&host_addr, sizeof(struct sockaddr)) == -1)
		{
			perror("bind Error");
			close(sockfd);
			break;
		}

		if(listen(sockfd, SAP_BACKLOG) ==-1)
		{
			perror("listen Error!");
			close(sockfd);
			break;
		}

		/* Set socket fd IO mode to NONBLOCK */
		fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);

		//GETSSIDPASSWROD
		wpa_client->GetSsidPassword(ssid,password,userid);
		snprintf(send_msg, sizeof(send_msg), "SSID=%s&PWD=%s&Userid=%s", ssid.data(), password.data(),userid.data());
		NOTICE("send_msg: %s\n", send_msg);

		while(timeout_cnt--) //150 == 300sec == 5min
		{
			socklen_t sin_size = sizeof(struct sockaddr_in);
			if((client_fd = accept(sockfd, (struct sockaddr *)&remote_addr, &sin_size)) == -1)
			{
				//perror("accept Error");
				sleep(1);
				NOTICE("AP server alive[%d]\n",timeout_cnt);
				continue;
			}

			NOTICE("received a connection from %s\n", inet_ntoa(remote_addr.sin_addr));

			pthread_t tid;
			CONNECT_INFO send_info;
			send_info.fd = client_fd;
			send_info.msg = send_msg;
			if(pthread_create(&tid, NULL, SendConnectInfo_Process, &send_info) == 0)
			{
				NOTICE("Create SendConnectInfo_Process tid= %d , cli_count = %d\n", (int)tid, cli_count);
			}
			else
			{
				ERROR("Create SendConnectInfo_Process error\n");
			}

			NOTICE("cli_count = %d\n", cli_count++);

		}
		close(sockfd);
	}while(0);

	//close soft AP
	wifi::wifi_system("killall -9 hostapd");
	usleep(200000);
	NOTICE("<WLAN1> Stop ZMD_AP Server OK:");
	wifi::wifi_system("ifconfig wlan1 0.0.0.0");
	usleep(100000);
	sprintf(cmd, "%s wlan1 mode 2", IWCONFIG_TOOL);
	wifi::wifi_system(cmd);
	usleep(100000);
	wifi::wifi_system("ifconfig wlan1 down");
	return 0;
}

int open_AP(wifi::WPAClient *wpa_client)
{
	stop_sap();
	pthread_t pid;
	if(pthread_create(&pid,NULL,SoftAPServer_Thread,(void*)wpa_client) < 0)
	{
		ERROR("pthread_create failed!\n");
	}
	return WIFI_STEP_DHCP_IP;
}

int dhcp_ip(wifi::WPAClient& wpa_client)
{
	static int times = 0;
	stop_sap();
	if(!wpa_client.DhcpIP())
	{
		ERROR("dhcp ip failed!\n");
		if(wpa_client.GetWPAStatus())
		{
			if(times++ == 5)
			{
				times = 0;
				ERROR("dhcp timeout return scan!\n");
				return WIFI_STEP_SCAN;
			}
			return WIFI_STEP_DHCP_IP;
		}
		times = 0;
		ERROR("dhcp wifi disconnect return scan!\n");
		return WIFI_STEP_SCAN;
	}
	times = 0;
	NOTICE("dhcp ip successed!\n");
	return WIFI_STEP_SUCCESS;
}

int check_wifi_status(wifi::WPAClient& wpa_client)
{
	if(!wpa_client.GetWPAStatus())
	{
		NOTICE("WIFI Disconnected Now!\n");
		wifi_status = 0;
		return WIFI_STEP_SCAN;
	}

	if(wifi_status == 0)
	{
		wpa_client.SaveconfigWiFi();
		wpa_client.SaveUseridFile(AP_FILE_PATH);
		wifi_status = 1;
		set_led_status(WIFI_LED_B_ON);
		NOTICE("STEP: WIFI_STEP_SUCCESS\n");
	}

	if(wifi_changebyinterface)
	{
		NOTICE("WIFI Will Change Now!\n");
		wifi_status = 0;
		return WIFI_STEP_GET_FROM_INTERFACE;
	}

	sleep(5);
	return WIFI_STEP_SUCCESS;
}

void* WifiProcess(void* para)
{
	prctl(PR_SET_NAME, __FUNCTION__);
	pthread_detach(pthread_self());
	wifi::WPAClient wpa_client;
	int step = WIFI_STEP_IDLE;

	if(!wpa_client.GetInitStatus()) 
	{
		ERROR("wpa client init failed!\n");
		pthread_exit(0);
	}

	while(g_processAlive)
	{
		switch(step)
		{
			case WIFI_STEP_IDLE:
				NOTICE("STEP: WIFI_STEP_IDLE\n");
				set_led_status(WIFI_LED_G_FLASH);
				step = check_wpa_status(wpa_client);
				break;

			case WIFI_STEP_SCAN:
				NOTICE("STEP: WIFI_STEP_SCAN\n");
				set_led_status(WIFI_LED_G_FLASH);
				step = scan_ssid_password(wpa_client);
				break;

			case WIFI_STEP_GET_FROM_FILE:
				NOTICE("STEP: WIFI_STEP_GET_FROM_FILE\n");
				step = get_ssid_passwd_from_file(wpa_client);
				break;

			case WIFI_STEP_GET_FROM_AP:
				NOTICE("STEP: WIFI_STEP_GET_FROM_AP\n");
				step = get_ssid_passwd_from_AP(wpa_client);
				break;

			case WIFI_STEP_GET_FROM_INTERFACE:
				NOTICE("STEP: WIFI_STEP_GET_FROM_INTERFACE\n");
				step = get_ssid_passwd_from_interface(wpa_client);
				break;

			case WIFI_STEP_CONNECTING:
				NOTICE("STEP: WIFI_STEP_CONNECTING\n");
				set_led_status(WIFI_LED_B_FLASH);
				step = connect_wifi(wpa_client);
				break;

			case WIFI_STEP_CONNECT_AP_OK:
				NOTICE("STEP: WIFI_STEP_CONNECT_AP_OK\n");
				step = open_AP(&wpa_client);
				break;

			case WIFI_STEP_DHCP_IP:
				NOTICE("STEP: WIFI_STEP_DHCP_IP\n");
				set_led_status(WIFI_LED_B_FLASH);
				step = dhcp_ip(wpa_client);
				break;
				
			case WIFI_STEP_SUCCESS:
				step = check_wifi_status(wpa_client);
				break;
		}
	}
	NOTICE("WIFI LIB will exit!\n");
	set_led_status(WIFI_LED_B_FLASH);
	wpa_client.DisconnectWiFi(); 
	pthread_exit(0);
}

int InitWifi(int factoryflag)
{
	g_factoryflag = factoryflag;
	g_processAlive = 1;
	pthread_t pid;
	if(pthread_create(&pid,NULL,WifiProcess,NULL) < 0)
	{
		ERROR("pthread_create failed!\n");
		return -1;
	}
	pthread_mutex_init(&g_mutex,NULL);
	return 0;
}

void ReleaseWifi(void)
{
	g_processAlive = 0;
}

void GetWifiVersion(void)
{
	NOTICE("###########eink2 wifi lib %s#############\n",WIFI_VERSION);
}

int GetConnectStatus(void)
{
	return wifi_status;
}

int SetWifiConfig(char* ssid,char* password,int forceflag)
{
	if(NULL == ssid)
	{
		ERROR("ssid is null!\n");
		return -1;
	}

	if(!forceflag && !strcmp(g_ssid,ssid) && !strcmp(g_pawd,password))
	{
		ERROR("ssid password is same!\n");
		return -1;
	}

	wifi_changebyinterface = 1;

	pthread_mutex_lock(&g_mutex);
	memset(g_ssid, 0, sizeof(g_ssid));
	memset(g_pawd, 0, sizeof(g_pawd));
	strncpy(g_ssid, ssid, sizeof(g_ssid));
	strncpy(g_pawd, password, sizeof(g_pawd));
	pthread_mutex_unlock(&g_mutex);

	return 0;
}
