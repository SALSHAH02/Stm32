/**
 ******************************************************************************
 * @file    Wifi/WiFi_HTTP_Server/src/main.c
 * @author  MCD Application Team
 * @brief   This file provides main program functions
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2017 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include<string.h>
#include<stdio.h>
#include<stdbool.h>

#ifdef __ICCARM__
#include <LowLevelIOInterface.h>
#endif

/* Private defines -----------------------------------------------------------*/
#define PORT           80

#define TERMINAL_USE

#define WIFI_WRITE_TIMEOUT 10000
#define WIFI_READ_TIMEOUT  10000
#define SOCKET                 0

#ifdef  TERMINAL_USE
#define LOG(a) printf a
#else
#define LOG(a)
#endif

#define SSID_SIZE     100
#define PASSWORD_SIZE 100
#define USER_CONF_MAGIC                 0x0123456789ABCDEFuLL

/* Private typedef------------------------------------------------------------*/

typedef struct {
	char ssid[SSID_SIZE];
	char password[PASSWORD_SIZE];
	uint8_t security;
} wifi_config_t;

typedef struct {
	uint64_t wifi_config_magic; /**< The USER_CONF_MAGIC magic word signals that the wifi config
	 (wifi_config_t) is present in Flash. */
	wifi_config_t wifi_config;
} user_config_t;

typedef enum {
	one_press = 0, double_press, hold
} button_type;

typedef struct {
	bool timer_enable;
	bool button_flag1;
	uint8_t status :3;
	uint32_t buttonTimer;
	uint32_t debounce_time;

} check;

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
#if defined (TERMINAL_USE)
extern UART_HandleTypeDef hDiscoUart;
#endif /* TERMINAL_USE */

/* configuration storage in Flash memory */
#if defined(__ICCARM__)
/* IAR */
extern void __ICFEDIT_region_FIXED_LOC_start__;
const  user_config_t    *lUserConfigPtr = &__ICFEDIT_region_FIXED_LOC_start__;
#elif defined(__CC_ARM)
/* Keil / armcc */
user_config_t __uninited_region_start__ __attribute__((section("UNINIT_FIXED_LOC"), zero_init));
const  user_config_t    *lUserConfigPtr = &__uninited_region_start__;
#elif defined(__GNUC__)
/* GNU compiler */
user_config_t __uninited_region_start__ __attribute__((section("UNINIT_FIXED_LOC")));
const user_config_t *lUserConfigPtr = &__uninited_region_start__;
#endif

static volatile uint8_t button_flag = 0;
static user_config_t user_config;

static uint8_t http[1024];
static uint8_t IP_Addr[4];
static int LedState = 0;

/* Private function prototypes -----------------------------------------------*/
#if defined (TERMINAL_USE)
#ifdef __GNUC__
/* With GCC, small printf (option LD Linker->Libraries->Small printf
 set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#define GETCHAR_PROTOTYPE int __io_getchar(void)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#define GETCHAR_PROTOTYPE int fgetc(FILE *f)
#endif /* __GNUC__ */
#endif /* TERMINAL_USE */

static void SystemClock_Config(void);
static WIFI_Status_t SendWebPage(uint8_t ledIsOn, uint8_t temperature);
static int wifi_server(void);
static int wifi_start(void);
static int wifi_connect(void);
static bool WebServerProcess(void);
static void Button_ISR(void);
static void Button_Reset(void);
static uint8_t Button_WaitForPush(uint32_t delay);

/* Private functions ---------------------------------------------------------*/
/**
 * @brief  Main program
 * @param  None
 * @retval None
 */

uint8_t button_hold(uint32_t delay);
bool time_diff(uint32_t init_time, uint32_t delay_ms);
unsigned char buff[300];
volatile uint8_t buff_1[300];
uint8_t com_up[] = "\033[10A";
uint8_t com_up1[] = "\033[1A";
uint8_t com_dn[] = "\033[B";
uint8_t msg1[] = "\033\143";
uint8_t com_up2[] = "\033[10A";
int8_t counter = 0;
int8_t counter1 = 0;
WIFI_APs_t A;
int rk = 0;
#define f_test 00
int max = 0;
unsigned char ssid_num[2];
int flag = 0;
uint8_t tmp[100];

check button;

int main(void) {
	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the system clock */
	SystemClock_Config();

	/* Configure LED2 */
	BSP_LED_Init(LED2);

	/* USER push button is used to ask if reconfiguration is needed */
	BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

	/* WIFI Web Server demonstration */
#if defined (TERMINAL_USE)
	/* Initialize all configured peripherals */
	hDiscoUart.Instance = DISCOVERY_COM1;
	hDiscoUart.Init.BaudRate = 115200;
	hDiscoUart.Init.WordLength = UART_WORDLENGTH_8B;
	hDiscoUart.Init.StopBits = UART_STOPBITS_1;
	hDiscoUart.Init.Parity = UART_PARITY_NONE;
	hDiscoUart.Init.Mode = UART_MODE_TX_RX;
	hDiscoUart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	hDiscoUart.Init.OverSampling = UART_OVERSAMPLING_16;
	hDiscoUart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	hDiscoUart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

	BSP_COM_Init(COM1, &hDiscoUart);
	BSP_TSENSOR_Init();

#endif /* TERMINAL_USE */

	wifi_server();

}

/**
 * @brief  Send HTML page
 * @param  None
 * @retval None
 */

static int wifi_start(void) {
	uint8_t MAC_Addr[6];

	/*Initialize and use WIFI module */
	if (WIFI_Init() == WIFI_STATUS_OK) {
		//printf("eS-WiFi Initialized.\n");
		if (WIFI_GetMAC_Address(MAC_Addr) == WIFI_STATUS_OK) {
			//LOG(
			//("eS-WiFi module MAC Address : %02X:%02X:%02X:%02X:%02X:%02X\n", MAC_Addr[0], MAC_Addr[1], MAC_Addr[2], MAC_Addr[3], MAC_Addr[4], MAC_Addr[5]));
		} else {
			//LOG(("> ERROR : CANNOT get MAC address\n"));
			return -1;
		}
	} else {
		return -1;
	}
	return 0;
}

int wifi_connect(void) {
	wifi_start();

	max = WIFI_ListAccessPoints(&A, 20);
	int sm = 0, j = 0, t = 0;
	char t1[100];

	for (int i = 0; i < A.count; i++) {
		sm = i;
		for (j = i + 1; j < A.count; j++) {
			if (A.ap[j].RSSI > A.ap[sm].RSSI) {
				sm = j;
			}
		}

		t = A.ap[i].RSSI;

		A.ap[i].RSSI = A.ap[sm].RSSI;
		A.ap[sm].RSSI = t;

		strcpy(t1, A.ap[i].SSID);
		strcpy(A.ap[i].SSID, A.ap[sm].SSID);
		strcpy(A.ap[sm].SSID, t1);

	}

	for (int i = 0; i < A.count; i++) {
		LOG((" %d.%s", i, A.ap[i].SSID));
		LOG(("(%d)\n", A.ap[i].RSSI));
	}
	HAL_UART_Transmit(&hDiscoUart, com_up2, strlen(com_up2), 10);

	memset(&user_config, 0, sizeof(user_config));
	memcpy(&user_config, lUserConfigPtr, sizeof(user_config));

	while (1) {
		while (1) {
			if (button.button_flag1) {
				button.button_flag1 = RESET;
				if (button.timer_enable != SET) {
					button.timer_enable = SET;
					button.buttonTimer = HAL_GetTick();
					button.status = one_press;
				} else {
					button.status = double_press;
				}
			}
			if (time_diff(button.buttonTimer, 200) && button.timer_enable) {
				if ((button.status == one_press)
						&& (button.status != double_press)) {
					//  flag = 0;

					if ((counter1>=A.count-1) || flag ==1) {
						counter1 = 0;
						HAL_UART_Transmit(&hDiscoUart, com_up, strlen(com_up),10);
						flag = 0;
					} else {
						counter1++;
						HAL_UART_Transmit(&hDiscoUart, com_dn, strlen(com_dn),50);
					}

					button.timer_enable = RESET;
				}

				if (button.status == double_press) {
					counter1 = counter1 - 1;
					HAL_UART_Transmit(&hDiscoUart, com_up1, strlen(com_up1),50);
				}

				button.timer_enable = RESET;

			}

			if (!(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13)))
				button_hold(1500);



		}


	}
	return 0;

}

int wifi_server(void) {
	bool StopServer = false;

	//LOG(("\nRunning HTML Server test\n"));
	if (wifi_connect() != 0)
		return -1;

	if (WIFI_STATUS_OK
			!= WIFI_StartServer(SOCKET, WIFI_TCP_PROTOCOL, 1, "", PORT)) {
		LOG(("ERROR: Cannot start server.\n"));
	}

	//LOG(
	//	("Server is running and waiting for an HTTP  Client connection to %d.%d.%d.%d\n",IP_Addr[0],IP_Addr[1],IP_Addr[2],IP_Addr[3]));

	do {
		uint8_t RemoteIP[4];
		uint16_t RemotePort;

		//LOG(
		//	("Waiting connection to http://%d.%d.%d.%d\n",IP_Addr[0],IP_Addr[1],IP_Addr[2],IP_Addr[3]));
		while (WIFI_STATUS_OK
				!= WIFI_WaitServerConnection(SOCKET, 1000, RemoteIP,
						&RemotePort)) {
			LOG(("."));
		}

		//LOG(
		//("\nClient connected %d.%d.%d.%d:%d\n",RemoteIP[0],RemoteIP[1],RemoteIP[2],RemoteIP[3],RemotePort));

		StopServer = WebServerProcess();

		if (WIFI_CloseServerConnection(SOCKET) != WIFI_STATUS_OK) {
			LOG(("ERROR: failed to close current Server connection\n"));
			return -1;
		}
	} while (StopServer == false);

	if (WIFI_STATUS_OK != WIFI_StopServer(SOCKET)) {
		LOG(("ERROR: Cannot stop server.\n"));
	}

	LOG(("Server is stop\n"));
	return 0;
}

static bool WebServerProcess(void) {
	uint8_t temp;
	uint16_t respLen;
	static uint8_t resp[1024];
	bool stopserver = false;

	if (WIFI_STATUS_OK == WIFI_ReceiveData(SOCKET, resp, 1000, &respLen,
	WIFI_READ_TIMEOUT)) {
		LOG(("get %d byte from server\n",respLen));

		if (respLen > 0) {
			if (strstr((char*) resp, "GET")) /* GET: put web page */
			{
				temp = (int) BSP_TSENSOR_ReadTemp();
				if (SendWebPage(LedState, temp) != WIFI_STATUS_OK) {
					LOG(("> ERROR : Cannot send web page\n"));
				} else {
					LOG(("Send page after  GET command\n"));
				}
			} else if (strstr((char*) resp, "POST"))/* POST: received info */
			{
				LOG(("Post request\n"));

				if (strstr((char*) resp, "radio")) {
					if (strstr((char*) resp, "radio=0")) {
						LedState = 0;
						BSP_LED_Off(LED2);
					} else if (strstr((char*) resp, "radio=1")) {
						LedState = 1;
						BSP_LED_On(LED2);
					}
					temp = (int) BSP_TSENSOR_ReadTemp();
				}
				if (strstr((char*) resp, "stop_server")) {
					if (strstr((char*) resp, "stop_server=0")) {
						stopserver = false;
					} else if (strstr((char*) resp, "stop_server=1")) {
						stopserver = true;
					}
				}
				temp = (int) BSP_TSENSOR_ReadTemp();
				if (SendWebPage(LedState, temp) != WIFI_STATUS_OK) {
					LOG(("> ERROR : Cannot send web page\n"));
				} else {
					LOG(("Send Page after POST command\n"));
				}
			}
		}
	} else {
		LOG(("Client close connection\n"));
	}
	return stopserver;

}

/**
 * @brief  Send HTML page
 * @param  None
 * @retval None
 */
static WIFI_Status_t SendWebPage(uint8_t ledIsOn, uint8_t temperature) {
	uint8_t temp[50];
	uint16_t SentDataLength;
	WIFI_Status_t ret;

	/* construct web page content */
	strcpy((char*) http,
			(char*) "HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\n\r\n");
	strcat((char*) http, (char*) "<html>\r\n<body>\r\n");
	strcat((char*) http, (char*) "<title>STM32 Web Server</title>\r\n");
	strcat((char*) http,
			(char*) "<h2>STM32L4S Discovery kit IoT node : Web server using WiFi with STM32</h2>\r\n");
	strcat((char*) http, (char*) "<br /><hr>\r\n");
	strcat((char*) http,
			(char*) "<p><form method=\"POST\"><strong>Temp: <input type=\"text\" value=\"");
	sprintf((char*) temp, "%d", temperature);
	strcat((char*) http, (char*) temp);
	strcat((char*) http, (char*) "\"> <sup>O</sup>C");

	if (ledIsOn) {
		strcat((char*) http,
				(char*) "<p><input type=\"radio\" name=\"radio\" value=\"0\" >LED off");
		strcat((char*) http,
				(char*) "<br><input type=\"radio\" name=\"radio\" value=\"1\" checked>LED on");
	} else {
		strcat((char*) http,
				(char*) "<p><input type=\"radio\" name=\"radio\" value=\"0\" checked>LED off");
		strcat((char*) http,
				(char*) "<br><input type=\"radio\" name=\"radio\" value=\"1\" >LED on");
	}

	strcat((char*) http,
			(char*) "</strong><p><input type=\"submit\"></form></span>");
	strcat((char*) http, (char*) "</body>\r\n</html>\r\n");

	ret = WIFI_SendData(0, (uint8_t*) http, strlen((char*) http),
			&SentDataLength, WIFI_WRITE_TIMEOUT);

	if ((ret == WIFI_STATUS_OK) && (SentDataLength != strlen((char*) http))) {
		ret = WIFI_STATUS_ERROR;
	}

	return ret;
}

/**
 * @brief  System Clock Configuration
 *         The system Clock is configured as follow :
 *            System Clock source            = PLL (MSI)
 *            SYSCLK(Hz)                     = 80000000
 *            HCLK(Hz)                       = 80000000
 *            AHB Prescaler                  = 1
 *            APB1 Prescaler                 = 1
 *            APB2 Prescaler                 = 1
 *            MSI Frequency(Hz)              = 4000000
 *            PLL_M                          = 1
 *            PLL_N                          = 40
 *            PLL_R                          = 2
 *            PLL_P                          = 7
 *            PLL_Q                          = 4
 *            Flash Latency(WS)              = 4
 * @param  None
 * @retval None
 */
static void SystemClock_Config(void) {
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_OscInitTypeDef RCC_OscInitStruct;

	/* MSI is enabled after System reset, activate PLL with MSI as source */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
	RCC_OscInitStruct.MSIState = RCC_MSI_ON;
	RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
	RCC_OscInitStruct.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
	RCC_OscInitStruct.PLL.PLLM = 1;
	RCC_OscInitStruct.PLL.PLLN = 40;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLP = 7;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		/* Initialization Error */
		while (1)
			;
	}

	/* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
	 clocks dividers */
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
		/* Initialization Error */
		while (1)
			;
	}
}

/**
 * @brief Reset button state
 *        To be called before Button_WaitForPush()
 */
void Button_Reset() {
	button_flag = 0;
}

/**
 * @brief Waiting for button to be pushed
 */
uint8_t Button_WaitForPush(uint32_t delay) {
	uint32_t time_out = HAL_GetTick() + delay;

	do {
		if (button_flag > 0) {
			return button_flag;
		}
		HAL_Delay(100);
	} while (HAL_GetTick() < time_out);

	return 0;
}

#if defined (TERMINAL_USE)
/**
 * @brief  Retargets the C library printf function to the USART.
 * @param  None
 * @retval None
 */
PUTCHAR_PROTOTYPE {
	/* Place your implementation of fputc here */
	/* e.g. write a character to the USART1 and Loop until the end of transmission */
	HAL_UART_Transmit(&hDiscoUart, (uint8_t*) &ch, 1, 0xFFFF);

	return ch;
}

#ifdef __ICCARM__
/**
 * @brief
 * @param
 * @retval
 */
size_t __read(int handle, unsigned char * buffer, size_t size)
{
	int nChars = 0;

	/* handle ? */

	for (/* Empty */; size > 0; --size)
	{
		uint8_t ch = 0;
		while (HAL_OK != HAL_UART_Receive(&hDiscoUart, (uint8_t *)&ch, 1, 30000))
		{
			;
		}

		*buffer++ = ch;
		++nChars;
	}

	return nChars;
}
#elif defined(__CC_ARM) || defined(__GNUC__)
/**
 * @brief  Retargets the C library scanf function to the USART.
 * @param  None
 * @retval None
 */
GETCHAR_PROTOTYPE {
	/* Place your implementation of fgetc here */
	/* e.g. read a character on USART and loop until the end of read */
	uint8_t ch = 0;
	while (HAL_OK != HAL_UART_Receive(&hDiscoUart, (uint8_t*) &ch, 1, 30000)) {
		;
	}
	return ch;
}
#endif /* defined(__CC_ARM)  */
#endif /* TERMINAL_USE */

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
 * @brief  EXTI line detection callback.
 * @param  GPIO_Pin: Specifies the port pin connected to corresponding EXTI line.
 * @retval None
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == GPIO_PIN_1) {
		SPI_WIFI_ISR();
	}

	if (GPIO_Pin == GPIO_PIN_13 && (HAL_GetTick() - button.debounce_time > 15)) {
		button.button_flag1 = SET;
		button.debounce_time = HAL_GetTick();
	}
	else {
		__NOP();
	}

}

uint8_t button_hold(uint32_t delay) {

	char buff1[100];
	uint32_t time_out;
	time_out = HAL_GetTick();
	static int temp = -1, k;
	while (!(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13))) {
		if ((HAL_GetTick() - time_out) > delay) {

			flag = 1;

			HAL_UART_Transmit(&hDiscoUart, msg1, strlen(msg1), 500);
			HAL_Delay(100);
			HAL_UART_Transmit(&hDiscoUart, com_up, strlen(com_up), 100);
			switch (counter1) {

			case 0:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[0].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			case 1:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[1].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			case 2:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[2].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			case 3:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[3].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			case 4:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[4].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			case 5:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[5].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			case 6:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[6].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			case 7:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[7].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			case 8:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[8].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			case 9:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[9].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			case 10:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[10].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			case 11:

				sprintf(user_config.wifi_config.ssid, "%s", A.ap[11].SSID);
				LOG(
						("\nYou have entered %s as SSID.\n", user_config.wifi_config.ssid));
				break;

			default:
				LOG(("\nInvalid Choice\n"));
				break;

			}
			if (temp != counter1) {

					char c;
					do {
						printf(
								"\nEnter Security Mode (0 - Open, 1 - WEP, 2 - WPA, 3 - WPA2): ");
						c = getchar();
					} while ((c < '0') || (c > '3'));
					user_config.wifi_config.security = c - '0';
					LOG(
							("\nYou have entered %d as the security mode.\n", user_config.wifi_config.security));

					if (user_config.wifi_config.security != 0) {
						printf("\nEnter WiFi password : ");
						gets(user_config.wifi_config.password);
					}

				printf("\nConnecting to %s\n", user_config.wifi_config.ssid);
				WIFI_Ecn_t security;
				switch (user_config.wifi_config.security) {
				case 0:
					security = WIFI_ECN_OPEN;
					break;
				case 1:
					security = WIFI_ECN_WEP;
					break;
				case 2:
					security = WIFI_ECN_WPA_PSK;
					break;
				case 3:
				default:
					security = WIFI_ECN_WPA2_PSK;
					break;
				}
				if (WIFI_Connect(user_config.wifi_config.ssid,
						user_config.wifi_config.password, security)
						== WIFI_STATUS_OK) {
					if (WIFI_GetIP_Address(IP_Addr) == WIFI_STATUS_OK) {
						LOG(
								("eS-WiFi module connected:\ngot IP Address : %d.%d.%d.%d\n", IP_Addr[0], IP_Addr[1], IP_Addr[2], IP_Addr[3]));
						HAL_UART_Transmit(&hDiscoUart, msg1, strlen(msg1), 500);
						HAL_Delay(700);
						for (int i = 0; i < A.count; i++) {
							if ((i) != counter1)

							{
								LOG((" %d.%s", i, A.ap[i].SSID));
								LOG((" (%d)\n", A.ap[i].RSSI));
							}

							else {
								LOG((" %d.* %s", i, A.ap[i].SSID));
								LOG((" (%d)\n", A.ap[i].RSSI));
							}

						}
						temp = counter1;
						HAL_UART_Transmit(&hDiscoUart, com_up, strlen(com_up),500);

					} else {
						LOG(
								(" ERROR : es-wifi module CANNOT get IP address\n"));
						return -1;
					}
				} else {
					LOG(("ERROR : es-wifi module NOT connected\n"));
					already_selected: counter1 = 0;
					HAL_UART_Transmit(&hDiscoUart, msg1, sizeof(msg1), 1000);
					HAL_Delay(1000);
					for (int i = 0; i < A.count; i++) {

						LOG((" %d.%s", i, A.ap[i].SSID));
						LOG((" (%d)\n", A.ap[i].RSSI));

					}
					HAL_UART_Transmit(&hDiscoUart, com_up, strlen(com_up),1000);
					return -1;
				}

			} else {

				LOG(("You are already connected to this network\n"));
				HAL_Delay(600);
				goto already_selected;

			}

			button.timer_enable = 0;
			break;
		}

	}

	return 0;
}

bool time_diff(uint32_t init_time, uint32_t delay_ms) {
	bool ret = 0;
	ret = (HAL_GetTick() - (init_time) > delay_ms ? 1 : 0);
	return ret;
}

/**
 * @brief  SPI3 line detection callback.
 * @param  None
 * @retval None
 */
void SPI3_IRQHandler(void) {
	HAL_SPI_IRQHandler(&hspi);
}

/**
 * @brief Update button ISR status
 */
static void Button_ISR(void) {
	button_flag++;
}

