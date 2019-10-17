#include "wifi.h"

/*****************************************************
 * sdk led control api
 * user should finshed this API
 *
 * **************************************************/
extern void WifiSetLedStatus(int status);

/*******************************************************
 * WIFI moudle will use this API to control LED status
 * input arg:
 * status:
 *        1(WIFI_LED_B_ON):	         Turn Blue LED On
 *        2(WIFI_LED_G_FLASH):       Turn Greeb LED Flash
 *        3(WIFI_LED_B_FLASH):       Turn Blue LED Flash
 *
 * *****************************************************/
int set_led_status(int status)
{
	WifiSetLedStatus(status);
	return 0;
}
