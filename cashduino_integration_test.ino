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

#include <Wire.h>
#include <LiquidCrystal.h>

/*LCD pin to Arduino*/
const int pin_RS = 8; 
const int pin_EN = 9; 
const int pin_d4 = 4; 
const int pin_d5 = 5; 
const int pin_d6 = 6; 
const int pin_d7 = 7; 
const int pin_BL = 10; 
LiquidCrystal lcd( pin_RS,  pin_EN,  pin_d4,  pin_d5,  pin_d6,  pin_d7);


#define MAX_BUFF_CASHDUINO    16
#define MAX_BUFF_ARDUINO      14
#define CASH_DUINO_ADDR       8
#define KEY_1                 0x33
#define KEY_2                 0xCC
#define NONE                  0xFF
#define DISABLE_COIN_DEVICE   2
#define DISABLE_BILL_DEVICE   6
#define CASHDUINO_POLL_TIME   250
#define MAX_BUFFER_MSG        40

/* options to the user, the user need to type in the Serial Monitor 1 to 9 */
typedef enum{
  SEND_ENQUIRY_BOARD = 0,
  SEND_COIN_ENABLE,
  COIN_DISABLE,
  SEND_COIN_AUDIT,
  SEND_COIN_OUT,
  INVALID_OPTION
}keys_options;

/* arduino tasks options */
typedef enum{
  ARDUINO_CMD_ENQUIRY       = 0xC1,
  ARDUINO_CMD_COIN_ENABLE   = 0xA1,
  ARDUINO_CMD_COIN_AUDIT    = 0xA2,
  ARDUINO_CMD_COIN_OUT      = 0xA3,
}cmd_arduino;

/* cashduino cmd answers */
typedef enum{
  CASHDUINO_CMD_ENQUIRY     = 0xD1,
  CASHDUINO_CMD_COIN_ENABLE = 0xF1,
  CASHDUINO_CMD_COIN_AUDIT  = 0xF2,
  CASHDUINO_CMD_COIN_OUT    = 0xF3,
  CASHDUINO_CMD_COIN_EVENT  = 0xF4,
}cmd_cashduino;


/* global variables */
uint8_t new_event  = NONE;
uint8_t command    = 0x00;
double  amount     = 0;

void setup() {
  lcd.begin(16, 2);
  lcd.setCursor(3,0);
  lcd.print("CashDuino");
  /* join i2c bus (address optional for master) */
  Wire.begin();
}

void loop() {
  uint8_t rx_cashduino[MAX_BUFF_CASHDUINO]  = {0};
  uint8_t cashduino_cmd                     = NONE;
  uint8_t key_button                        = 0;
  
  key_button = analogRead (0);
 
  lcd.setCursor(0,1);
  if (key_button < 60) /* right key */
  {
   amount = 0;
   send_task_to_cashduino(SEND_ENQUIRY_BOARD);
  }

  /* read cashduino buffer */
  read_buffer_from_cashduino(rx_cashduino);

  /* is a new event? */
  if ( looking_for_new_event(rx_cashduino) == true )
  {
    /* is a valid package? */
    if( is_a_valid_package(rx_cashduino) == true )
    {
      cashduino_cmd = parsing_buffer_cashduino(rx_cashduino);
      if(cashduino_cmd == CASHDUINO_CMD_ENQUIRY)
      {
        lcd.setCursor(0,1);
        lcd.print("enquiry = ");
        lcd.setCursor(10,1);
        lcd.print(rx_cashduino[2]);
      }
      
      /* parsing buffer to find data */
      cashduino_cmd = parsing_buffer_coin_acceptor(rx_cashduino, &amount);
  
      if ( cashduino_cmd == CASHDUINO_CMD_COIN_EVENT)
      {
        lcd.setCursor(0,1);
        lcd.print("Cash: $ " );
        lcd.setCursor(8,1);
        lcd.print(amount, 2); /* pint amount */
      }
    }
  }

  /* time to enable cashduino data process  */
  delay(CASHDUINO_POLL_TIME); 

}

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

/* function to know if the board is working */
uint8_t parsing_buffer_cashduino(uint8_t *rx_cashduino)
{
  uint8_t ret_code = NONE;
  /* type of command */
  switch(rx_cashduino[0])
  {
    case CASHDUINO_CMD_ENQUIRY:
      ret_code = CASHDUINO_CMD_ENQUIRY;
      break; 
  }
  return ret_code;
}

/* function to parsing the cashduino buffer and get information */
uint8_t parsing_buffer_coin_acceptor(uint8_t *rx_cashduino, double *amount)
{
  uint8_t ret_code = NONE;
  /* type of command */
  switch(rx_cashduino[0])
  {
    case CASHDUINO_CMD_COIN_ENABLE:
      /*Serial.print("Coin charger configured \n" ); //debug*/
      break;
    case CASHDUINO_CMD_COIN_AUDIT:
      ret_code = CASHDUINO_CMD_COIN_AUDIT;
      break;
    case CASHDUINO_CMD_COIN_OUT:
      /*Serial.print("Coin changer dispense coin \n" );*/
      break;
     case CASHDUINO_CMD_COIN_EVENT:
      /*Serial.print("Event Counter: ");Serial.print(rx_cashduino[2]);Serial.print("\n"); //debug*/
      if( (rx_cashduino[1] >= 0x40) && (rx_cashduino[1] <= 0x5F))
      {
        ret_code = CASHDUINO_CMD_COIN_EVENT;
        /*Serial.print("Coin in CashBox =) -> " ); //debug*/
        switch (rx_cashduino[1]){
          case 0x41:
            /*Serial.print("($0.50 MXN) Inserted\n"); //debug*/
            *(amount) += 0.5;
            break;
          case 0x42:
            /*Serial.print("($1 MXN) Inserted\n"); //debug*/
            *(amount) += 1;
            break;
          case 0x43:
            /*Serial.print("($2 MXN) Inserted\n"); //debug*/
            *(amount) += 2;
            break;
          case 0x44:
            /*Serial.print("($5 MXN) Inserted\n"); //debug*/
            *(amount) += 5;
            break;
          case 0x45:
            /*Serial.print("($10 MXN) Inserted\n"); //debug*/
            *(amount) += 10;
            break;
        }
      }
      break;
  }
  return ret_code;
}

/* function to update the event counter */
bool looking_for_new_event(uint8_t *rx_cashduino)
{
  uint8_t i = 0;
  if( (command != rx_cashduino[0]) || ( new_event != rx_cashduino[2] ) )
  {
      command = rx_cashduino[0];
      new_event = rx_cashduino[2];
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
void send_task_to_cashduino(uint8_t selection){

  uint8_t ARDUINO_TASK [] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33};

  switch(selection){
    case SEND_ENQUIRY_BOARD:
      ARDUINO_TASK[0] = ARDUINO_CMD_ENQUIRY;
      break;
    case SEND_COIN_ENABLE:
      ARDUINO_TASK[0] = ARDUINO_CMD_COIN_ENABLE;
      ARDUINO_TASK[2] = 0xFF;
      ARDUINO_TASK[4] = 0xFF;
      break;
    case COIN_DISABLE:
      ARDUINO_TASK[0] = ARDUINO_CMD_COIN_ENABLE;
      break;
    case SEND_COIN_AUDIT:
      ARDUINO_TASK[0] = ARDUINO_CMD_COIN_AUDIT;
      break;
    case SEND_COIN_OUT:
      ARDUINO_TASK[0] = ARDUINO_CMD_COIN_OUT;
      ARDUINO_TASK[1] = 0x01;
      ARDUINO_TASK[2] = 0x02;
      break;
  }

  if( (selection >= SEND_ENQUIRY_BOARD) && (selection < INVALID_OPTION) )
  {
    arduino_push_buffer(ARDUINO_TASK);
  }
 
}

void arduino_push_buffer(uint8_t *cmd)
{
  uint8_t i = 0;                
  Wire.beginTransmission(CASH_DUINO_ADDR);    /* transmit to CashDuino */
  for(i = 0; i<MAX_BUFF_ARDUINO; i++)
  {
    Wire.write(cmd[i]);                       /* sends one byte */
  }
  Wire.endTransmission();                     /* stop transmitting */
}
