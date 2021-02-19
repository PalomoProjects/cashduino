/*
 * Description: Integration code to use cashduino shiled 
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
#include <avr/pgmspace.h>

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

/* user select the option (1-9) and display the message in the Serial Monitor */
 const char MATRIX_MESSAGE[][MAX_BUFFER_MSG] PROGMEM = {"ARDUINO SEND ENQUIRY BOARD > ",        /* option 1 */
                                                        "ARDUINO SEND COIN ENABLE TASK > ",     /* option 2 */
                                                        "ARDUINO SEND COIN DISABLE TASK > ",    /* option 3 */
                                                        "ARDUINO SEND COIN AUDIT TASK > ",      /* option 4 */
                                                        "ARDUINO SEND COIN OUT TASK > ",        /* option 5 */
                                                        "ARDUINO SEND BILL ENABLE TASK > ",     /* option 6 */
                                                        "ARDUINO SEND BILL DISABLE TASK > ",    /* option 7 */
                                                        "ARDUINO SEND BILL TO STACKER TASK > ", /* option 8 */
                                                        "ARDUINO SEND BILL REJECT TASK > "      /* option 9 */
                                                       };

/* options to the user, the user need to type in the Serial Monitor 1 to 9 */
typedef enum{
  SEND_ENQUIRY_BOARD = 0,
  SEND_COIN_ENABLE,
  COIN_DISABLE,
  SEND_COIN_AUDIT,
  SEND_COIN_OUT,
  SEND_BILL_ENABLE,
  SEND_BILL_DISABLE,
  SEND_BILL_TO_STACKER,
  SEND_BILL_REJECT,
  INVALID_OPTION
}pc_options;

/* arduino tasks options */
typedef enum{
  ARDUINO_CMD_ENQUIRY       = 0xC1,
  ARDUINO_CMD_COIN_ENABLE   = 0xA1,
  ARDUINO_CMD_COIN_AUDIT    = 0xA2,
  ARDUINO_CMD_COIN_OUT      = 0xA3,
  ARDUINO_CMD_BILL_ENABLE   = 0xB1,
  ARDUINO_CMD_BILL_ESCROW   = 0xB2
}cmd_arduino;

/* cashduino cmd answers */
typedef enum{
  CASHDUINO_CMD_ENQUIRY     = 0xD1,
  CASHDUINO_CMD_COIN_ENABLE = 0xF1,
  CASHDUINO_CMD_COIN_AUDIT  = 0xF2,
  CASHDUINO_CMD_COIN_OUT    = 0xF3,
  CASHDUINO_CMD_BILL_ENABLE = 0xE1,
  CASHDUINO_CMD_BILL_ESCROW = 0xE2,
  CASHDUINO_CMD_COIN_EVENT  = 0xF4,
  CASHDUINO_CMD_BILL_EVENT  = 0xE5,
}cmd_cashduino;


/* global variables */
uint8_t new_event  = NONE;
uint8_t command    = 0x00;
double  amount     = 0;

void setup() {
  /* join i2c bus (address optional for master) */
  Wire.begin();
  /* put your setup code here, to run once: */
  Serial.begin(9600);
}

void loop() {
  uint8_t rx_cashduino[MAX_BUFF_CASHDUINO]  = {0};
  uint8_t selection                         = INVALID_OPTION;
  uint8_t bill_cmd                          = NONE;
  uint8_t coin_cmd                          = NONE;

  /* read serial port and send task to cashduino */
  if ( ( selection = read_buffer_from_serial() ) != INVALID_OPTION )
  {
    if( (selection == DISABLE_COIN_DEVICE) || (selection == DISABLE_BILL_DEVICE) )
    {
      amount = 0;
    }
    /* it is a valid data, push the buffer to cashduino */
    send_task_to_cashduino(selection);
  }

  /* read cashduino buffer */
  read_buffer_from_cashduino(rx_cashduino);

  /* is a new event? */
  if ( looking_for_new_event(rx_cashduino) == true )
  {
    /* is a valid package? */
    if( is_a_valid_package(rx_cashduino) == true )
    {
      /* parsing buffer to find data */
      parsing_buffer_cashduino(rx_cashduino);
      coin_cmd = parsing_buffer_coin_acceptor(rx_cashduino, &amount);
      bill_cmd = parsing_buffer_bill_acceptor(rx_cashduino, &amount);

      if ( ( coin_cmd == CASHDUINO_CMD_COIN_EVENT) || ( bill_cmd == CASHDUINO_CMD_BILL_EVENT) )
      {
        Serial.print("Cash: $ " );
        Serial.println(amount, 2); /* pint amount */
      }
      else if(coin_cmd == CASHDUINO_CMD_COIN_AUDIT)
      {
        /* print channel values, information available from 3 to 9 */
        Serial.print(rx_cashduino[3]);Serial.print("($0.50 MXN)\n");
        Serial.print(rx_cashduino[4]);Serial.print("($1 MXN)\n");
        Serial.print(rx_cashduino[5]);Serial.print("($2 MXN)\n");
        Serial.print(rx_cashduino[6]);Serial.print("($5 MXN)\n");
        Serial.print(rx_cashduino[7]);Serial.print("($10 MXN)\n");
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
void parsing_buffer_cashduino(uint8_t *rx_cashduino)
{
  /* type of command */
  switch(rx_cashduino[0])
  {
    case CASHDUINO_CMD_ENQUIRY:
      /* Serial.print("CashDuino is a live :) \n" ); //debug */
      break; 
  }
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
      if( (rx_cashduino[1] >= 0x40) && (rx_cashduino[1] <= 0x4F))
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
      if( (rx_cashduino[1] >= 0x50) && (rx_cashduino[1] <= 0x5F) )
      {
        ret_code = CASHDUINO_CMD_COIN_EVENT;
        /*Serial.print("Coin in Tube =) -> " ); //debug*/
        switch (rx_cashduino[1]){
          case 0x51:
            /*Serial.print("$0.50 MXN\n"); //debug*/
            *(amount) += 0.5;
            break;
          case 0x52:
            /*Serial.print("$1 MXN\n"); //debug*/
            *(amount) += 1;
            break;
          case 0x53:
            /*Serial.print("$2 MXN\n"); //debug*/
            *(amount) += 2;
            break;
          case 0x54:
            /*Serial.print("$5 MXN\n"); //debug*/
            *(amount) += 5;
            break;
          case 0x55:
            /*Serial.print("$10 MXN\n"); //debug*/
            *(amount) += 10;
            break;
          break;
        }
      }
      if(rx_cashduino[1] == 0x01)
      {
        /*Serial.print("Push Reject =(\n" );//debug*/
      }
      break;
  }
  return ret_code;
}

/* function to parsing the cashduino buffer and get information */
uint8_t parsing_buffer_bill_acceptor(uint8_t *rx_cashduino, double *amount)
{
  uint8_t ret_code = NONE;
  /* type of command */
  switch(rx_cashduino[0])
  {
    case CASHDUINO_CMD_BILL_ENABLE:
      /*Serial.print("Bill acceptor configured :)\n" );//debug*/
      break;
    case CASHDUINO_CMD_BILL_ESCROW:
      /*Serial.print("Bill Escrow\n" );//debug*/
      break;
    case 0xE3:
      /*Serial.print("Bill Audit\n" );//debug*/
      break;
    case 0xE4:
      /*Serial.print("Bill Out\n" );//debug*/
      break;
    case CASHDUINO_CMD_BILL_EVENT:
      /*Serial.print("Event Counter: ");Serial.print(rx_cashduino[2]);Serial.print("\n");*/
      if( (rx_cashduino[1] >= 0x90) && (rx_cashduino[1] <= 0x9F))
      {
        /*Serial.print("Bill in pre stacker =) -> " );*/
        switch (rx_cashduino[1]){
          case 0x90:
            /*Serial.print("$20 MXN\n");//debug*/
            break;
          case 0x91:
            /*Serial.print("$50 MXN\n");//debug*/
            break;
          case 0x92:
            /*Serial.print("$100 MXN\n");//debug*/
            break;
          case 0x93:
            /*Serial.print("$200 MXN\n");//debug*/
            break;
          case 0x94:
            /*Serial.print("$500 MXN\n");//debug*/
            break;
          break;
        }
      }
      if( (rx_cashduino[1] >= 0x80) && (rx_cashduino[1] <= 0x8F))
      {
        ret_code = CASHDUINO_CMD_BILL_EVENT;
        /*Serial.print("Bill in cashbox =) -> " );//debug*/
        switch (rx_cashduino[1]){
          case 0x80:
            /*Serial.print("$20 MXN\n");//debug*/
            *(amount) += 20;
            break;
          case 0x81:
            /*Serial.print("$50 MXN\n");//debug*/
            *(amount) += 50;
            break;
          case 0x82:
            /*Serial.print("$100 MXN\n");//debug*/
            *(amount) += 100;
            break;
          case 0x83:
            /*Serial.print("$200 MXN\n");//debug*/
            *(amount) += 200;
            break;
          case 0x84:
            /*Serial.print("$500 MXN\n");//debug*/
            *(amount) += 500;
            break;
          break;
        }
      }
      if( (rx_cashduino[1] >= 0xA0) && (rx_cashduino[1] <= 0xAF))
      {
        /*Serial.print("Bill rejected =) -> " );*/
        switch (rx_cashduino[1]){
          case 0xA0:
            /*Serial.print("$20 MXN\n");//debug*/
            break;
          case 0xA1:
            /*Serial.print("$50 MXN\n");//debug*/
            break;
          case 0xA2:
            /*Serial.print("$100 MXN\n");//debug*/
            break;
          case 0xA3:
            /*Serial.print("$200 MXN\n");//debug*/
            break;
          case 0xA4:
            /*Serial.print("$500 MXN\n");//debug*/
            break;
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
    /* Serial.println(rx_cashduino[cashduino_id], HEX); */  /* Debug print the cashduino buffer */
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
    case SEND_BILL_ENABLE:
      ARDUINO_TASK[0] = ARDUINO_CMD_BILL_ENABLE;
      ARDUINO_TASK[1] = 0xFF;
      ARDUINO_TASK[2] = 0xFF;
      ARDUINO_TASK[3] = 0xFF;
      ARDUINO_TASK[4] = 0xFF;
    break;
    case SEND_BILL_DISABLE:
      ARDUINO_TASK[0] = ARDUINO_CMD_BILL_ENABLE;
    break;
    case SEND_BILL_TO_STACKER:
      ARDUINO_TASK[0] = ARDUINO_CMD_BILL_ESCROW;
      ARDUINO_TASK[1] = 0x01;
    break;
    case SEND_BILL_REJECT:
      ARDUINO_TASK[0] = ARDUINO_CMD_BILL_ESCROW;
    break;
    case INVALID_OPTION:
    break;
  }

  if( (selection >= SEND_ENQUIRY_BOARD) && (selection <= SEND_BILL_REJECT) )
  {
    arduino_push_buffer(MATRIX_MESSAGE[selection], ARDUINO_TASK);
  }
 
}

void arduino_push_buffer(const char *message, uint8_t *cmd)
{
  char myChar = 0;
  uint8_t i = 0;
  for (i = 0; i < strlen_P(message); i++) {
    myChar = pgm_read_word_near(message + i); /* Debug */
    Serial.print(myChar);
  }                     
  
  Wire.beginTransmission(CASH_DUINO_ADDR);    /* transmit to CashDuino */
  for(i = 0; i<MAX_BUFF_ARDUINO; i++)
  {
    Wire.write(cmd[i]);                       /* sends one byte */
    Serial.print(cmd[i], HEX);                /* Debug */
  }
  Wire.endTransmission();                     /* stop transmitting */
  Serial.print("\n" );                        /* Debug */
}

/* function to read the user option */
uint8_t read_buffer_from_serial(void)
{
  uint8_t serial_option  = 0;
  if (Serial.available() > 0)
  {
    serial_option = Serial.read();
    if( ( serial_option >= '1' ) && (serial_option <= '9') )
    {
      serial_option -= '0';
      return serial_option - 1;
    }
  }
  return INVALID_OPTION;
}
