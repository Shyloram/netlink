#ifndef __WIFI_H__
#define __WIFI_H__

#define WIFI_VERSION                    "V1.3"
#define AP_CONF_PATH                    "/tmp/ap.conf"
#define AP_FILE_PATH                    "/tmp/ap_file"

#ifdef HISI
#define IWCONFIG_TOOL                   "/app/wifi/tools/iwconfig"
#define HOSTAPD_TOOL                    "/app/wifi/tools/hostapd"
#define HOSTAPD_CONF_FILE2              "/app/wifi/tools/rtl_hostapd_2.conf"
#ifdef EN_ZINK
#define HOSTAPD_CONF_FILE               "/app/wifi/tools/rtl_hostapd.conf"
#define UDHCPD_TOOL                     "/app/wifi/tools/udhcpd"
#define HTTPD_TOOL                      "/app/wifi/tools/httpd"
#define DHCPD_CONF_FILE                 "/app/wifi/tools/dhcp1.conf"
#define HTTP_DIR                        "/app/www"
#define HTTP_TIMEOUT_DIR                "/app/timeout"
#endif
#endif

#ifdef ANKA
#define IWCONFIG_TOOL                   "/usr/bin/iwconfig"
#define HOSTAPD_TOOL                    "/usr/bin/hostapd"
#define HOSTAPD_CONF_FILE2              "/usr/bin/rtl_hostapd_2.conf"
#ifdef EN_ZINK
#define HOSTAPD_CONF_FILE               "/usr/bin/rtl_hostapd.conf"
#define UDHCPD_TOOL                     "/sbin/udhcpd"
#define HTTPD_TOOL                      "/sbin/httpd"
#define DHCPD_CONF_FILE                 "/usr/bin/dhcp1.conf"
#define HTTP_DIR                        "/usr/bin/www"
#define HTTP_TIMEOUT_DIR                "/usr/bin/timeout"
#endif
#endif

#define SERVPORT						4444	/* server port number */
#define SAP_BACKLOG						100		/* max client number of connecting */

enum _WIFI_LED_STATUS
{
	WIFI_LED_B_ON = 1,
	WIFI_LED_G_FLASH,
	WIFI_LED_B_FLASH
};

enum WifiStep
{
	WIFI_STEP_IDLE = 0,
	WIFI_STEP_SCAN,
	WIFI_STEP_GET_FROM_FILE,
	WIFI_STEP_GET_FROM_AP,
	WIFI_STEP_GET_FROM_INTERFACE,
	WIFI_STEP_CONNECTING,
	WIFI_STEP_CONNECT_AP_OK,
	WIFI_STEP_APMODE,
	WIFI_STEP_DHCP_IP,
	WIFI_STEP_SUCCESS
};

typedef struct ConnectInfo {
	int 	fd;
	char	*msg;
}CONNECT_INFO;

int set_led_status(int status);

#endif /* __WIFI_H__*/
