#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdbool.h>

#define SIMULATE_MEGA_ON_PC 0

#if SIMULATE_MEGA_ON_PC 
    #define PC_LOG_ENABLE 1
#else
    #define PC_LOG_ENABLE 1
#endif

#define PC_BAUDRATE (115200U) // velocità della porta seriale verso il pc
#define MEGA_BAUDRATE (115200U) // velocità sulla uart1
#define TX_PERIOD_MS (1000U) //ogni quanti ms invio "Hello" ad arduino
#define LINE_BUF_SIZE (64U) // dimensione del buffer che accumula i caratteri fino a \n

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart1;

static UART_HandleTypeDef *hlink = NULL;

static void SystemClock_Config(void); //imposta le sorgenti di clock che utilizzerò dopo
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void Error_Handler(void); // si mette per determianre un comportamento in caso di errori 

void HAL_MspInit(void); // inizializzo i clock dei blocchi di sistema ecc...
void HAL_UART_MspInit(UART_HandleTypeDef *huart); // abilita il clock della uart e configura i pin in alternate function

static const char kHelloA[] = "Ciao cuci'\r\n"; // stacic perchè cos' la visibilità è limitata a questo file (linkage interno), evito collisioni con i nomi delgi altri file.
static const char kAckB[] = "ACK\r\n";

// utility per il log 

static inline void PC_Log(const char *s){ // inline serve per dire al compilatore: invece di fare la solita chiamata di funzione, copia direttamente il corpo della funzione qui dove la sto chiamando. Quindi non salta in un'altra zona di codice e poi torna indietro, incolla il codice lì sul posto.
    #if PC_LOG_ENABLE
        HAL_UART_Transmit(&huart2, (uint8_t*)s, (uint16_t)strlen(s),HAL_MAX_DELAY); 
    #else
        (void)s; // in pratica faccio un cast a void, quando non utlizzo s, se non lo facessi, in caso di non utilizzo mi darebbe un errore del tipo: parametro non usato
    #endif
}

//cosa fare quando arriva una riga valida ad arduino

static inline void Processing_FromMega(const char *line){ //questa funzione mi serve a vedere cosa è arrivato ad arduino attraveso il monitor seriale 
    //prima invio il log al pc di cosa è arrivato
    PC_Log("Arduino -> STM32: ");
    PC_Log(line);
    PC_Log("\r\n");

    if(strcmp(line, "ACK_B") == 0){
        return;
    }

    //risposta: ACK_B verso arduino
    HAL_UART_Transmit(&huart2, (uint8_t*)kAckB, sizeof(kAckB) - 1U, 1000); // nella size scrivo -1U perchè perchè kAckB è un array che contiene anche il carattere '\0'. 
    PC_Log("STM32 -> Arduino: ACK_B");

}

int main(void){
    HAL_Init(); //inizializzo lo stato interno della HAL, è come se stessi accendendo i servizi base
    SystemClock_Config(); // applico la configurazione di clock che ho deciso nel fine .h

    MX_USART2_UART_Init();  // inizializzo le uart    
    
    #if SIMULATE_MEGA_ON_PC
        hlink = &huart2;
    #else
        MX_USART1_UART_Init();
        hlink = &huart1;
    #endif

    PC_Log("stm32 pronta\r\n -USART2 115200 = DEBUG (Virtual COM)\r\n -USART1 115200 = LINK verso Arduino (PA9 = TX, PA10 = RX)\r\n - Inviero' la parola HELLO_A ogni 1s su USART1 e rispondero' ACK_B quando ricevero' la riga.\r\n"); 
   

    char line1[LINE_BUF_SIZE]; // qui accumulo i caratteri ricevuti da arduino fino al newline (\n)
    size_t idx1 = 0U; // è l'indice di scrittura dentro line1

    uint32_t last_tx = HAL_GetTick(); // mi restituisce un contatore in ms da quando è partita la HAL
    //uint32_t last_tx = 0;

    for(;;){
        uint32_t now = HAL_GetTick();

        if((now - last_tx) >= TX_PERIOD_MS){ //last_tx rappresenta l'ultima volta in cui abbiamo inviato "HELLO_A", quindi la differenza con now mi da quanti ms sto aspettando, quando raggiunge TX_PERIOD_MS è ora di inviare
            HAL_UART_Transmit(&huart2, (uint8_t*)kHelloA, sizeof(kHelloA) -1U, 1000);  
            PC_Log("STM32 -> Arduino: HELLO_A\r\n");
            last_tx = now; //aggiorno il momento di ultimo invio per il prossimo ciclo 
        }

        

        // STATE MACHINE 

        uint8_t c; //definisco un contenitore temporaneo per memorizzare un carattere arrivato dalla uart

        if(HAL_UART_Receive(hlink, &c, 1U, 0U) == HAL_OK){ //qui è come se chiedessi: hai 1 byte pronto in RX su USART!? DAmmi subito risposta, se è arrivaot 1 byte ritorna HAL_OK e lo mette in c 
            if(c == '\r'){
                //ignoro '\r', così evito di generare due fine riga(uno sul \r e uno sul \n)
            }
            else if(c == '\n'){
                line1[idx1] = '\0'; // in pratica \n rappresenta il delimitatore del messaggio, quindi con questa riga chiudo la stringa
                Processing_FromMega(line1); // qui invio la stringa alla funzione che si occupa di processarla 
                idx1 = 0U; // qui resetto il buffer per la prossima riga. Se non la resetto i caratteri della prossima riga potrebbero accodarsi alla precedente
            }
            
            else{ // questa è la parte dove gestisco il caso in cui arriva un carattere che non sia ne '\r' ne '\n'
                if(idx1 < (LINE_BUF_SIZE - 1U)) // riservo sempre un posto per il terminatore '\0'
                {
                    line1[idx1++] = (char)c; //Qui copio il byte ricevuto dentro al buffer e incrmento l'indice, il cast (char) serve perchè c è un uint8_t, mentre il buffer è un array di char
                }
            }
        }
    }
}


// inizializzo la USART2

static void MX_USART2_UART_Init(void){
    huart2.Instance = USART2;
    huart2.Init.BaudRate = PC_BAUDRATE;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity =UART_PARITY_NONE;
    huart2.Init.Mode =  UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16; // 16 campioni per ogni singolo bit

    if(HAL_UART_Init(&huart2) != HAL_OK){
        Error_Handler();
    } 
}

static void MX_USART1_UART_Init(void){
    huart1.Instance = USART1;
    huart1.Init.BaudRate = MEGA_BAUDRATE;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity =UART_PARITY_NONE;
    huart1.Init.Mode =  UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16; // 16 campioni per ogni singolo bit

    if(HAL_UART_Init(&huart1) != HAL_OK){
        Error_Handler();
    } 
}

// imposto l'albero del clock

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /* Oscillatore: HSI ON, nessun PLL → SYSCLK = 16 MHz */
  RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_OFF;

  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  

  /* Bus: AHB=SYSCLK, APB1=AHB, APB2=AHB → tutti a 16 MHz */
  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  |
                                     RCC_CLOCKTYPE_SYSCLK|
                                     RCC_CLOCKTYPE_PCLK1 |
                                     RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

// abilito i clock

void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
}

// configuro i clock e i pin per le uart prima che la HAL scriva i registri 
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (huart->Instance == USART2)
  {
    /* USART2 → PA2=TX, PA3=RX (AF7) → Debug PC */
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin       = GPIO_PIN_2 | GPIO_PIN_3;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;                /* idle UART = alto */
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
  else if (huart->Instance == USART1)
  {
    /* USART1 → PA9=TX, PA10=RX (AF7) → Link Mega */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin       = GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  }
}

void SysTick_Handler(void) {
  HAL_IncTick();
}


static void Error_Handler(void){
    __disable_irq();
    while(1){

    }
}