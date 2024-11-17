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
  word a = 12438;
  word b = 44972;
  //Serial.print(a += b);
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
  char cmd[8] = { "0" };
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
  executeCommand(cmd);

}  // END readCommand()

// function to perform a specific command
void executeCommand(char *cmd) {

  if (strcmp(cmd, "#SCSIZE$") == 0) {
    // get sector size used for dumping ROM
    Serial.print('#');
    write32(sector_size);
    Serial.print('$');
    Serial.flush();
  } else if (strcmp(cmd, "#HEADER$") == 0) {
    // get first 64 bytes which are the ROM header
    //Serial.println("#ABCDEFG$");
    readRom(rom_base, 64);
  } else if (strncmp(cmd, "#SC", 3) == 0) {
    // get a specific sector from the ROM
    byte bytes[4] = {cmd[6], cmd[5], cmd[4], cmd[3]};
    uint32_t *sector = (uint32_t *)bytes;
    readRom(rom_base + *sector, sector_size);
    //Serial.print('#');
    //Serial.print(*sector);
    //Serial.println('$');
  } else if (strncmp(cmd, "#DUMP", 5) == 0) {
    // send dump size in bytes
    uint32_t n = uint32_t(10) * (cmd[5] - 48) + (cmd[6] - 48);
    uint32_t dump_size = n * MB;
    Serial.print('#');
    write32(dump_size);
    Serial.print('$');
  
    // send rom contents
    for (uint32_t addr = rom_base; addr < rom_base + dump_size; addr += sector_size) {
      readRom(addr, sector_size);
      break;
    }

  } else Serial.println("#ERRO$");

}

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
    if (true) {
      checksum = w + checksum;
    }

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

}

// function to send dummy packet instead of real data packet
char *dummyPacket(void) {

  Serial.print('#');
  for (uint32_t i=0; i<sector_size; i++)
    Serial.write(i % 8);
  Serial.print('$');

} // END dummyPacket()

void write32(uint32_t val) {

    // write all 4 bytes starting with most significant
    Serial.write(val >> 24);
    Serial.write(val >> 16);
    Serial.write(val >> 8);
    Serial.write(val);

}

void setup_N64_Cart() {

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