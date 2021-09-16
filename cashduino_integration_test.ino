/*
 * Description: Integration code to use cashduino shield
 * Author: Jesus Palomo (jgualberto.palomo@gmail.com)
 * Copyrigth: 2021 palomothings 
 * 
 * This file is part of palomothings site
 * 
 * CashDuino integration is a free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License GPLV2 as published by 
 * the Free Software Fundation.
 * 
 * CashDuino Integracion is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without event the implied warranty of
 * MERCHANTABILITY of FITNEDD FOR A PARTICULAR PURPOSE. See the 
 * GNU General Public License for more details. http://www.gnu.org/license/
 *  
 */
 
 /* 
 * LCD CONNECTION:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 5
 * LCD D5 pin to digital pin 4
 * LCD D6 pin to digital pin 3
 * LCD D7 pin to digital pin 2
 * LCD R/W pin to ground
 */

#include <Wire.h>
#include <LiquidCrystal.h>
#include <SD.h>
#include <avr/io.h>
#include <string.h>
#include <stdlib.h>

/* MACRO DEFINITIONS */
#define MAX_BUFF_CASHDUINO    16
#define MAX_BUFF_ARDUINO      14
#define CASH_DUINO_ADDR       8
#define KEY_1                 0x33
#define KEY_2                 0xCC
#define NONE                  0xFF
#define MAX_BUFF_QR_MSG       48
#define MAX_NAME_PRODUCT      20
#define MAX_PRICE_STR         20
#define MAX_BUFFER_LOG        128
#define MAX_TUBES_COIN        5

/* PIN definition for user options buttons */
#define PIN_CONFIRM           22
#define PIN_CLEAR             23
#define PIN_ADD               24

/* PIN microSD definition */
#define PIN_CS_SD             53

/* PIN Board Relay */
#define PIN_RELAY             25
#define RELAY_ON  digitalWrite(PIN_RELAY, HIGH)
#define RELAY_OFF  digitalWrite(PIN_RELAY, LOW)

/* PIN arduino mega board led */
#define PIN_LED               13

/* initialize the library with the numbers of the interface pins */
LiquidCrystal lcd(12, 11, 5, 4, 3, 2);

#define lcd_printf(x,y,data)  lcd.setCursor(x, y); \
                              lcd.print(data);
                              
#define lcd_printf_items(items) lcd.setCursor(0,0); \
                                lcd.print("( )"); \
                                lcd.setCursor(1,0); \
                                lcd.print(items);                              
                              
#define lcd_printf_total(total) lcd.setCursor(0,0); \
                                lcd.print("(0)TOTAL: $0.00     "); \
                                lcd.setCursor(11,0); \
                                lcd.print(total);
                                
#define lcd_printf_credit(credit) lcd.setCursor(0,1); \
                                lcd.print("  CREDIT: $0.00     "); \
                                lcd.setCursor(11,1); \
                                lcd.print(credit);

#define lcd_printf_price(price) lcd.setCursor(0,2); \
                                lcd.print("   PRICE: $0.00     "); \
                                lcd.setCursor(11,2); \
                                lcd.print(price);                                
                                 
#define lcd_printf_name(name)   lcd.setCursor(0,3); \
                                lcd.print("                   "); \
                                lcd.setCursor(0,3); \
                                lcd.print( ( char * ) name );


/* user options */
typedef enum{
  BTN_NONE = 0,
  BTN_CONFIRM,
  BTN_CLEAR,
  BTN_ADDING
}button;

/* options to the user, the user need to type in the Serial Monitor 1 to 9 */
typedef enum{
  TASK_ENQUIRY_BOARD = 0,
  TASK_COIN_ENABLE,
  TASK_COIN_DISABLE,
  TASK_COIN_AUDIT,
  TASK_COIN_OUT,
  INVALID_OPTION
}task_cashduino;

/* arduino tasks options */
typedef enum{
  ARDUINO_CMD_ENQUIRY       = 0xC1,
  ARDUINO_CMD_COIN_ENABLE   = 0xA1,
  ARDUINO_CMD_COIN_AUDIT    = 0xA2,
  ARDUINO_CMD_COIN_OUT      = 0xA3
}cmd_arduino;

/* cashduino cmd answers */
typedef enum{
  CASHDUINO_CMD_ENQUIRY     = 0xD1,
  CASHDUINO_CMD_COIN_ENABLE = 0xF1,
  CASHDUINO_CMD_COIN_AUDIT  = 0xF2,
  CASHDUINO_CMD_COIN_OUT    = 0xF3,
  CASHDUINO_CMD_COIN_EVENT  = 0xF4
}cmd_cashduino;

/* data structure to drive operation */
typedef struct{
  uint8_t qr_data  [MAX_BUFF_QR_MSG];
  uint8_t name     [MAX_NAME_PRODUCT];
  uint8_t str_price[MAX_PRICE_STR];
  uint8_t arduino_task[MAX_BUFF_ARDUINO];
  uint8_t tube_values[MAX_TUBES_COIN];
  uint8_t coin_out[MAX_TUBES_COIN];
  char buffer_log[MAX_BUFFER_LOG];
  uint8_t qr_index;
  double  price;
  double  total;
  double  cash_in;
  double  cash_out;
  uint8_t total_item;
}store;

/* global variables */
uint8_t new_event  = NONE;
uint8_t command    = 0x00;
File logger;

/* prototype functions */
void scheduler_store(store *data);
void mapping_user_selection(store *data);
void qr_data_processing(store *data);
void send_task_to_cashduino(store *data, uint8_t task);
void cashout_process(store *data);
uint8_t push_coin_outs(store *data);
void data_logger_sd(store *data);



void setup() {
  
  /* setting lcd */
  lcd.begin(20, 4);
  lcd.clear();
  
  /* setting cashduino bus i2c */
  Wire.begin(); 
  
  /* enable bar QR reader port */
  Serial1.begin(115200);
  
  /* setting I/O */
  pinMode(PIN_CONFIRM, INPUT);
  pinMode(PIN_CLEAR, INPUT);
  pinMode(PIN_ADD, INPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_CS_SD, OUTPUT);
  
  /* enable serial debugger */
  Serial.begin(9600);
  
  /* SD initialization */
  if(!SD.begin(PIN_CS_SD))
  {
    Serial.println("No se puede iniciar la micro SD");  
  }
  else
  {
    Serial.println("micro SD inicializada");  
  }
}



void loop() {
  
  /* initialize local variables */
  uint8_t rx_cashduino[MAX_BUFF_CASHDUINO]  = {0};
  uint8_t cashduino_cmd                     = NONE;
  uint8_t polling                           = 0;
  uint8_t i                                 = 0;
  store data;
          
  /* initialize data members */
  memset(data.qr_data, 0, sizeof(data.qr_data));
  memset(data.name, 0, sizeof(data.name));
  memset(data.str_price, 0, sizeof(data.str_price));
  memset(data.arduino_task, 0, sizeof(data.arduino_task));
  memset(data.coin_out, 0, sizeof(data.coin_out));
  memset(data.buffer_log, 0, sizeof(data.buffer_log));
  data.qr_index = 0;
  data.price = 0;
  data.total = 0;
  data.cash_in = 0;
  data.cash_out = 0;
  data.total_item = 0;
    
  /* print home */
  lcd_printf(0,0,"(0)TOTAL: $0.00     ");
  lcd_printf(0,1,"  CREDIT: $0.00     ");
  lcd_printf(0,2,"   PRICE: $0.00     ");
  lcd_printf(0,3,"                    ");
  
  send_task_to_cashduino(&data, TASK_ENQUIRY_BOARD);
  
  /* main loop */
  while(true)
  {
    
    /* led to see the polling timing */
    polling ^= 1;
    digitalWrite(PIN_LED, polling ? HIGH : LOW);
    
    /* ###################### CASHDUINO ###################### */
    /* read cashduino buffer */
    read_buffer_from_cashduino(rx_cashduino);
    
    /* is a new event? */
    if ( looking_for_new_event(rx_cashduino) == true )
    {
      /* is a valid package? */
      if( is_a_valid_package(rx_cashduino) == true )
      {
        /* parsing buffer to find data */
        cashduino_cmd = parsing_cashduino_buffer(rx_cashduino, &data.cash_in);
        
        switch(cashduino_cmd)
        {
          case CASHDUINO_CMD_ENQUIRY:
            send_task_to_cashduino(&data, TASK_COIN_DISABLE);
            break;
          case CASHDUINO_CMD_COIN_EVENT:
            
            lcd_printf_credit(data.cash_in);
            
            /* log product and price */
            sprintf(data.buffer_log, "Total Inserted %d",(int) data.cash_in);
            data_logger_sd(&data);
            
            /* Turn on Relay regarding total_item */
            for(i = 0; i < data.total_item; ++i)
            {
              RELAY_ON;
              delay(500);
              RELAY_OFF;
              delay(500);
            }
            break;
        }
      }
    }
    /* ###################### CASHDUINO ###################### */    
    
    /* processing bar code reader and buttons */
    scheduler_store(&data); 
   
  }
}

/* this function will puilling the process that the system needs */
void scheduler_store(store *data)
{  
  unsigned int i = 0;
   
  /* time to attend another process */
  for(i = 0; i < 5000; i ++)
  { 
    qr_data_processing(data);
    mapping_user_selection(data);
  }
  
}

/* ###################### SD CARD ###################### */
/* This function will write the events in the micro SD card  */
void data_logger_sd(store *data)
{ 
  logger = SD.open("LOG.TXT", FILE_WRITE);
  if(logger)
  {
    logger.print("[");
    logger.print((long int)millis());
    logger.print("]");
    logger.println(data->buffer_log);
    logger.close();
    /* clean buffer */
    memset(data->buffer_log, 0, sizeof(data->buffer_log));
  }
  else
  {
    Serial.println("No se pudo guardar el mensaje ... )= ");
  }
  
}
/* ###################### SD CARD ###################### */


/* ###################### QR CODE ###################### */
void qr_data_processing(store *data)
{

  if (Serial1.available())
  {
    data->qr_data[data->qr_index] = Serial1.read();
    Serial.write(data->qr_data[data->qr_index]); /* debug */
    data->qr_index = 0;
  }
  
}

/* ###################### QR CODE ###################### */




/* ###################### BUTTONS ###################### */
/* function to read the selection by user */
void mapping_user_selection(store *data)
{
  uint8_t selection = 0;

  selection = read_keys();
  
  switch (selection) {
  case BTN_CONFIRM:
    Serial.write('1'); /* debug */
    send_task_to_cashduino(data, TASK_COIN_ENABLE);
    break;
  case BTN_CLEAR:
    /* clear total and disable payment systems */
    send_task_to_cashduino(data, TASK_COIN_DISABLE);
    Serial.write('2'); /* debug */
    break;
  case BTN_ADDING:
    /* adding product price */
    Serial.write('3'); /* debug */
    data->total_item += 1;
    data->price = 1;
    data->total += data->price;
    lcd_printf_total(data->total);
    lcd_printf_items(data->total_item);
    break;
  case BTN_NONE:
    /* nothing to do */
    break;
  }  
}

/* function for read the falling button state */
uint8_t read_keys(void)
{
  
  static uint8_t btn_flag[3] = {0};
  
  if ( (digitalRead(PIN_CONFIRM) == LOW) && (btn_flag[0] == 0) )
  {
    btn_flag[0] = 1;
    delay(250);
    return BTN_CONFIRM;
  }
  else if ( (digitalRead(PIN_CONFIRM) == HIGH) && (btn_flag[0] == 1) )
  {
    btn_flag[0] = 0;
  }
  
  if ( (digitalRead(PIN_CLEAR) == LOW) && (btn_flag[1] == 0) )
  {
    btn_flag[1] = 1;
    delay(250);
    return BTN_CLEAR;
  }
  else if ( (digitalRead(PIN_CLEAR) == HIGH) && (btn_flag[1] == 1) )
  {
    btn_flag[1] = 0;
  }
  
  if ( (digitalRead(PIN_ADD) == LOW) && (btn_flag[2] == 0) )
  {
    btn_flag[2] = 1;
    delay(250);
    return BTN_ADDING;
  }
  else if ( (digitalRead(PIN_ADD) == HIGH) && (btn_flag[2] == 1) )
  {
    btn_flag[2] = 0;
  }
  
  return BTN_NONE;

}
/* ###################### BUTTONS ###################### */


/* ###################### CASHDUIO ###################### */

/* function to validate the data structure */
bool is_a_valid_package(uint8_t *rx_cashduino)
{
  if( (rx_cashduino[11] == KEY_1) && (rx_cashduino[12] == KEY_2) &&
      (rx_cashduino[13] == KEY_1) && (rx_cashduino[14] == KEY_2) &&
      (rx_cashduino[15] == KEY_1) )
  {
    return true;
  }
  return false;
}

/* function to parsing the cashduino buffer and get information */
uint8_t parsing_cashduino_buffer(uint8_t *rx_cashduino, double *amount)
{
  uint8_t ret_code = NONE;
  
  /* type of command */
  switch(rx_cashduino[0])
  {
    case CASHDUINO_CMD_ENQUIRY:
      ret_code = CASHDUINO_CMD_ENQUIRY;
      break;
    case CASHDUINO_CMD_COIN_ENABLE:
      ret_code = CASHDUINO_CMD_COIN_ENABLE;
      break;
    case CASHDUINO_CMD_COIN_AUDIT:
      ret_code = CASHDUINO_CMD_COIN_AUDIT;
      break;
    case CASHDUINO_CMD_COIN_OUT:
      ret_code=CASHDUINO_CMD_COIN_OUT;
      break;
     case CASHDUINO_CMD_COIN_EVENT:
      if( (rx_cashduino[1] >= 0x40) && (rx_cashduino[1] <= 0x4F))
      {
        ret_code = CASHDUINO_CMD_COIN_EVENT;
        switch (rx_cashduino[1]){
          case 0x40:
            *(amount) += 0.5;
            break;
          case 0x41:
            *(amount) += 0.5;
            break;
          case 0x42:
            *(amount) += 1;
            break;
          case 0x43:
            *(amount) += 2;
            break;
          case 0x44:
            *(amount) += 5;
            break;
          case 0x45:
            *(amount) += 10;
            break;
        }
      }
      if( (rx_cashduino[1] >= 0x50) && (rx_cashduino[1] <= 0x5F) )
      {
        ret_code = CASHDUINO_CMD_COIN_EVENT;
        switch (rx_cashduino[1]){
          case 0x50:
            *(amount) += 0.5;
            break;
          case 0x51:
            *(amount) += 0.5;
            break;
          case 0x52:
            *(amount) += 1;
            break;
          case 0x53:
            *(amount) += 2;
            break;
          case 0x54:
            *(amount) += 5;
            break;
          case 0x55:
            *(amount) += 10;
            break;
          break;
        }
      }
      break;
  }
  return ret_code;
}


/* algorithm to scan coin outs array and push data */
uint8_t push_coin_outs(store *data)
{
  uint8_t coin_out = false;
  uint8_t tubes_id[]={5,4,3,2,0}; /* channel definiton for MEI CF7000 */
  uint8_t i = 0;
  
  for(i = 0; i < 5; ++i)
  {
    if(data->coin_out[i] > 0)
    {
      data->arduino_task[1] = data->coin_out[i];
      data->coin_out[i] = 0;
      data->arduino_task[2] = tubes_id[i];
      send_task_to_cashduino(data, TASK_COIN_OUT);
      coin_out = true;
      break;
    }
  }
  
  return coin_out; 
  
}

/* This function will calculate the amount of coins to be dispense, this function
 * don't pretent ofuscate information, I know, the function can be optimized but this is not the case.
 */
void cashout_process(store *data)
{
  uint8_t coin_outs = 0;
  /* 
  Channel in MEI CF7000 
  MXN
  VALUE 0 | CHANNEL 1 = $0.50
  VALUE 1 | CHANNE    = $	
  VALUE 2 | CHANNEL 2 = $1
  VALUE 3 | CHANNEL 3 = $2
  VALUE 4 | CHANNEL 4 = $5
  VALUE 5 | CHANNEL 5 = $10   
  */
 
  /* Coin Channel - $10 MXN */
  if(data->tube_values[0] > 0)
  {
    coin_outs = data->cash_out / 10.00;
    if(coin_outs > data->tube_values[0])
    {
      coin_outs = data->tube_values[0];
    }
    data->coin_out[0] = coin_outs;
    data->cash_out = data->cash_out - ( 10 * coin_outs);
  }
  
  /* Coin Channel - $5 MXN */
  if(data->tube_values[1] > 0)
  {
    coin_outs = data->cash_out / 5.00;
    if(coin_outs > data->tube_values[1])
    {
      coin_outs = data->tube_values[1];
    }
    data->coin_out[1] = coin_outs;
    data->cash_out = data->cash_out - ( 5 * coin_outs);
  }
  
  /* Coin Channel - $2 MXN */
  if(data->tube_values[2] > 0)
  {
    coin_outs = data->cash_out / 2.00;
    if(coin_outs > data->tube_values[2])
    {
      coin_outs = data->tube_values[2];
    }
    data->coin_out[2] = coin_outs;
    data->cash_out = data->cash_out - ( 2 * coin_outs);
  }
  
  /* Coin Channel - $1 MXN */
  if(data->tube_values[3] > 0)
  {
    coin_outs = data->cash_out / 1.00;
    if(coin_outs > data->tube_values[3])
    {
      coin_outs = data->tube_values[3];
    }
    data->coin_out[3] = coin_outs;
    data->cash_out = data->cash_out - ( 1 * coin_outs);
  }
  
  /* Coin Channel - $0.50 MXN */
  if(data->tube_values[4] > 0)
  {
    coin_outs = data->cash_out / 0.50;
    if(coin_outs > data->tube_values[4])
    {
      coin_outs = data->tube_values[4];
    }
    data->coin_out[4] = coin_outs;
    data->cash_out = data->cash_out - ( 0.50 * coin_outs);
  }
    
}

/* function to update the event counter */
bool looking_for_new_event(uint8_t *rx_cashduino)
{
  uint8_t i = 0;
  if( (command != rx_cashduino[0]) || ( new_event != rx_cashduino[2] ) )
  {
      command = rx_cashduino[0];
      new_event = rx_cashduino[2];
      Serial.print("CASHDUINO ANSWER < " );                 /* Debug */
      for(i = 0; i<MAX_BUFF_CASHDUINO; i++){
        Serial.print(rx_cashduino[i], HEX);                 /* Debug */
      }
      Serial.print("\n" );                                  /* Debug */
      return true;
  }
  return false;
}

/* function to pull the buffer from the cashduino */
void read_buffer_from_cashduino(uint8_t *rx_cashduino)
{
  Wire.requestFrom(CASH_DUINO_ADDR, MAX_BUFF_CASHDUINO);    /* request 6 bytes from slave CashDuino */
  while (Wire.available()) {                                /* slave may send less than requested */
    *(rx_cashduino++) = Wire.read();                        /* receive a byte as character */
  }
}

/* function to push data over TWI */
void send_task_to_cashduino(store *data, uint8_t task)
{
  
  /* copy key */
  data->arduino_task[9] = 0x33;
  data->arduino_task[10] = 0xCC;
  data->arduino_task[11] = 0x33;
  data->arduino_task[12] = 0xCC;
  data->arduino_task[13] = 0x33;
  
  switch(task){
    case TASK_ENQUIRY_BOARD:
      data->arduino_task[0] = ARDUINO_CMD_ENQUIRY;
      break;
    case TASK_COIN_ENABLE:
      data->arduino_task[0] = ARDUINO_CMD_COIN_ENABLE;
      data->arduino_task[2] = 0xFF;
      data->arduino_task[4] = 0xFF;
      break;
    case TASK_COIN_DISABLE:
      data->arduino_task[0] = ARDUINO_CMD_COIN_ENABLE;
      break;
    case TASK_COIN_AUDIT:
      data->arduino_task[0] = ARDUINO_CMD_COIN_AUDIT;
      break;
    case TASK_COIN_OUT:
      data->arduino_task[0] = ARDUINO_CMD_COIN_OUT;
      break;
  }
  /* send data to CashDuino */
  arduino_push_buffer(data->arduino_task);
  
  /* clean up */
  memset(data->arduino_task, 0, sizeof(data->arduino_task));
 
}

/* push data over I2C buffer */
void arduino_push_buffer(uint8_t *cmd)
{
  uint8_t i = 0;
  Wire.beginTransmission(CASH_DUINO_ADDR);    /* transmit to CashDuino */
  for(i = 0; i<MAX_BUFF_ARDUINO; i++)
  {
    Wire.write(cmd[i]);                       /* sends one byte */
    Serial.print(cmd[i], HEX);                /* Debug */
  }
  Wire.endTransmission();                     /* stop transmitting */
  Serial.print("\n" );                        /* Debug */
}

