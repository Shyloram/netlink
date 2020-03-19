#include <string>
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include "wpaclient.h"

namespace wifi 
{
	/*tool*/
	int wifi_system(const char *cmd)
	{
		FILE* fp;
		int res;
		char buff[1024]= {};

		if(cmd == NULL)
		{
			ERROR("input cmd is NULL!\n");
			return -1;
		}

		fp = popen(cmd,"r");
		if(fp == NULL)
		{
			ERROR("popen error:%s!\n",strerror(errno));
			return -1;
		}

		while(fgets(buff,sizeof(buff),fp))
		{
			NOTICE("%s\n",buff);
		}

		res = pclose(fp);
		if(-1 == res)
		{
			ERROR("pclose error:%s!\n",strerror(errno));
			return -1;
		}
		if(WIFEXITED(res))
		{
			NOTICE("subprocess[%s] exited, exit code: %d\n",cmd,WEXITSTATUS(res));
			if(0 == WEXITSTATUS(res))
			{
				NOTICE("command[%s] succeed!\n",cmd);
			}
			else
			{
				if(127 == WEXITSTATUS(res))
				{
					ERROR("command[%s] not found!\n",cmd);
					return WEXITSTATUS(res);
				}
				else
				{
					ERROR("command[%s] failed:%s!\n",cmd,strerror(WEXITSTATUS(res)));
					return WEXITSTATUS(res);
				}
			}
			return res;
		}
		else
		{
			ERROR("subprocess[%s] exit failed\n",cmd);
			return -1;
		}
		return 0;
	}

	/*class MXDHCP*/
	MXDHCP::MXDHCP()
	{
		m_pstream = NULL;
	}

	MXDHCP::~MXDHCP() 
	{
		if(m_pstream!=NULL)
		{
			pclose(m_pstream);
			m_pstream = NULL;
		}
	}

	/*private*/
	bool MXDHCP::CheckString(char *buf ,int len)
	{
		if(strstr(buf,"Adding DNS server")==NULL)
		{
			return false;
		}
		return true;
	}

	/*public*/
	bool MXDHCP::Start(const std::string & net_interface) 
	{
		if(m_pstream!=NULL)
		{
			pclose(m_pstream);
			m_pstream = NULL;
		}
		std::string cmd = "udhcpc -b -i " + net_interface + " -x hostname:Zmodo-IPC &";
		wifi_system("killall udhcpc");
		usleep(100*1000);
		m_pstream = popen(cmd.data(),"r");
		if(m_pstream == NULL)
		{
			ERROR("popen error\n");
			return false;
		}
		return true;
	}

	bool MXDHCP::GetDHCPStatus() 
	{
		if(m_pstream == NULL)
		{
			return false;
		}

		char buff[1024] = {};
		int len = 1024;
		int res = fread(buff,sizeof(char),len,m_pstream);
		if(res<=0)
		{
			return false;
		}
		if(!CheckString(buff,res))
		{
			ERROR("CheckString failed\n");
			return false;
		}
		pclose(m_pstream);
		m_pstream = NULL;
		return true;
	}

	/*class WPAClient*/
	WPAClient::WPAClient(const std::string & wpa_control_path) 
	{
		m_wpa_context = NULL;
		m_wpa_control_path = wpa_control_path;
		m_ssid = "";
		m_password = "";
		m_userid = "";
		Init();
	}

	WPAClient::~WPAClient() 
	{
		Close(m_wpa_context);
	}

	/*private*/
	bool WPAClient::Init() 
	{
		m_wpa_context = Open(m_wpa_control_path);
		if (m_wpa_context == NULL) 
		{
			ERROR("Open failed\n");
			return false;
		}
		return true;
	}

	WPAContext * WPAClient::Open(const std::string & path) 
	{
		struct WPAContext *ctrl;
		static int counter = 0;
		int ret;
		int tries = 0;
		size_t res;

		ctrl = (struct WPAContext*)malloc(sizeof(struct WPAContext));
		if (ctrl == NULL) 
		{
			ERROR("malloc failed\n");
			return NULL;
		}
		memset(ctrl, 0, sizeof(struct WPAContext));

		ctrl->s = socket(PF_UNIX, SOCK_DGRAM, 0);
		if (ctrl->s < 0) 
		{
			ERROR("socket failed\n");
			free(ctrl);
			return NULL;
		}

		ctrl->local.sun_family = AF_UNIX;
		counter++;
try_again:
		ret = snprintf(ctrl->local.sun_path, sizeof(ctrl->local.sun_path),"/tmp/wpa_ctrl_%d-%d",(int)getpid(), counter);
		if (ret < 0 || (size_t)ret >= sizeof(ctrl->local.sun_path)) 
		{
			ERROR("snprintf failed\n");
			close(ctrl->s);
			free(ctrl);
			return NULL;
		}
		tries++;
		if (bind(ctrl->s, (struct sockaddr *) &ctrl->local,sizeof(ctrl->local)) < 0) 
		{
			if (errno == EADDRINUSE && tries < 2) 
			{
				/*
				 * getpid() returns unique identifier for this instance
				 * of wpa_ctrl, so the existing socket file must have
				 * been left by unclean termination of an earlier run.
				 * Remove the file and try again.
				 */
				unlink(ctrl->local.sun_path);
				goto try_again;
			}
			ERROR("bind failed\n");
			close(ctrl->s);
			free(ctrl);
			return NULL;
		}

		ctrl->dest.sun_family = AF_UNIX;
		res = strlcpy(ctrl->dest.sun_path, path.data(),sizeof(ctrl->dest.sun_path));
		if (res >= sizeof(ctrl->dest.sun_path)) 
		{
			ERROR("strlcpy failed\n");
			close(ctrl->s);
			free(ctrl);
			return NULL;
		}
		if (connect(ctrl->s, (struct sockaddr *) &ctrl->dest,sizeof(ctrl->dest)) < 0) 
		{
			ERROR("connect failed\n");
			close(ctrl->s);
			unlink(ctrl->local.sun_path);
			free(ctrl);
			return NULL;
		}
		return ctrl;
	}

	void WPAClient::Close(WPAContext * context) 
	{
		if (context == NULL)
		{
			return;
		}
		unlink(context->local.sun_path);
		if (context->s >= 0)
		{
			close(context->s);
		}
		free(context);
	}

	bool WPAClient::Request(WPAContext * context, const std::string & cmd, std::string & reply) 
	{
		int res;
		fd_set rfds;
		struct timeval tv;

		if (context == NULL) 
		{
			return false;
		}

		if (send(context->s, cmd.data(), cmd.length(), 0) < 0) 
		{
			ERROR("send failed\n");
			return -1;
		}
		while(1)
		{
			tv.tv_sec = 10;
			tv.tv_usec = 0;
			FD_ZERO(&rfds);
			FD_SET(context->s, &rfds);
			res = select(context->s + 1, &rfds, NULL, NULL, &tv);
			if (res < 0)
			{
				ERROR("select failed\n");
				return false;
			}
			if (FD_ISSET(context->s, &rfds)) 
			{
				char temp[10 * 1024] = {0};
				int temp_len = 10 * 1024;
				res = recv(context->s, temp, temp_len, 0);
				if (res < 0)
				{
					ERROR("recv failed\n");
					return false;
				}
				if (res > 0 && temp[0] == '<') 
				{
					continue;
				}
				reply = temp;
				break;
			} 
			else 
			{
				return false;
			}
		}
		return true;
	}

	bool WPAClient::CheckCommandWithOk(const std::string cmd) 
	{
		std::string recv;
		if (!Request(m_wpa_context, cmd, recv)) 
		{
			return false;
		}
		if (strstr(recv.data(), "OK") == NULL) 
		{
			ERROR("CheckCommandWithOk failed:(%s):%s\n",cmd.data(),recv.data());
			return false;
		}
		return true;
	}

	bool WPAClient::AddWiFi(int & id) 
	{
		std::string add_cmd = "ADD_NETWORK";
		std::string recv;
		if (!Request(m_wpa_context, add_cmd, recv)) 
		{
			return false;
		}
		id = atoi(recv.data());
		return true;
	}

	bool WPAClient::SetScanSSID(int id) 
	{
		std::ostringstream  oss;
		oss << "SET_NETWORK " << id << " scan_ssid 1";
		std::string cmd = oss.str();
		return CheckCommandWithOk(cmd);
	}

	bool WPAClient::SetSSID(const std::string & ssid, int id) 
	{
		std::ostringstream  oss;
		oss << "SET_NETWORK " << id << " ssid " << "\"" << ssid << "\"";
		std::string cmd = oss.str();
		return CheckCommandWithOk(cmd);
	}

	bool WPAClient::SetPassword(const std::string & password, int id) 
	{
		std::ostringstream  oss;
		oss << "SET_NETWORK " << id << " psk " << "\"" << password << "\"";
		std::string cmd = oss.str();
		return CheckCommandWithOk(cmd);
	}

	bool WPAClient::SetWEPPassword(const std::string & password, int id) 
	{
		std::ostringstream  oss;
		oss << "SET_NETWORK " << id << " wep_key0 " << "\"" << password << "\"";
		std::string cmd = oss.str();
		return CheckCommandWithOk(cmd);
	}

	bool WPAClient::SetProtocol(int id, int en_crypt) 
	{
		std::ostringstream  oss;
		oss << "SET_NETWORK " << id;
		std::string cmd = oss.str();
		if (en_crypt == PROTOCOL_WPA) 
		{
			cmd += " key_mgmt WPA-PSK";
		} 
		else 
		{
			cmd += " key_mgmt NONE";
		}
		return CheckCommandWithOk(cmd);
	}

	bool WPAClient::EnableWiFi(int id) 
	{
		std::ostringstream  oss;
		oss << "ENABLE_NETWORK " << id;
		std::string cmd = oss.str();
		return CheckCommandWithOk(cmd);
	}

	bool WPAClient::CleanAllWiFi() 
	{
		CheckCommandWithOk("REMOVE_NETWORK all");
		CheckCommandWithOk("DISABLE_NETWORK all");
		return true;
	}

	int WPAClient::CheckProtocol(std::string & ssid)
	{
		std::string cmd = "SCAN_RESULTS";
		std::string recv;
		char recv_ssid[100] = {};
		char recv_protocol[100] = {};
		int pos = 0;
		int ret;
		int i;

		for(i = 0;i<20;i++)
		{
			if (!Request(m_wpa_context, cmd, recv)) 
			{
				return -1;
			}
			if(recv.size() > 48)
			{
				break;
			}
			usleep(100 * 1000);
		}
		if(i >= 20)
		{
			return false;
		}

		while(1)
		{
			pos = recv.find("\n",pos);
			if(pos == -1)
			{
				break;
			}
			memset(recv_ssid,0,sizeof(recv_ssid));
			memset(recv_protocol,0,sizeof(recv_protocol));
			ret = sscanf(recv.data()+pos,"%*[^[]%[^\t]%*[\t]%[^\n]",recv_protocol,recv_ssid);
			if(ret != -1)
			{
				NOTICE("[recv_ssid]:%s,        [recv_protocol]:%s\n",recv_ssid,recv_protocol);
				if(!strcmp(recv_ssid,ssid.data()))
				{
					if(strstr(recv_protocol,"WEP"))
					{
						return PROTOCOL_WEP;
					}
					else
					{
						return PROTOCOL_WPA;
					}
				}
			}
			pos++;
		}
		return PROTOCOL_WPA;
	}

	/*public*/
	std::string WPAClient::GetCurrentSSID()
	{
		std::string cmd = "STATUS";
		std::string ssid_key = "\nssid=";
		std::string recv;
		std::string ssid = "";
		if (!Request(m_wpa_context, cmd, recv)) 
		{
			return "";
		}
		char temp[1024] = {0};
		strcpy(temp, recv.data());
		char *key = NULL;
		key = strstr(temp, ssid_key.data());
		if (key == NULL) 
		{
			return "";
		}
		key += ssid_key.length();
		for (; (*key != '\0') && (*key != '\n') && (*key != '\r'); key++) 
		{
			ssid += *key;
		}
		return ssid;
	}

	bool WPAClient::ReconfigureWiFi()
	{
		CheckCommandWithOk("RECONFIGURE");
		return true;
	}

	bool WPAClient::SaveconfigWiFi()
	{
		CheckCommandWithOk("SAVE_CONFIG");
		return true;
	}

	bool WPAClient::SaveUseridFile(const char* filepath)
	{
		FILE *fd;
		char cmdstr[128];

		if(m_userid.empty())
		{
			return true;
		}
		fd = fopen(filepath,"w+");
		if(fd == NULL)
		{
			ERROR("fopen error:%s!\n",strerror(errno));
			return false;
		}
		sprintf(cmdstr, "%s\n", m_userid.data());
		fprintf(fd,"%s",cmdstr);
		fclose(fd);
		return true;
	}

	bool WPAClient::ConnectWiFi(void) 
	{
		int net_id;
		int protocol;

		if(m_ssid.empty())
		{
			return false;
		}
		if(m_password.empty())
		{
			protocol = PROTOCOL_NONE;
		}
		else
		{
			protocol = CheckProtocol(m_ssid);
		}
		if (!CleanAllWiFi()) 
		{
			ERROR("CleanAllWiFi failed\n");
			return false;
		}
		if (!AddWiFi(net_id)) 
		{
			ERROR("AddWiFi failed\n");
			return false;
		}
		if (!SetSSID(m_ssid, net_id)) 
		{
			ERROR("SetSSID failed\n");
			return false;
		}
		if(protocol == PROTOCOL_WPA)
		{
			if (!SetPassword(m_password, net_id)) 
			{
				ERROR("SetPassword failed\n");
				return false;
			}
		}
		if (!SetProtocol(net_id, protocol)) 
		{
			ERROR("SetProtocol failed\n");
			return false;
		}
		if(protocol == PROTOCOL_WEP)
		{
			if (!SetWEPPassword(m_password, net_id)) 
			{
				ERROR("SetWEPPassword failed\n");
				return false;
			}
		}
		if (!SetScanSSID(net_id))
		{
			ERROR("SetScanSSID failed\n");
			return false;
		}
		if (!EnableWiFi(net_id)) 
		{
			ERROR("EnableWiFi failed\n");
			return false;
		}
		return true;
	}

	bool WPAClient::DisconnectWiFi(void) 
	{
		CheckCommandWithOk("DISCONNECT");
		return true;
	}

	bool WPAClient::GetWPAStatus() 
	{
		std::string cmd = "STATUS";
		std::string recv;
		int addr;
		if (!Request(m_wpa_context, cmd, recv)) 
		{
			return false;
		}
		addr = recv.find("COMPLETED");
		if (addr == -1) 
		{
			//NOTICE("status:%s\n",recv.data());
			return false;
		}
		return true;
	}

	bool WPAClient::DhcpIP() 
	{
		int timeout = 50;
		if (!m_dhcp.Start("wlan0")) 
		{
			return false;
		}
		while(!m_dhcp.GetDHCPStatus()) 
		{
			if(timeout-- <= 0)
			{
				ERROR("dhcp ip time out\n");
				return false;
			}
			usleep(100 * 1000);
		}
		return true;
	}

	bool WPAClient::ScanAP(const std::string & ssid)
	{
		std::string cmd = "SCAN";
		std::string recv;
		char recv_ssid[100] = {};
		char wlaninfo[2048] = {};
		FILE *fd;
		int addr;
		int pos = 0;
		int ret = 0;
		int i = 0;

		while(1)
		{
			if(i > 10)
			{
				NOTICE("WPA SCAN RESURT: FAIL-BUSY(%d) timeout!\n",i);
				return false;
			}
			if (!Request(m_wpa_context, cmd, recv)) 
			{
				return false;
			}
			if (strstr(recv.data(), "OK") == NULL) 
			{
				break;
			}
			else if(strstr(recv.data(), "FAIL-BUSY") == NULL)
			{
				NOTICE("WPA SCAN RESURT: FAIL-BUSY(%d)\n",i++);
				usleep(500 * 1000);
				continue;
			}
			else
			{
				return false;
			}
		}

		cmd = "SCAN_RESULTS";
		recv = "";
		for(i = 0;i<20;i++)
		{
			usleep(100 * 1000);
			if (!Request(m_wpa_context, cmd, recv)) 
			{
				return -1;
			}
			if(recv.size() > 48)
			{
				break;
			}
		}

		//fot debug
		//printf("%s\n",recv.data());

		sprintf(wlaninfo,"[");
		while(1)
		{
			pos = recv.find("\n",pos);
			if(pos == -1)
			{
				break;
			}
			memset(recv_ssid,0,sizeof(recv_ssid));
			ret = sscanf(recv.data()+pos,"%*[^[]%*[^\t]%*[\t]%[^\n]",recv_ssid);
			if(ret != -1 && strlen(recv_ssid) != 0)
			{
				NOTICE("[scan_ssid]:%s\n",recv_ssid);
				sprintf(wlaninfo,"%s\"%s\",",wlaninfo,recv_ssid);
			}
			pos++;
		}
		wlaninfo[strlen(wlaninfo)] = ']';
		fd = fopen("/tmp/wlaninfo.conf","w");
		if(fd)
		{
			fwrite(wlaninfo,1,strlen(wlaninfo),fd);
			fclose(fd);
		}

		addr = recv.find(ssid.data());
		if (addr == -1) 
		{
			return false;
		}
		return true;
	}

	int  WPAClient::SetSsidPassword(const std::string & ssid,const std::string & password,const std::string & userid,int flag)
	{
		m_ssid = ssid;
		m_password = password;
		m_userid = userid;
		m_flag = flag;
		return 0;
	}

	int  WPAClient::GetSsidPassword(std::string & ssid,std::string & password,std::string & userid)
	{
		ssid = m_ssid;
		password = m_password;
		userid = m_userid;
		return 0;
	}

	int WPAClient::GetFlag()
	{
		return m_flag;
	}
}
