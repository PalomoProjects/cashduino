/*
 * Description: Integration code to use cashduino shield
 * Author: Jesus Palomo (jesus@palomothings.com)
 * Copyrigth: 2021 palomothings 
 * Project: using CashDuino-KIOSK and Arduino Mega with PC application over USB-SERIAL port.
 * Description: This code reads bills notes from Bill Acceptor device and send that information to a PC application using the SERIAL-PORT
 * Site: https://palomothings.com/
 */
 


 /* 
 * CASHDUINO KIOSK (BASE ON ARDUINO MEGA) PINOUT
 * 
 * LCD CONNECTION
 * LCD RS       12
 * LCD Enable   11
 * LCD D4       5
 * LCD D5       4
 * LCD D6       3
 * LCD D7       2
 * LCD R/W      GROUND  
 * 
 * BOTTON 1     22
 * BOTTON 2     23
 * BOTTON 3     24
 * 
 * RELAY        25
 * SS           53
 * LED          13
 * 
 */

#include <Wire.h>
#include <avr/io.h>
#include <string.h>
#include <stdlib.h>


#define MAX_BUFF_CASHDUINO    16
#define MAX_BUFF_ARDUINO      14
#define CASH_DUINO_ADDR       8
#define KEY_1                 0x33
#define KEY_2                 0xCC
#define NONE                  0xFF
#define MAX_BUFF_SERIAL_MSG   32
#define TOTAL_BYTE_SER_MSG    6
#define STX_PC                0x02
#define ETX_PC                0x03

/* PIN Board Relay */
#define PIN_RELAY             25
#define RELAY_ON  digitalWrite(PIN_RELAY, HIGH)
#define RELAY_OFF  digitalWrite(PIN_RELAY, LOW)

/* PIN arduino mega board led */
#define PIN_LED               13

/* Commands to read from PC application over serial port */
typedef enum{
  PC_ENQUIRY_BOARD = 0,
  PC_BILL_ENABLE,
  PC_BILL_DISABLE,
  PC_PULL_BUFFER,
  PC_INVALID_OPTION
}pc_recive;

/* Commands for send to PC over USB-Serial port (Arduino Mega)  */
typedef enum{
  ARDUINO_PC_ENQUIRY = 0,
  ARDUINO_PC_BILL_ENABLE,
  ARDUINO_PC_BILL_DISABLE,
  ARDUINO_PC_BILL_INSERTED,
  ARDUINO_PC_PULL_BUFFER,
  ARDUINO_PC_INVALID_OPTION
}pc_send;

/* Commands to send for CashDuino over TWI */
typedef enum{
  ARDUINO_CMD_ENQUIRY       = 0xC1,
  ARDUINO_CMD_BILL_ENABLE   = 0xB1,
  ARDUINO_CMD_BILL_ESCROW   = 0xB2,
}cmd_arduino;

/* Commanda to read from CashDuino over TWI */
typedef enum{
  CASHDUINO_CMD_ENQUIRY     = 0xD1,
  CASHDUINO_CMD_BILL_ENABLE = 0xE1,
  CASHDUINO_CMD_BILL_EVENT  = 0xE5,
}cmd_cashduino;

/* data structure for  */
typedef struct{
  uint8_t arduino_task  [MAX_BUFF_ARDUINO];
  pc_recive pc_option; 
  uint8_t cash_in;
  bool cashduino_present;
}store;

/* global variables */
static uint8_t new_event  = NONE;
static uint8_t command    = 0x00;

/* local functions to process answers from CashDuino and send task to CashDuino */
static bool is_a_valid_package(uint8_t *rx_cashduino);
static bool looking_for_new_event(uint8_t *rx_cashduino);
static void send_task_to_cashduino(store *data, pc_recive pc_option);
static void arduino_push_buffer(uint8_t *cmd);
static void read_buffer_from_cashduino(uint8_t *rx_cashduino);
static uint8_t parsing_cashduino_buffer(uint8_t *rx_cashduino, uint8_t *amount);

/* local functions to process request from PC and answer to PC */
static pc_recive serial_data_processing(store *data);
static void serial_data_send(store *data, pc_send pc_option);
static void serial_flush_buffer();

void setup() { 
  /* join i2c bus (address optional for master) */
  Wire.begin();
  
  /* put your setup code here, to run once: */
  Serial.begin(115200);

  /* setting I/O */
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_LED, OUTPUT);   
}

void loop() {
  
  /* initialize local variables */
  uint8_t rx_cashduino[MAX_BUFF_CASHDUINO]  = {0};
  uint8_t cashduino_cmd                     = NONE;
  uint8_t polling                           = 0;
  store data;

  memset(data.arduino_task, 0x00, sizeof(data.arduino_task));
  data.pc_option = PC_INVALID_OPTION;
  data.cash_in = 0;
  data.cashduino_present = false;
  
  RELAY_OFF;

  delay(5000); /*delay for wait bill acceptor*/
  Serial1.flush();
  serial_flush_buffer();
  
  send_task_to_cashduino(&data, PC_ENQUIRY_BOARD);
  
  while(1)
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
            data.cashduino_present = true;
            break;
          case CASHDUINO_CMD_BILL_EVENT:
            data.cash_in = rx_cashduino[1];
            serial_data_send(&data, ARDUINO_PC_BILL_INSERTED);
            break;
        }
      }
    }
    /* ###################### CASHDUINO ###################### */      
  
    /* processing another features */
    scheduler_store(&data); 
    
  }
  
}


/* this function will puilling the process that the system needs */
static void scheduler_store(store *data)
{  
  unsigned int i = 0;
  pc_recive pc_option = PC_INVALID_OPTION;

  /* time to attend another process */
  for(i = 0; i < 18000; i ++)
  { 
    pc_option = serial_data_processing(data);
    switch(pc_option)
    {
      case PC_ENQUIRY_BOARD:
        serial_data_send(data, ARDUINO_PC_ENQUIRY);
        break;
      case PC_BILL_ENABLE:
        send_task_to_cashduino(data, PC_BILL_ENABLE);
        serial_data_send(data, ARDUINO_PC_BILL_ENABLE);
        break;
      case PC_BILL_DISABLE:
        send_task_to_cashduino(data, PC_BILL_DISABLE);
        serial_data_send(data, ARDUINO_PC_BILL_DISABLE);
        break;
      case PC_PULL_BUFFER:
       serial_data_send(data, ARDUINO_PC_PULL_BUFFER);
        break;
      case PC_INVALID_OPTION:
        break;
    }
  }
  
}

/* send data to PC appliation using a custom data frame */
static void serial_data_send(store *data, pc_send pc_option)
{
  uint8_t arduino_pc[TOTAL_BYTE_SER_MSG] = {0};
  static uint8_t arduino_pc_aux[TOTAL_BYTE_SER_MSG] = {0};
  unsigned int checksum = 0; 
 
  switch(pc_option)
  {
    case ARDUINO_PC_ENQUIRY:
      arduino_pc[1] = 0xA0;
      break;
    case ARDUINO_PC_BILL_ENABLE:
      arduino_pc[1] = 0xA1;
      break;
    case ARDUINO_PC_BILL_DISABLE:
      arduino_pc[1] = 0xA2;
      break;
    case ARDUINO_PC_BILL_INSERTED:
      arduino_pc[1] = 0xA3;
      break;
    case ARDUINO_PC_PULL_BUFFER:
      memcpy(arduino_pc, arduino_pc_aux, TOTAL_BYTE_SER_MSG);
      break;
    default:
       arduino_pc[1] = 0xA0;
       break;
  }
  
  arduino_pc[0] = 0xF2;
  arduino_pc[2] = data->cash_in;
  arduino_pc[3] = data->cashduino_present;
  arduino_pc[4] = 0xF3;
  
  checksum = arduino_pc[1];
  checksum += arduino_pc[2];
  checksum += arduino_pc[3];
  checksum += arduino_pc[4];
  
  arduino_pc[5] = checksum & 0xFF;
  Serial.write(arduino_pc, sizeof(arduino_pc));
  memcpy(arduino_pc_aux, arduino_pc, TOTAL_BYTE_SER_MSG);
  Serial1.flush();
  serial_flush_buffer();
}

/* read instrucction from PC and parsing custom data frame */
static pc_recive serial_data_processing(store *data)
{
  uint8_t cheksum = 0;
  uint8_t serial_data[MAX_BUFF_SERIAL_MSG] = {0};
  uint8_t serial_index = 0;

  data->pc_option = PC_INVALID_OPTION;
  
  if(Serial.available() == TOTAL_BYTE_SER_MSG)
  {
    while(Serial.available())
    {
      serial_data[serial_index] = Serial.read();
      serial_index++;
    }
    Serial1.flush();
    serial_flush_buffer();
  } 
  
  if ( (serial_index == TOTAL_BYTE_SER_MSG ) && ( serial_data[0] == STX_PC) && ( serial_data[4] == ETX_PC ) )
  {
    cheksum = ( serial_data[1] + serial_data[2] + serial_data[3] + serial_data[4] ) & 0xFF;
        
    if( serial_data[5] == cheksum )
    {
      if ( ( serial_data[1] == 0xB0) )
      {
        data->pc_option = PC_ENQUIRY_BOARD;
      }
      if ( ( serial_data[1] == 0xB1) )
      {
        data->pc_option = PC_BILL_ENABLE;
      }
      if ( ( serial_data[1] == 0xB2) )
      {
        data->pc_option = PC_BILL_DISABLE;
      }
      if ( ( serial_data[1] == 0xB3) )
      {
        data->pc_option = PC_PULL_BUFFER;
      }
      serial_index = 0; 
    }
  }  
  
  /* reset buffer index and clean up */
  if(serial_index >= TOTAL_BYTE_SER_MSG)
  {
    serial_index = 0;
    Serial1.flush();
    serial_flush_buffer();
  }
  
  /* safety function to avoid overflow */
  if(serial_index >= MAX_BUFF_SERIAL_MSG)
  {
    serial_index = 0;
    Serial1.flush();
    serial_flush_buffer();
  }
  
  return data->pc_option;
  
}


/* function to validate the data structure */
static bool is_a_valid_package(uint8_t *rx_cashduino)
{
  if( (rx_cashduino[11] == KEY_1) && (rx_cashduino[12] == KEY_2) &&
      (rx_cashduino[13] == KEY_1) && (rx_cashduino[14] == KEY_2) &&
      (rx_cashduino[15] == KEY_1) )
  {
    return true;
  }
  return false;
}

/* function to update the event counter */
static bool looking_for_new_event(uint8_t *rx_cashduino)
{
  uint8_t i = 0;
  if( (command != rx_cashduino[0]) || ( new_event != rx_cashduino[2] ) )
  {
      command = rx_cashduino[0];
      new_event = rx_cashduino[2];
      //Serial.print("CASHDUINO ANSWER < " );                 /* Debug */
      for(i = 0; i<MAX_BUFF_CASHDUINO; i++){
        //Serial.print(rx_cashduino[i], HEX);                 /* Debug */
      }
      //Serial.print("\n" );                                  /* Debug */
      return true;
  }
  return false;
}

static void serial_flush_buffer()
{
  while (Serial.read() >= 0)
   ; // do nothing
}

/* function to push data over TWI */
static void send_task_to_cashduino(store *data, pc_recive pc_option)
{
  
  /* copy key */
  data->arduino_task[9] = KEY_1;
  data->arduino_task[10] = KEY_2;
  data->arduino_task[11] = KEY_1;
  data->arduino_task[12] = KEY_2;
  data->arduino_task[13] = KEY_1;
  
  switch(pc_option)
  {
    case PC_ENQUIRY_BOARD:
      data->arduino_task[0] = ARDUINO_CMD_ENQUIRY;
      break;
    case PC_BILL_ENABLE:
      data->arduino_task[0] = ARDUINO_CMD_BILL_ENABLE;
      data->arduino_task[1] = 0xFF;
      data->arduino_task[2] = 0xFF;
      data->arduino_task[3] = 0x00;
      data->arduino_task[4] = 0x00;
      break;
    case PC_BILL_DISABLE:
      data->arduino_task[0] = ARDUINO_CMD_BILL_ENABLE;
      break;
  }
  
  /* send data to CashDuino */
  arduino_push_buffer(data->arduino_task);
  
  /* clean up */
  memset(data->arduino_task, 0, sizeof(data->arduino_task));
 
}

/* push data over I2C buffer */
static void arduino_push_buffer(uint8_t *cmd)
{
  uint8_t i = 0;
  Wire.beginTransmission(CASH_DUINO_ADDR);    /* transmit to CashDuino */
  for(i = 0; i<MAX_BUFF_ARDUINO; i++)
  {
    Wire.write(cmd[i]);                       /* sends one byte */
    //Serial.print(cmd[i], HEX);                /* Debug */
  }
  Wire.endTransmission();                     /* stop transmitting */
  //Serial.print("\n" );                        /* Debug */
}

/* function to pull the buffer from the cashduino */
static void read_buffer_from_cashduino(uint8_t *rx_cashduino)
{
  Wire.requestFrom(CASH_DUINO_ADDR, MAX_BUFF_CASHDUINO);    /* request 6 bytes from slave CashDuino */
  while (Wire.available()) {                                /* slave may send less than requested */
    *(rx_cashduino++) = Wire.read();                        /* receive a byte as character */
  }
}

/* function to parsing the cashduino buffer and get information */
static uint8_t parsing_cashduino_buffer(uint8_t *rx_cashduino, uint8_t *amount)
{
  uint8_t ret_code = NONE;
  
  /* type of command */
  switch(rx_cashduino[0])
  {
    case CASHDUINO_CMD_ENQUIRY:
      ret_code = CASHDUINO_CMD_ENQUIRY;
      break;
   case CASHDUINO_CMD_BILL_EVENT:
          ret_code = CASHDUINO_CMD_BILL_EVENT;
      break;
  }
  return ret_code;
}
