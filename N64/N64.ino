// computer communicates by sending 8 byte commands.
// first byte must be '#', last must be '$'

// responses from arduino also begin with '#' and end with '$'

uint32_t MB = 1048576; // 1 megabyte
uint32_t sector_size = 512; // sector size used to transfer data back to PC
uint32_t rom_base = 0x10000000; // rom base address

void setup() {

  // prepare cartidge for reading
  setup_N64_Cart();

  // start serial connection at 2 Mbaud
  Serial.begin(500000);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }
  Serial.print("#INIT$");
  Serial.flush();

}  // END setup()

void loop() {
  readCommand();
  delay(1);
}  // END loop()

// function to read commands from serial input
void readCommand(void) {

  // loop to get command from serial input
  int cnt = 0;
  char cmd[9] = { "\0" };
  while (true) {
    // get incoming serial comm.
    if (Serial.available()) cmd[cnt++] = Serial.read();

    // return if command exceeds length
    if (cnt > 8) {
      Serial.println("#BADC$");
      break;
    }

    // break & continue if valid command
    if (cnt > 0 && cmd[0] == '#' && cmd[7] == '$') break;
  }

  // parse the command
  if (strcmp(cmd, "#SCSIZE$") == 0) {
    // get sector size used for dumping ROM
    Serial.print('#');
    write32(sector_size);
    Serial.print('$');
    Serial.flush();
  } else if (strcmp(cmd, "#HEADER$") == 0) {
    // get first 64 bytes which are the ROM header
    readRom(rom_base, 64);
  } else if (strncmp(cmd, "#SC", 3) == 0) {
    // get a specific sector from the ROM
    byte bytes[4] = {cmd[6], cmd[5], cmd[4], cmd[3]};
    uint32_t *sector = (uint32_t *)bytes;
    readRom(rom_base + *sector, sector_size);
  } else Serial.println("#ERRO$");

} // END readCommand()

void readRom(uint32_t addr, int size) {

  // set the address
  setAddress_N64(addr);

  // send the requested size of data
  Serial.print('#');
  word checksum = 0;
  for (int c = 0; c < size; c += 2) {
    // get word
    word w = readWord_N64();
    // update checksum
    checksum = w + checksum;
    // write to serial
    Serial.write(highByte(w));
    Serial.write(lowByte(w));
  }
  // Pull ale_H(PC1) high
  PORTC |= (1 << 1);
  // write checksum
  Serial.write(highByte(checksum));
  Serial.write(lowByte(checksum));
  Serial.print('$');
  Serial.flush();

} // END readRom()

void write32(uint32_t val) {

    // write all 4 bytes starting with most significant
    Serial.write(val >> 24);
    Serial.write(val >> 16);
    Serial.write(val >> 8);
    Serial.write(val);

} // END write32()

void setup_N64_Cart() {

  // pinout
  //
  //   GND |  1 | GND                   GND | 26 | GND
  //   GND |  2 | GND                   GND | 27 | GND
  //   A15 |  3 | AD15                  AD0 | 28 | A0
  //   A14 |  4 | AD14                  AD1 | 29 | A1
  //   A13 |  5 | AD13                  AD2 | 30 | A2
  //   GND |  6 | GND                   GND | 31 | GND
  //   A12 |  7 | AD12                  AD3 | 32 | A3
  //    D8 |  8 | WRITE               ALE_L | 33 | D37
  //   VCC |  9 | VCC                   VCC | 34 | VCC
  //    D9 | 10 | READ                ALE_H | 35 | D36
  //   A11 | 11 | AD11                  AD4 | 36 | A4
  //   A10 | 12 | AD10                  AD5 | 37 | A5
  //    NC | 13 | 12V                   12V | 38 | NC
  //    NC | 14 | NC                     NC | 39 | NC
  //    A9 | 15 | AD9                   AD6 | 40 | A6
  //    A8 | 16 | AD8                   AD7 | 41 | A7
  //   VCC | 17 | VCC                   VCC | 42 | VCC
  //    NC | 18 | CIC_to_PIF     PIF_to_CIC | 43 | NC
  //    NC | 19 | EEPROM_CLK      R4300_CLK | 44 | NC
  //   D17 | 20 | RESET                 NMI | 45 | NC
  //    D7 | 21 | EEPROM_DATA     Video_CLK | 46 | NC
  //   GND | 22 | GND                   GND | 47 | GND
  //   GND | 23 | GND                   GND | 48 | GND
  //    NC | 24 | L_Audio           R_Audio | 49 | NC
  //   GND | 25 | GND                   GND | 50 | GND

  // NC = No Connect
  // A0-A7 corresponds to Port F
  // A8-A15 corresponds to Port K
  // D17 (Port H, 0) = RESET
  // D7  (Port H, 4) = EEPROM_DATA
  // D8  (Port H, 5) = WRITE
  // D9  (Port H, 6) = READ
  // D37 (Port C, 0) = ALE_L
  // D36 (Port C, 1) = ALE_H

  // Set A0-A7 Pins to Output and set them low
  DDRF = 0xFF;
  PORTF = 0x00;
  // Set A8-A15 Pins to Output and set them low
  DDRK = 0xFF;
  PORTK = 0x00;
  // Set Control Pins to Output RESET(PH0) WR(PH5) RD(PH6) aleL(PC0) aleH(PC1)
  DDRH |= (1 << 0) | (1 << 5) | (1 << 6);
  DDRC |= (1 << 0) | (1 << 1);
  // Pull RESET(PH0) low until we are ready
  PORTH &= ~(1 << 0);
  // Output a high signal on WR(PH5) RD(PH6), pins are active low therefore everything is disabled now
  PORTH |= (1 << 5) | (1 << 6);
  // Pull aleL(PC0) low and aleH(PC1) high
  PORTC &= ~(1 << 0);
  PORTC |= (1 << 1);

  // Set Eeprom Data Pin(PH4) to Input
  DDRH &= ~(1 << 4);

  // Wait until all is stable
  delay(300);

}

// Switch Cartridge address/data pins to output
void adOut_N64() {
  //A0-A7
  DDRF = 0xFF;
  PORTF = 0x00;
  //A8-A15
  DDRK = 0xFF;
  PORTK = 0x00;
}

// Switch Cartridge address/data pins to read
void adIn_N64() {
  //A0-A7
  DDRF = 0x00;
  //A8-A15 
  DDRK = 0x00;
}

// Read one word out of the cartridge
word readWord_N64() {
  // Pull read(PH6) low
  PORTH &= ~(1 << 6);
  
  // Wait ~310ns
  __asm__("nop\n\t"
          "nop\n\t"
          "nop\n\t"
          "nop\n\t"
          "nop\n\t");

  // Join bytes from PINF and PINK into a word
  word tempWord = ((PINK & 0xFF) << 8) | (PINF & 0xFF);

  // Pull read(PH6) high
  PORTH |= (1 << 6);

  return tempWord;
}

// Set Cartridge address
void setAddress_N64(unsigned long myAddress) {
  // Set address pins to output
  adOut_N64();
  
  // Split address into two words
  word myAdrLowOut = myAddress & 0xFFFF;
  word myAdrHighOut = myAddress >> 16;
  
  // Switch WR(PH5) RD(PH6) ale_L(PC0) ale_H(PC1) to high (since the pins are active low)
  PORTH |= (1 << 5) | (1 << 6);
  PORTC |= (1 << 1);
  __asm__("nop\n\t"); // needed for repro
  PORTC |= (1 << 0);

  // Output high part to address pins
  PORTF = myAdrHighOut & 0xFF;
  PORTK = (myAdrHighOut >> 8) & 0xFF;
    
  // Leave ale_H high for additional 62.5ns
  __asm__("nop\n\t");

  // Pull ale_H(PC1) low
  PORTC &= ~(1 << 1);

  // Output low part to address pins
  PORTF = myAdrLowOut & 0xFF;
  PORTK = (myAdrLowOut >> 8) & 0xFF;

  // Leave ale_L high for ~125ns
  __asm__("nop\n\t"
          "nop\n\t");

  // Pull ale_L(PC0) low
  PORTC &= ~(1 << 0);

  // Set data pins to input
  adIn_N64();
}