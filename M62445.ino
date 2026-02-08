/*
This code is a skeleton to be used as a development base for the audio controller ref: M62445.
It is not clean and does not include protections.
Bit manipulations were left in binary so that the change caused by altering a bit is more perceptible.
It does not depend on external libraries except for the I2C protocol.
No dependencies or instructions for calling functions were created.
*/

//Notes presents on datasheet of M62445.
/*Send burst of 18 bits: D0^D17
Where: (A)
D0D1D2 = I0I1I2:inputSelectroControl
D3D4D5 = G0G1G2: Gain Control
D6 = M0: Rec Mute
D7 =0
D8D9D10D11 = K0K1K2K3: Karaoke Control
D12D13D14 = 0
D15D16D17 = 0,0,0
Where (B)
D0D1D2D3 = O0O1O2O3: Port Output 1~4 (Open Colector Output)
D4D5D6D7 = Ss0Ss1St0St1: Port Output 5,6(3 state Output)
D8D9D10D11 = O6O7O8O9: Port Output 7~10(Open Colector Output)
D12D13D14 = 0
D15D16D17 = 1,0,0
Where: (C)
D0D1D2D3D4D5D6 = L0L1L2L3L4L5L6: Lch Volume control
D7 = 1
D8D9D10D11D12D13D14 = R0R1R2R3R4R5R6: RchVolume Control
D15D16D17 = 0,1,1
Where: (D)
D0D1D2D3 = B0B1B2B3 = Bass boost/cut
D4D5D6D7 = T0T1T2T3: Treble Boost/cut
D8D9D10 = TB0TB1TB2: T-Bass Gain
D11D12D13D14 = 0
D15D16D17 = 1,1,1
*/

/*
  //Port support 3tate Output.  F bright RED and bright GREEN or 3 
  //or C one each time! The map about this is on the page 9/17
  // SsX is port 5, StX is port 6
  //Ss0 Ss1 St0 St1
  //0   0   0   0   LOW
  //1   0   1   0   open
  //0   1   0   1   open
  //1   1   1   1   HIGH
*/

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x3F, 20, 2);  // set the LCD address to 0x3F for a 16 chars and 2 line display

#define VFD_data 7   // 
#define VFD_clk 8    // 
#define VFD_latch 9  // 

const int phaseA = 2;  // This is the trigger pin number 2, Arduino have two pins allow trigger pin 2 and 3!
const int phaseB = 3;  // This is the trigger pin number 3, Arduino have two pins allow trigger pin 2 and 3!

// Note used to simplify and let the code more basic!!!
// enum menu { volume, gain, port14, port56, port710, treble, bass}; //Construct menu items!
// enum dir { up, down};

uint8_t dirUp = 0;    //This is only to define move of adjuste increment/decrement (0 or 1)
uint8_t dirDown = 0;  //This is only to define move of adjuste increment/decrement (0 or 1)
uint16_t upVal = 0x0000;
uint16_t downVal = 0x0000;
uint8_t menuItem = 0x01;  //I use a 8 bit to implement a count to allow define the quantity of items!

uint8_t bass = 0x00;
uint8_t treble = 0x00;
uint8_t gainTrebleBass = 0x00;  //This is the gain of trebel and bass, not confuse it with GAIN.
uint16_t volumeR = 0x001F;  //This is the low -dB of channel, position by default of 32 at scale.
uint16_t volumeL = 0x1F00;  //This is the low -dB of channel, position by default of 32 at scale.
uint8_t gain = 0x02;        //Default gain

uint8_t selectInput = 0x00;  //This is used to run over the 5 inputs possible of be selected!

uint8_t port14 = 0x00;   //This is used inside of menuAdjust to control ports!!!
uint8_t port56 = 0x00;   //This is used to control the 2 ports which support 3 status output!!!
uint8_t port710 = 0x01;  //This is used inside of menuAdjust to control ports!!!
uint8_t karaoke = 0x03;  // Definition of mono, stereo, vocal mono(L,R)

double wordA16 = 0x0000;  //Used to prepare the command letter A.
double wordB16 = 0x0000;  //Used to prepare the command letter B.
double wordC16 = 0x0000;  //Used to prepare the command letter C.
double wordD16 = 0x0000;  //Used to prepare the command letter D.

uint8_t wordA3 = 0x00;  //This will be address order of selection slot A (Last 3 bits of command)
uint8_t wordB3 = 0x01;  //This will be address order of selection slot B (Last 3 bits of command)
uint8_t wordC3 = 0x06;  //This will be address order of selection slot C (Last 3 bits of command)
uint8_t wordD3 = 0x07;  //This will be address order of selection slot D (Last 3 bits of command)

bool flagA = false;       //Used to avoid recursive calls, this will reduce noise in the sound, since 
bool flagB = false;       //I haven't implemented any filters! This is your responsibility!

//Variable belong to A command. It use 12 bit more 3 zeros and 3 bit of slot!
uint16_t select_Ctrl = 0x0001;  //Max = 0x05
uint16_t gain_Ctrl = 0x0008;    //Max = 0x08~0x38
uint16_t rec_Mute = 0x0020;     //Max = 0 or 1, 0 =rec mute Off, 1 = rec mute ON
uint16_t AD7 = 0x0000;          // This bit is allways "0", it is bit D7 on command "A"
uint16_t karaoke_Ctrl = 0x0300; // Default as STEREO
uint16_t bass_Ctrl = 0x0000;
uint16_t treble_Ctrl = 0x0000;

//Variable belong to B command. //It use 12 bit more 3 zeros and 3 bit of slot!
uint16_t port_output1_4 = 0x0000;   //O0, O1, O2, O3, port Open Collector Output
uint16_t port_output5_6 = 0x0000;   //Ss0, Ss1, St0, St1 Port 5 & 6. Port support 3tate Output
uint16_t port_output7_10 = 0x0000;  //O6, O7, O8, O9, port Open Collector Output. Pay attention they refer as 7~10
                                    // but do the count from 6 to 9!!!

//Variable belong to C command.
uint16_t lch_volume = 0x003F;  //L0, L1, L2, L3, L4, L5, L6. Att: Bit 5 & 6 are enabled at less than -36dB
uint16_t CD7 = 0x0080;         //this is the bit D7 of C commmand and stay with value "1".
uint16_t rch_volume = 0x3F00;  //R0, R1, R2, R3, R4, R5, R6. Att: Bit 5 & 6 are enabled at less than -36dB

//Variable belong to D command.
uint16_t bass_BoostCut = 0x0000;    //B0, B1, B2, B3, Set it to 0 to start at 0dB
uint16_t treble_BoostCut = 0x0000;  //T0, T1, T2, T3, Set it to 0 to start at 0dB
uint16_t bass_Gain = 0x0200;        //TB0, TB1, TB2. //I set it to -5dB: TB1 = 1; Look at page 10 on D-2.

void sendWithoutLatch(uint16_t a) {
  // send without stb
  uint16_t data = 0x0000;              //value to transmit, binary 10101010
  uint16_t mask = 0b0000000000000001;  //our bitmask

  data = a;
  //This don't send the strobe signal, to be used in burst data send
  for (mask = 0b0000000000000001; mask < 0b1000000000000000; mask <<= 1) {  //iterate through bit mask
    digitalWrite(VFD_clk, LOW);
    if (data & mask) {  // if bitwise AND resolves to true
      digitalWrite(VFD_data, HIGH);
      //Serial.print(1, BIN); //Only to debug, Comment Please!
    } else {  //if bitwise and resolves to false
      digitalWrite(VFD_data, LOW);
      //Serial.print(0, BIN); //Only to debug, Comment Please!
    }
    delayMicroseconds(5);
    digitalWrite(VFD_clk, HIGH);
    delayMicroseconds(5);
  }
  //digitalWrite(VFD_clk, LOW);
}
void sendSelectionSlot(uint8_t a) {
  // send without stb
  //This function send only the last 3 bit of the input format
  //selection slot!!! Bit 15, 16, 17 and is done by this way,
  //because 2 bytes only suport 16 bits. Other way is use as
  //a LONG variables to support 17 bits.
  uint16_t data = 0x0000;              //value to transmit, binary 10101010
  uint16_t mask = 0b0000000000000001;  //our bitmask

  data = a;
  //This don't send the strobe signal, to be used in burst data send
  for (mask = 0b0000000000000001; mask < 0b0000000000001000; mask <<= 1) {  //iterate through bit mask
    digitalWrite(VFD_clk, LOW);
    if (data & mask) {  // if bitwise AND resolves to true
      digitalWrite(VFD_data, HIGH);
      //Serial.print(1, BIN); //Only to debug, Comment Please!
    } else {  //if bitwise and resolves to false
      digitalWrite(VFD_data, LOW);
      //Serial.print(0, BIN); //Only to debug, Comment Please!
    }
    delayMicroseconds(5);
    digitalWrite(VFD_clk, HIGH);
    delayMicroseconds(5);
  }
  digitalWrite(VFD_clk, LOW);  //When data and clock are "H", can not receive the latch signal!!!
  delayMicroseconds(20);
  digitalWrite(VFD_data, LOW);  //When data and clock are "H", can not receive the latch signal!!!
  delayMicroseconds(20);
  digitalWrite(VFD_latch, HIGH);  //When data and clock are "H", can not receive the latch signal!!!
  delayMicroseconds(20);
  digitalWrite(VFD_latch, LOW);  //Return to LOW until next 17 bit be sended!
}
//////////////////////////////// This is actions on M62445 /////////////////////////////
void menuSelect(uint8_t value) {
  // Serial.print( "My number to MENU: ");  //Only to debug, comment it!
  // Serial.println(value, DEC);  //Only to debug, comment it!
  switch (value) {
    case 1:
      lcd.setCursor(4, 0);  //First Line of LCD
      lcd.print("Volume:     ");
      break;

    case 2:
      lcd.setCursor(4, 0);  //First Line of LCD
      lcd.print("Treble:     ");
      break;

    case 3:
      lcd.setCursor(4, 0);  //First Line of LCD
      lcd.print("Bass:       ");
      break;

    case 4:
      lcd.setCursor(4, 0);  //First Line of LCD
      lcd.print("Gain:      ");
      break;

    case 5:
      lcd.setCursor(4, 0);  //First Line of LCD
      lcd.print("SELECT INPUT:   ");
      break;

    case 6:
      lcd.setCursor(4, 0);  //First Line of LCD
      lcd.print("Karaoke Mode:   ");
      break;

    case 7:
      lcd.setCursor(4, 0);  //First Line of LCD
      lcd.print("Port 1~4:   ");
      break;

    case 8:
      lcd.setCursor(4, 0);  //First Line of LCD
      lcd.print("Port 5~6:   ");
      break;

    case 9:
      lcd.setCursor(4, 0);  //First Line of LCD
      lcd.print("Port 7~10:  ");
      break;

    default:;
      break;
  }
  //
}
void menuAdjust(uint8_t u, uint8_t d) {
  // Serial.print(u, DEC);
  // Serial.print(" : ");
  // Serial.println(d, DEC);
  //Here we do action over the option applied to the menuSelect()
  //If volume is on display moving the phaseB increase or decrease volume
  //Note I don't apply limits to avoid skip from max to min!!!
  //I don't use any memory to keep the value when power is down
  //this means the system return to the values present on declarition of variable
  //used to control volume!!!
  //Don't make distintion of Left channel and Right channel, adjust both channel with
  //same value!
  uint16_t value = menuItem;  //This variable pass to local the Volume, Gain, Port14, etc...!
  switch (value) {
    case 1:   // Volume
      if (u == 1) {
        //Use L0 up to L4 to ajust direct volume: 5 bits 1, 2, 4, 8, 16. This result as
        //a maximum of 32 values to volume without bit L5 & L6
        volumeR++;
        if (volumeR >= 31) {
          volumeR = 31; // 31 because the 0 will count as one position of max of gain!
        }
        // lch_volume = (0x0014 & 0x007F); //L0, L1, L2, L3, L4, L5, L6
        // rch_volume = (0x1400 & 0x7F00); //R0, R1, R2, R3, R4, R5, R6
        lch_volume = ((0x0000 | volumeR) & 0x003F);       //L0, L1, L2, L3, L4, L5, L6
        rch_volume = ((0x0000 | volumeR << 8) & 0x3F00);  //R0, R1, R2, R3, R4, R5, R6
      } else if (d == 1) {
        if (volumeR > 0) {
          volumeR--;
        } else if (volumeR <= 0) {
          volumeR = 0;
        }
        // lch_volume = (0x0014 & 0x007F); //L0, L1, L2, L3, L4, L5, L6
        // rch_volume = (0x1400 & 0x7F00); //R0, R1, R2, R3, R4, R5, R6
        lch_volume = ((0x0000 | volumeR) & 0x003F);       //L0, L1, L2, L3, L4, L5, L6
        rch_volume = ((0x0000 | volumeR << 8) & 0x3F00);  //R0, R1, R2, R3, R4, R5, R6
      }
      lcd.setCursor(4, 0);  //Second Line of LCD
      lcd.print("VOLUME:   ");
      lcd.setCursor(0, 3);  //Second Line of LCD
      lcd.print("Volume -dB      ");
      lcd.setCursor(12, 3);  //Second Line of LCD, write column 10
      lcd.print(volumeR);
      break;

    case 2:   // Bass 
        if (u == 1){
        bass++;
          if (bass >=11) {
            bass = 11;
          }
        // //inverteByte(bass);
        // treble_bass_Ctrl = ((0x0000 | bass ) & 0x000F);  //
      } else if (d == 1){
        if (bass > 0) {
          bass--;
        } else if (bass <= 0){
          bass = 0;
        }
        // //inverteByte(bass);
        // treble_bass_Ctrl = ((0x0000 | bass ) & 0x000F);  //
      }
      lcd.setCursor(4, 0);  //Second Line of LCD
      lcd.print("BASS:       ");
      lcd.setCursor(0, 3);  //Second Line of LCD
      lcd.print("Bass:           ");
      lcd.setCursor(8, 3);  //Second Line of LCD
      
      switch (bass) {
        case 0:
          bass_Ctrl = ((0x0000 | 0x000D) & 0x000F);
          lcd.print("+10dB");
          break;  //
        case 1:
          bass_Ctrl = ((0x0000 | 0x000C) & 0x000F);
          lcd.print("+8dB");
          break;  //
        case 2:
          bass_Ctrl = ((0x0000 | 0x000B) & 0x000F);
          lcd.print("+6dB");
          break;  //
        case 3:
          bass_Ctrl = ((0x0000 | 0x000A) & 0x000F);
          lcd.print("+4dB");
          break;  //
        case 4:
          bass_Ctrl = ((0x0000 | 0x0009) & 0x000F);
          lcd.print("+2dB");
          break;  //
        case 5:
          bass_Ctrl = ((0x0000 | 0x0008) & 0x000F);
          lcd.print("0dB");
          break;  //
        case 6:
          bass_Ctrl = ((0x0000 | 0x0000) & 0x000F);
          lcd.print("0dB");
          break;  //
        case 7:
          bass_Ctrl = ((0x0000 | 0x0001) & 0x000F);
          lcd.print("-2dB");
          break;  //
        case 8:
          bass_Ctrl = ((0x0000 | 0x0002) & 0x000F);
          lcd.print("-4dB");
          break;  //
        case 9:
          bass_Ctrl = ((0x0000 | 0x0003) & 0x000F);
          lcd.print("-6dB");
          break;  //
        case 10:
          bass_Ctrl = ((0x0000 | 0x0004) & 0x000F);
          lcd.print("-8dB");
          break;  //
        case 11:
          bass_Ctrl = ((0x0000 | 0x0005) & 0x000F);
          lcd.print("-10dB");
          break;  //  
        default:;
        break;
      }
      break;
 
    case 3:   // Treble 
       if (u == 1) {
        treble++;
        if (treble >=11) {
          treble = 11;
        }
        // //inverteByte(treble);
        // treble_treble_Ctrl = ((0x0000 | treble ) & 0x000F);  //
      } else if (d == 1) {
        if (treble > 0) {
          treble--;
        } else if (treble <= 0) {
          treble = 0;
        }
        // //inverteByte(treble);
        // treble_treble_Ctrl = ((0x0000 | treble ) & 0x000F);  //
      }
      lcd.setCursor(4, 0);  //Second Line of LCD
      lcd.print("TREBLE:     ");
      lcd.setCursor(0, 3);  //Second Line of LCD
      lcd.print("treble:         ");
      lcd.setCursor(8, 3);  //Second Line of LCD
      
      switch (treble) {
        case 0:
          treble_Ctrl = ((0x0000 | 0x00D0) & 0x00F0);
          lcd.print("+10dB");
          break;  //
        case 1:
          treble_Ctrl = ((0x0000 | 0x00C0) & 0x00F0);
          lcd.print("+8dB");
          break;  //
        case 2:
          treble_Ctrl = ((0x0000 | 0x00B0) & 0x00F0);
          lcd.print("+6dB");
          break;  //
        case 3:
          treble_Ctrl = ((0x0000 | 0x00A0) & 0x00F0);
          lcd.print("+4dB");
          break;  //
        case 4:
          treble_Ctrl = ((0x0000 | 0x0090) & 0x00F0);
          lcd.print("+2dB");
          break;  //
        case 5:
          treble_Ctrl = ((0x0000 | 0x0080) & 0x00F0);
          lcd.print("0dB");
          break;  //
        case 6:
          treble_Ctrl = ((0x0000 | 0x0000) & 0x00F0);
          lcd.print("0dB");
          break;  //
        case 7:
          treble_Ctrl = ((0x0000 | 0x0010) & 0x00F0);
          lcd.print("-2dB");
          break;  //
        case 8:
          treble_Ctrl = ((0x0000 | 0x0020) & 0x00F0);
          lcd.print("-4dB");
          break;  //
        case 9:
          treble_Ctrl = ((0x0000 | 0x0030) & 0x00F0);
          lcd.print("-6dB");
          break;  //
        case 10:
          treble_Ctrl = ((0x0000 | 0x0040) & 0x00F0);
          lcd.print("-8dB");
          break;  //
        case 11:
          treble_Ctrl = ((0x0000 | 0x0050) & 0x00F0);
          lcd.print("-10dB");
          break;  //  
        default:;
          break;
      }
      break;
  
    case 4:   // Gain
      if (u == 1) {
        gain++;
        if (gain > 7) {
          gain = 7;
        }
        inverteByte(gain);
        gain_Ctrl = ((0x0000 | gain << 3) & 0x0038);  //Max = 0x04~0x07: G0 to G2
      } else if (d == 1) {
        if (gain > 0) {
          gain--;
        } else if (gain <= 0) {
          gain = 0;
        }
        inverteByte(gain);
        gain_Ctrl = ((0x0000 | gain << 3) & 0x0038);  //Max = 0x04~0x07: G0 to G2
      }
      lcd.setCursor(4, 0);  //Second Line of LCD
      lcd.print("GAIN:     ");
      lcd.setCursor(0, 3);  //Second Line of LCD
      lcd.print("Gain:           ");
      lcd.setCursor(8, 3);  //Second Line of LCD
      lcd.print(gain);
      break;

    case 5:   // Input
      if (u == 1) {
        selectInput++;
        if (selectInput > 5) {
          selectInput = 5;
        }
        inverteByte(selectInput);
        //In the next line, move 5 bit to right because I have inverted the byte and only passe 3 bits of this byte!
        select_Ctrl = ((0x0000 | selectInput) & 0x0007);  //Max = 0x05: I0 to I2, but is neccessary invert the byte!!!
      } else if (d == 1) {
        if (selectInput > 0) {
          selectInput--;
        } else if (selectInput <= 0) {
          selectInput = 0;
        }
        inverteByte(selectInput);
        select_Ctrl = ((0x0000 | selectInput >> 5) & 0x0007);  //Max = 0x05: I0 to I2, but is neccessary invert the byte!!!
      }
      lcd.setCursor(4, 0);  //Second Line of LCD
      lcd.print("INPUT:      ");
      lcd.setCursor(0, 3);  //Second Line of LCD
      switch (selectInput) {
        case 0: lcd.print("       OFF      "); break;  //Pay attention to the relation between the order channel and bit weight!!!
        case 1: lcd.print("INPUT: D        "); break;  //Pay attention to the relation between the order channel and bit weight!!!
        case 2: lcd.print("INPUT: B        "); break;  //Pay attention to the relation between the order channel and bit weight!!!
        case 3: lcd.print("INPUT: C        "); break;  //Pay attention to the relation between the order channel and bit weight!!!
        case 4: lcd.print("INPUT: A        "); break;  //Pay attention to the relation between the order channel and bit weight!!!
        case 5: lcd.print("INPUT: E        "); break;  //Pay attention to the relation between the order channel and bit weight!!!
        default:;
        break;
      }
      break;

    case 6:   // Karaoke
      if (u == 1) {
        karaoke = (karaoke << 1);
        if (karaoke >= 5) {
          karaoke = 5;
        }
      } else if (d == 1) {
        if (karaoke > 1) {
          karaoke = (karaoke >> 1);
        } else if (karaoke <= 1) {
          karaoke = 1;
        }
      }
      lcd.setCursor(4, 0);  //Second Line of LCD
      lcd.print(" KARAOKE    ");
      lcd.setCursor(0, 3);  //Second Line of LCD
      switch (karaoke) {
        case 1:
          karaoke_Ctrl = ((0x0000 | 0x0300) & 0x0F00);
          lcd.print("     STEREO     ");
          break;  //
        case 2:
          karaoke_Ctrl = ((0x0000 | 0x0F00) & 0x0F00);
          lcd.print("   MONO(L+R)    ");
          break;  //
        case 3:
          karaoke_Ctrl = ((0x0000 | 0x0D00) & 0x0F00);
          lcd.print("     L  MPX     ");
          break;  //
        case 4:
          karaoke_Ctrl = ((0x0000 | 0x0E00) & 0x0F00);
          lcd.print("    R  MPX      ");
          break;  //
        case 5:
          karaoke_Ctrl = ((0x0000 | 0x0B00) & 0x0F00);
          lcd.print("Vocal cut (L-R) ");
          break;  //
        default:;
        break;
      } 
      break;
         
    case 7:   // Port14
      if (u == 1) {
        port14++;
        if (port14 >= 15) {
          port14 = 15;
        }
        port_output1_4 = ((0x0000 | port14) & 0x000F);  //0x0002; //O0, O1, O2, O3, port Open Collector Output
      } else if (d == 1) {
        if (port14 > 0) {
          port14--;
        } else if (port14 <= 0) {
          port14 = 0;
        }
        port_output1_4 = ((0x0000 | port14) & 0x000F);  //0x0002; //O0, O1, O2, O3, port Open Collector Output
      }
      lcd.setCursor(4, 0);  //Second Line of LCD
      lcd.print("Port 1-4:   ");
      lcd.setCursor(0, 3);  //Second Line of LCD
      lcd.print("Port 1-4:       ");
      lcd.setCursor(10, 3);  //Second Line of LCD, write column 10
      lcd.print(port14);
      break;

    case 8:   // Port56
      if (u == 1) {
        port56++;
        if (port56 >= 15) {
          port56 = 15;
        }
        port_output5_6 = ((0x0000 | port56 << 4) & 0x00F0);  //0x0002; //O0, O1, O2, O3, port Open Collector Output
      } else if (d == 1) {                                   //Note the port 5 is controlled by 2 bits, port 6 also controlled by 2 bits! (Ss0, Ss1 : St0, St1)
        if (port56 > 0) {                                    //This means port 5 will bright with 0x3, port 6 will bight with 0xC
          port56--;
        } else if (port56 <= 0) {
          port56 = 0;
        }
        port_output5_6 = ((0x0000 | port56 << 4) & 0x00F0);  //0x0002; //Ss0, Ss1, St0, St1 Port 5 & 6.
      }
      lcd.setCursor(4, 0);  //Second Line of LCD
      lcd.print("PORT 5, 6:  ");
      lcd.setCursor(0, 3);  //Second Line of LCD
      lcd.print("Port 5 & 6:     ");
      lcd.setCursor(13, 3);  //Second Line of LCD, write column 13
      lcd.print(port56);
      break;

    case 9:   // Port710
      if (u == 1) {
        port710 = (port710 << 1);
        if (port710 >= 8) {
          port710 = 8;
        }
        port_output7_10 = ((0x0000 | port710 << 8) & 0x0F00);  //0x0002; //O0, O1, O2, O3, port Open Collector Output
      } else if (d == 1) {
        if (port710 > 1) {
          port710 = (port710 >> 1);
        } else if (port710 <= 1) {
          port710 = 1;
        }
        port_output7_10 = ((0x0000 | port710 << 8) & 0x0F00);  //0x0002; //O0, O1, O2, O3, port Open Collector Output
      }
      lcd.setCursor(4, 3);  //Second Line of LCD
      lcd.print("PORT 7-10:  ");
      lcd.setCursor(0, 3);  //Second Line of LCD
      lcd.print("Port 7-10:      ");
      lcd.setCursor(10, 3);  //Second Line of LCD, write column 10
      lcd.print(port710);
  }
  dirUp = 0;                          //Let it again with value "0"
  dirDown = 0;                        //Let it again with value "0"
  //Case you need analize anything you have doubts about values, uncoment the one or two lines of print
  //and if necessary put the value you want if not present on next lines.
  //Keep allways a low number of print lines, they can block the code!!!
  // Serial.print("Your Select: ");            // Only to debug!!! Comment Please!
  // Serial.println(karaoke_Ctrl, BIN);  // Only to debug!!! Comment Please!
  // Serial.print("Volume L: "); // Only to debug!!! Comment Please!
  // Serial.println(lch_volume, BIN); // Only to debug!!! Comment Please!
  // Serial.print("Volume R: "); // Only to debug!!! Comment Please!
  // Serial.println(rch_volume, BIN); // Only to debug!!! Comment Please!
  constructAll();  //Construct the commands A, B, C & D to be sent!
}
uint8_t inverteByte(uint8_t b) {
  //This is used to invert the byte used to define the bits belongs to the input selector!!!
  //because they are inverted order count of 1 up to 5! Inverte the byte solve the issue!
  uint8_t inverted = 0;
  for (uint8_t i = 0; i < 8; i++) {
    inverted <<= 1;
    inverted |= b & 1;
    b >>= 1;
  }
  return inverted;
}
////////////////////////////////// creat A /////////////////////////////////////
void inSelectorCtrl() {
  //Input Selector control bits
  //Very importante see the options is from "0" up to "5", but have a problem because they do the prohibition
  //of value "3" and "7"! Please pay attention, because if you invert the byte the problem is more easy to solve
  //because now you can count from 0 to 5 in normal way!
  //select_Ctrl = 0x0001; //Max = 0x05: I0 to I2  // This is now defined on the menuAdjust!
}
void gainCtrl() {
  //Gain control
  //gain_Ctrl = 0x0030; //Max = 0x04~0x07: G0 to G2 // This is now defined on the menuAdjust!
}
void recMuteCtrl() {
  //rec Mute on or off
  rec_Mute = 0x0000;  //Possible = 0 or 1, 0 =rec mute Off, 1 = rec mute ON
}
void ad7Ctrl() {
  AD7 = 0x0000;
}
void karaokeCtrl() {
  //karaoke_Ctrl = 0x0300;  //Stereo // This is now defined on the menuAdjust!
}
////////////////////////////////// creat B /////////////////////////////////////
void portOutput14() {
  //port_output1_4 = 0x0002; //O0, O1, O2, O3, port Open Collector Output // This is now defined on the menuAdjust!
}
void portOutput56() {
  //port_output5_6 = 0x00F0; //Ss0, Ss1, St0, St1 Port 5 & 6. // This is now defined on the menuAdjust!
}
void portOutput710() {
  //port_output7_10 = 0x0200; //O6, O7, O8, O9, port Open Collector Output. // This is now defined on the menuAdjust!
}
////////////////////////////////// creat C /////////////////////////////////////
void lchVolume() {
  //lch_volume = 0x0014; //L0, L1, L2, L3, L4, L5, L6 // This is now defined on the menuAdjust!
}
void cd7Ctrl() {
  CD7 = 0x0080;  //Note the bit 7 of command C is allways with value "1";
}
void rchVolume() {
  //rch_volume = 0x1400; //R0, R1, R2, R3, R4, R5, R6 // This is now defined on the menuAdjust!
}
////////////////////////////////// creat D /////////////////////////////////////
void bassBoostCut() {
 //  bass_Ctrl = 0x000D;  //B0, B1, B2, B3 // This is now defined on the menuAdjust!
}
void trebleBoostCut() {
 //  treble_Ctrl = 0x00D0;  //T0, T1, T2, T3 // This is now defined on the menuAdjust!
}
void bassGain() {
 //  bass_Gain = 0x0700;  //TB0, TB1, TB2 //This control all Gain Bass and Treble 7= 14dB // This is now defined on the menuAdjust!
}
//////////////////////////////// End creat X ///////////////////////////////////
//
//////////////////////// Zone of Construct commands ////////////////////////////
///////////////////////////     constructA   ///////////////////////////////////
void constructA() {
  uint16_t sendA = 0x8000;
  inSelectorCtrl();
  gainCtrl();
  recMuteCtrl();
  ad7Ctrl();
  karaokeCtrl();

  sendA = (sendA | select_Ctrl);  // & 0x0007);  // Select the port: A:E, please pay attention to the binary Order, very imporante! Wheigt bit is inverted!
  sendA = (sendA | gain_Ctrl);  //Gain control
  sendA = (sendA | rec_Mute); //
  sendA = (sendA | AD7);  //need be zero!!! //bit D7 keep the "0" value, don't need do operation!
  sendA = (sendA | karaoke_Ctrl); //Select the combine of audio channel... active Stereo to both channels!
  //Bit d12, d13, d14 keep the value of "0"!
  //Now to finish the construction of command A, need send the last 3 zeros of input format selection slot!
  //Serial.println(sendA, BIN);  //Only to debug, Comment Please!
  // Serial.println("-Next line is 14 bits of data A: " ); //Only to debug, Comment Please!
  sendWithoutLatch(sendA);
  // Serial.println(); //Only to debug, Comment Please!
  // Serial.println("-last 3 bits of A: "); //Only to debug, Comment Please!
  sendSelectionSlot(wordA3);
  // Serial.println(); //Only to debug, Comment Please!
}
///////////////////////////     constructB   ///////////////////////////////////
void constructB() {
  uint16_t sendB = 0x8000;
  portOutput14();
  portOutput56();
  portOutput710();

  sendB = (sendB | port_output1_4);
  sendB = (sendB | port_output5_6);
  sendB = (sendB | port_output7_10);

  //Bit d12, d13, d14 keep the value of "0"!
  //Now to finish the construction of command B, need send the last 3 zeros of input format selection slot!
  // Serial.println(sendB, BIN); //Only to debug, Comment Please!
  // Serial.println("-Next line is 14 bits of data B: " );
  sendWithoutLatch(sendB);
  // Serial.println(); //Only to debug, Comment Please!
  // Serial.println("-last 3 bits of B: "); //Only to debug, Comment Please!
  sendSelectionSlot(wordB3);
  // Serial.println(); //Only to debug, Comment Please!
}
///////////////////////////     constructC   ///////////////////////////////////
void constructC() {
  uint16_t sendC = 0x8000;
  //lchVolume();  //This is to create the default value to volume, now comming already from menuAdjust().
  cd7Ctrl();
  //rchVolume();

  sendC = (sendC | lch_volume);  // & 0x0007);
  sendC = (sendC | CD7);
  sendC = (sendC | rch_volume);
  //Bit d12, d13, d14 keep the value of "0"!
  //Now to finish the construction of command C, need send the last 3 zeros of input format selection slot!
  // Serial.println(sendC, BIN); //Only to debug, Comment Please!
  // Serial.println("-Next line is 14 bits of data C: " ); //Only to debug, Comment Please!
  sendWithoutLatch(sendC);
  // Serial.println(); //Only to debug, Comment Please!
  // Serial.println("-last 3 bits of C: "); //Only to debug, Comment Please!
  sendSelectionSlot(wordC3);
  //Serial.println(); //Only to debug, Comment Please!
}
///////////////////////////     constructD   ///////////////////////////////////
void constructD() {
  uint16_t sendD = 0x8000;
  bassBoostCut();
  trebleBoostCut();
  bassGain();

  sendD = (sendD | bass_Ctrl);
  sendD = (sendD | treble_Ctrl);
  sendD = (sendD | bass_Gain);
  //Bit d12, d13, d14 keep the value of "0"!
  //Now to finish the construction of command D, need send the last 3 zeros of input format selection slot!
   Serial.println(sendD, BIN); //Only to debug, Comment Please!
  // Serial.println("-Next line is 14 bits of data D: " ); //Only to debug, Comment Please!
  sendWithoutLatch(sendD);
  // Serial.println(); //Only to debug, Comment Please!
  // Serial.println("-last 3 bits of D: "); //Only to debug, Comment Please!
  sendSelectionSlot(wordD3);
  // Serial.println(); //Only to debug, Comment Please!
}
/////////////////////////// End construct cmds  ////////////////////////////////
//
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(VFD_data, OUTPUT);
  pinMode(VFD_clk, OUTPUT);
  pinMode(VFD_latch, OUTPUT);

  pinMode(phaseA, INPUT_PULLUP);  //I need apply a internal pull-up, or apply it as extenal.
  pinMode(phaseB, INPUT_PULLUP);  //I need apply a internal pull-up, or apply it as extenal.
  pinMode(4, INPUT_PULLUP);       //This pin is used as complement of pin 2. Pin 2 with pin 4
  pinMode(5, INPUT_PULLUP);       //This pin is used as complement of pin 3. Pin 3 with pin 5
  pinMode(12, OUTPUT);            // This pin is used to indicate the trigger event on pin 2 or 3 through one LED
  pinMode(11, OUTPUT);            // This pin is used to indicate the trigger event on pin 2 or 3 through one LED

  //We can use trigger event like: RISING, FAILLING or CHANGE.
  //This depend of hardware choice, Resistor Pull Up, Pull Down or both transitions!
  attachInterrupt(digitalPinToInterrupt(phaseA), detectA, RISING);  //enables external Interrupt at pin of phaseA (Arduino Uno can only use pin 2 and 3)
  attachInterrupt(digitalPinToInterrupt(phaseB), detectB, RISING);  //enables external Interrupt at pin of phaseB (Arduino Uno can only use pin 2 and 3)

  //Dont insert any print inside of interrupt function!!!
  //If you run the search function, please active the terminal to be possible print lines,
  //Other way the run will be blocked!
  //scanBUS(); // To find address of LCD we are using connected to SDA/SCL!!!
  lcd.init();  // initialize the lcd
}
void constructAll() {
  //This construct all 4 groups of settings to send, cmd A, cmd B, cmd C, cmd D.
  constructA();
  delay(10);
  constructB();
  delay(10);
  constructC();
  delay(10);
  constructD();
  delay(10);
}
void loop() {
  // put your main code here, to run repeatedly:
  // uint8_t counter = 0x00;
  // Print a message to the LCD.
  lcd.backlight();
  lcd.setCursor(0, 0);  //First Line of LCD
  lcd.print("     M62445     ");
  // lcd.setCursor(2,1);  //Third Line of LCD
  // lcd.print(" MENU: ");
  lcd.setCursor(0, 3);  //Second Line of LCD
  lcd.print("      MENU      ");
  // lcd.setCursor(2,3); //Fourth Line of LCD
  // lcd.print("............");
  constructAll();  //Apply values by default we have defined to the 4 commands (A:D).
                   //
  while (1) {
    // The next 7 lines is only to schou a count action on display, pleas comment it 
    // to avoid the noise in audio by the constant send of messages through port I2C (sda/sck)
    // counter++;
    // lcd.setCursor(0, 0);
    // lcd.print(counter);
    // if (counter == 255) {  //Note we used a byte (only 8 bits to count from 0 up to 255 in binary, the max is 256 but 0 is one by this reason 255!!!)
    //   lcd.setCursor(0, 0);
    //   lcd.print("    ");  //Only to clean the info on the column where werite 3 digits of count finished!
    // }
    if (flagA){ //I use this flag to avoid constantly sending data to the screen and 
    //causing audio noise, since I don't apply filters in the setup for that!
    menuSelect(menuItem);        //This is responsable to select the item of MENU!
                                 // is controlled by the trigger of knob button of quadrature phaseA.
    menuAdjust(dirUp, dirDown);  //This do interaction with the item of MENU selected to adjuste the
                                 //value! exp: Volume, Ports, Input selector... etc! Is activated by the
                                 //trigger of knob button of quadrature phaseB.
                                 //
      flagB = false;                             
      flagA = false;
    }
    if(flagB){  //I use this flag to avoid constantly sending data to the screen and 
    //causing audio noise, since I don't apply filters in the setup for that!
    menuSelect(menuItem);        //This is responsable to select the item of MENU!
                                 // is controlled by the trigger of knob button of quadrature phaseA.
    menuAdjust(dirUp, dirDown);  //This do interaction with the item of MENU selected to adjuste the
                                 //value! exp: Volume, Ports, Input selector... etc! Is activated by the
                                 //trigger of knob button of quadrature phaseB.
                                 //
      flagB = false;                             
      flagA = false;
    }
    delay(250);
    
  }  //End and Close of cycle While(1)
}
void detectA() {
  //This trigger event will be responsable by the MENU options!
  //ISR for phase A
  //bitRead(PIND, 3) is probably the same as direct port manipulation
  //byte p3 = PIND&(1<<3); // read Arduino Uno Digital Pin 3 as fast as possible
  if (bitRead(PIND, 4) == LOW) {  // check other phase for determine direction
    menuItem = menuItem + 1;
    digitalWrite(11, !digitalRead(11));
  } else {
    menuItem = menuItem - 1;
    digitalWrite(12, !digitalRead(12));
  }
  if (menuItem <= 1) {
    menuItem = 1;
  } else if (menuItem >= 9) {  //Adjust the number of items you want use! Of course if you change this set, you 
    menuItem = 9;              // need also change on the menuSelect(); and menuAdjuste();
  }
  // Serial.println(menuItem, BIN); // Only to debug!!!
  flagA = true;
}
void detectB() {
  //This trigger event will be responsable by move UP & DOWN of adjustes!
  //ISR for phase B
  //bitRead(PIND, 5) is probably the same as direct port manipulation
  //byte p3 = PIND&(1<<5); // read Arduino Uno Digital Pin 3 as fast as possible
  if (bitRead(PIND, 5) == LOW) {  // check other phase for determine direction
    dirUp = 1;
    dirDown = 0;
    digitalWrite(11, !digitalRead(11));
  } else {
    dirUp = 0;
    dirDown = 1;
    digitalWrite(12, !digitalRead(12));
  }
  flagB = true;
}
