#include <Wire.h>

#define MAX_BUFF_CASHDUINO    16
#define MAX_BUFF_ARDUINO      14
#define CASH_DUINO_ADDR       8
#define KEY_1                 0x33
#define KEY_2                 0xCC
#define NONE                  0xFF
#define DISABLE_COIN_DEVICE   2
#define DISABLE_BILL_DEVICE   6
#define CASHDUINO_POLL_TIME   250

#define CASHDUINO_COIN_AUDIT  0xF2
#define CASHDUINO_COIN_EVENT  0xF4
#define CASHDUINO_BILL_EVENT  0xE5


uint8_t new_event  = 0xFF;
uint8_t command    = 0x00;

double  amount     = 0;
uint8_t cent       = 0;
uint8_t un_peso    = 0;
uint8_t dos_peso   = 0;
uint8_t cinco_peso = 0;
uint8_t diez_peso  = 0;

void setup() {
  // join i2c bus (address optional for master)
  Wire.begin();
  // put your setup code here, to run once:
  Serial.begin(9600);
}

/*
#### CASHLESS EVENTS - AFTER ENABLE WAIT FOR USER TO INSERT (SWIPE) CARD
<--- ARDUINO READ FROM CASHDUINO OVER I2C:
CMD SUB  EVT  DAT3 DAT4 DAT5 DAT6 DAT7 DAT8 DAT9 DAT10 KEY1 KEY2 KEY3 KEY4 KEY5 <- 16 Bytes
2B  00   00   00   00   00   00   00   00   00   00    33   CC   33   CC   33

|DAT3 |   DESCRIPTION   |
|*0x01|   CASH_SALE OK  |
|0x03 |   BEGIN SESSION |
|0x04 |   CANCELLED     |
|0x05 |   VEND APPROVED |
|0x06 |   VEND DENIED   |
|0x07 |   END SESSION   |
*/

void loop() 
{
 uint8_t MATRIX_CMD [][14] = {
                             {0xC1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},    /* COIN ENABLE */
                             {0x1A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},    /* CASHLESS ENABLE TASK */
                             {0x1A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},    /* CASHLESS ENABLE TASK */
                             {0x1B, 0x00, 0x64, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},    /* CASHLESS CHARGE AMOUNT */
                             {0x1C, 0x01, 0x64, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},    /* CASHLESS VEND SUCCESS */
                             {0x1D, 0x01, 0x64, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33},    /* CASHLESS CANCEL VEND */
                             {0x1E, 0x01, 0x64, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x33, 0xCC, 0x33, 0xCC, 0x33}     /* CASHLESS CASH SALE */
                            };
 
 /* type in the send box section the selecction that you wants */
 char MATRIX_MESSAGE[][60] = {"ARDUINO SEND ENQUIRY BOARD > ",                    /* option 1 */
                              "ARDUINO SEND CASHLESS ENABLE TASK > ",             /* option 2 */
                              "ARDUINO SEND CASHLESS DISABLE TASK > ",            /* option 3 */
                              "ARDUINO SEND CASHLESS VEND REQUEST TASK > ",       /* option 4 */
                              "ARDUINO SEND CASHLESS VEND SUCCESS TASK > ",       /* option 5 */
                              "ARDUINO SEND CASHLESS CANCEL VEND TASK > ",        /* option 6 */
                              "ARDUINO SEND CASHLESS CASH SALE TASK > "           /* option 7 */
                             };
                             
  uint8_t rx_cashduino[MAX_BUFF_CASHDUINO] = {0};
  uint8_t pc_option      = NONE;
  uint8_t cashless_cmd   = NONE;

  /* lisent the serial port and sends task */
  if ( ( pc_option = read_buffer_from_serial() ) != NONE ){
    if( (pc_option == DISABLE_COIN_DEVICE) || (pc_option == DISABLE_BILL_DEVICE) ) {
      amount = 0;
    }
    send_task_to_cashduino(MATRIX_MESSAGE[pc_option], MATRIX_CMD[pc_option]);
  }

  /* read the CashDuino buffer */
  read_buffer_from_cashduino(rx_cashduino);

  /* is a new event? */
  if ( looking_for_new_event(rx_cashduino) == true )
  {
    /* is a valid package? */
    if( is_a_valid_package(rx_cashduino) == true )
    {
      cashless_cmd = parsing_buffer_cashless(rx_cashduino, &amount);
    }
  }
  
  delay(CASHDUINO_POLL_TIME);

}

/* function to validate the data structure */
bool is_a_valid_package(uint8_t *rx_cashduino){
  if( (rx_cashduino[11] == KEY_1) && (rx_cashduino[12] == KEY_2) &&
      (rx_cashduino[13] == KEY_1) && (rx_cashduino[14] == KEY_2) &&
      (rx_cashduino[15] == KEY_1) ){
    return true;
  }
  return false;
}

/* function to parsing the cashduino buffer and get information */
uint8_t parsing_buffer_cashless(uint8_t *rx_cashduino, double *amount){

  uint8_t ret_code = NONE;
  /* type of command */
  switch(rx_cashduino[0])
  {
    case 0xD1:
      Serial.print("CashDuino is a live :) \n" );//debug
      break;
    case 0x2A:
      //Serial.print("DATA: " ); Serial.print(rx_cashduino[3], HEX);Serial.print("\n");
      if(rx_cashduino[3] == 0x01)
      {
        Serial.print("Cashless is enable, waiting for insert card ... :)\n" );//debug 
      }
      if(rx_cashduino[3] == 0x00)
      {
        Serial.print("Cashless is disable :)\n" );//debug 
      }
      break;
    case 0x2B:
       if(rx_cashduino[1] == 0x03)
       {
         Serial.print("CASHLESS BEGIN SESSION \n" );//debug
       }
       if(rx_cashduino[1] == 0x04)
       {
         Serial.print("CASHLESS CANCEL REQUEST \n" );//debug
       }
       if(rx_cashduino[1] == 0x05)
       {
         Serial.print("CASHLESS VEND APPROVED \n" );//debug
       }
       if(rx_cashduino[1] == 0x06)
       {
         Serial.print("CASHLESS VEND DENIED \n" );//debug
       }
       if(rx_cashduino[1] == 0x07)
       {
         Serial.print("CASHLESS SESSION END, THANK YOU! \n" );//debug
       }
      break;
  }
  return ret_code;
}

/* function to pull the buffer from the cashduino */
bool looking_for_new_event(uint8_t *rx_cashduino){
  uint8_t i = 0;
  if( (command != rx_cashduino[0]) || ( new_event != rx_cashduino[2] ) ){
      command = rx_cashduino[0];
      new_event = rx_cashduino[2];
      Serial.print("CASHDUINO ANSWER < " );                 //Debug
      for(i = 0; i<MAX_BUFF_CASHDUINO; i++){
        Serial.print(rx_cashduino[i], HEX);                 //Debug
      }
      Serial.print("\n" );                                  //Debug
      return true;
  }
  return false;
}

void read_buffer_from_cashduino(uint8_t *rx_cashduino){
  Wire.requestFrom(CASH_DUINO_ADDR, MAX_BUFF_CASHDUINO);    // request 6 bytes from slave CashDuino
  while (Wire.available()) {                                // slave may send less than requested
    *(rx_cashduino++) = Wire.read();                        // receive a byte as character
    /*Serial.println(rx_cashduino[cashduino_id], HEX);*/    // Debug print the character
  }
}

/* function to push data over TWI */
void send_task_to_cashduino(const char *message, uint8_t *cmd){
  Serial.print(message);                      //Debug
  uint8_t i = 0;
  Wire.beginTransmission(CASH_DUINO_ADDR);    // transmit to CashDuino
  for(i = 0; i<MAX_BUFF_ARDUINO; i++){
    Wire.write(cmd[i]);                       // sends one byte
    Serial.print(cmd[i], HEX);                // Debug
  }
  Wire.endTransmission();                     // stop transmitting
  Serial.print("\n" );                        //Debug
}

/* function to sends task */
uint8_t read_buffer_from_serial(void){
  uint8_t serial_option  = 0;
  if (Serial.available() > 0)
  {
    serial_option = Serial.read();
    if( ( serial_option >= '1' ) && (serial_option <= '9') ){
      serial_option -= '0';
      return serial_option - 1;
    }
  }
  return NONE;
}
