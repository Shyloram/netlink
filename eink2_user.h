/*************************************************************************
	> eink2 moudle user interface list
	> user just only include this eink2_user.h file
	> to use eink2 moudle
 ************************************************************************/

/*************************************
 * eink moudle init API
 * return:
 *      0    successed
 *     -1    failed
 * ************************************/
int InitWifi(void);

/*************************************
 * get wifi lib version
 * return:
 *  version  successed
 *     -1    failed
 * ************************************/
void GetWifiVersion(void);

/**************************************
 * get eink moudle connection status 
 * return:
 *      0    disconnect
 *      1    connected
 * ************************************/
int GetConnectStatus(void);

/**************************************
 * set wifi ssid and password then connecting 
 * return:
 *      0    successed
 *      -1   failed
 * ************************************/
int SetWifiConfig(char* ssid,char* password);
