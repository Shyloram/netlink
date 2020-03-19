#ifndef __WPA_CLIENT_H__
#define __WPA_CLIENT_H__

#include <string>
#include <sys/un.h>

//Debug
#define NOTICE(format, ...)     fprintf(stdout, "\033[1m\033[32m[WIFILIBLOG][Func:%s][Line:%d]\033[0m, "format, __FUNCTION__,  __LINE__, ##__VA_ARGS__)
#define ERROR(format, ...)      fprintf(stdout, "\033[1m\033[31m[WIFILIBLOG][Func:%s][Line:%d]\033[0m, "format, __FUNCTION__,  __LINE__, ##__VA_ARGS__)

namespace wifi 
{
	const std::string WPA_PATH = "/tmp/wpa_supplicant/wlan0";

	struct WPAContext 
	{
		int s;
		struct sockaddr_un local;
		struct sockaddr_un dest;
	};

	enum WPAProtocol 
	{
		PROTOCOL_WPA = 0,
		PROTOCOL_WEP,
		PROTOCOL_NONE
	};

	enum GETFUNTION 
	{
		GET_FROM_FILE = 0,
		GET_FROM_AP
	};

	int wifi_system(const char* cmd);

	class MXDHCP 
	{       
		private:
			FILE *m_pstream;

			bool CheckString(char *buf,int len);

		public:
			MXDHCP();
			~MXDHCP();
			bool Start(const std::string& net_interface);
			bool GetDHCPStatus();
	};

	class WPAClient 
	{
		private:
			std::string m_ssid;
			std::string m_password;
			std::string m_userid;
			struct WPAContext* m_wpa_context;
			std::string m_wpa_control_path;
			MXDHCP m_dhcp;
			int m_flag;

			bool Init();
			struct WPAContext* Open(const std::string& path);
			void Close(struct WPAContext* context);
			bool Request(struct WPAContext* context, const std::string& cmd,std::string& reply);
			bool CheckCommandWithOk(const std::string cmd);
			bool AddWiFi(int& id);
			bool SetScanSSID(int id);
			bool SetSSID(const std::string& ssid, int id);
			bool SetPassword(const std::string& password, int id);
			bool SetWEPPassword(const std::string & password, int id);
			bool SetProtocol(int id, int en_crypt);
			bool EnableWiFi(int id);
			bool CleanAllWiFi();
			int  CheckProtocol(std::string & ssid);

		public:
			WPAClient(const std::string& wpa_control_path = WPA_PATH);
			~WPAClient();
			bool GetInitStatus(){return m_wpa_context!=NULL;}						  
			std::string GetCurrentSSID();											  
			bool ReconfigureWiFi();
			bool SaveconfigWiFi();
			bool SaveUseridFile(const char* filepath);
			bool ConnectWiFi();   
			bool DisconnectWiFi();   
			bool GetWPAStatus();
			bool DhcpIP();
			bool ScanAP(const std::string & ssid);
			int  SetSsidPassword(const std::string & ssid,const std::string & password,const std::string & userid,int flag);
			int  GetSsidPassword(std::string & ssid,std::string & password,std::string & userid);
			int  GetFlag();
	};
}
#endif // __WPA_CLIENT_H__
