// computer communicates by sending 8 byte commands.
// first byte must be '#', last must be '$'

// responses from arduino also begin with '#' and end with '$'

// works with MBC5 cartridges (for now)

uint32_t sector_size = 512; // sector size used to transfer data back to PC

void setup() {

  // prepare cartidge for reading
  setup_GB_Cart();

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

  // parse the command and execute
  if (strcmp(cmd, "#SCSIZE$") == 0) {
    // get sector size used for dumping ROM
    Serial.print('#');
    write32(sector_size);
    Serial.print('$');
    Serial.flush();
  } else if (strcmp(cmd, "#HEADER$") == 0) {
    // get ROM header bytes
    readRom(0x100, 80);
  } else if (strncmp(cmd, "#SC", 3) == 0) {
    // get a sector from the ROM starting at addr;
    uint32_t addr = read32(cmd);
    readRom(addr, sector_size);
  } else if (strncmp(cmd, "#BK", 3) == 0) {
    // set memory bank
    uint32_t bank = read32(cmd);
    writeByte_GB(0x2100, bank & 0xFF); // for MBC5
  } else Serial.println("#ERRO$");

}  // END readCommand()

void write32(uint32_t val) {

    // write all 4 bytes starting with most significant
    Serial.write(val >> 24);
    Serial.write(val >> 16);
    Serial.write(val >> 8);
    Serial.write(val);

} // END write32()

uint32_t read32(char *cmd) {

  // read 4 bytes from command payload
  byte bytes[4] = {cmd[6], cmd[5], cmd[4], cmd[3]};
  uint32_t *ptr = (uint32_t *)bytes;

  return *ptr;

} // END read32()

void readRom(uint32_t addr, int size) {

  // send the requested size of data
  Serial.print('#');
  word checksum = 0;
  for (uint32_t i = addr; i < addr + size; i++) {
    // write to serial
    byte b = readByte_GB(i);
    Serial.write(b);

    // update checksum
    if (true)
      checksum = b + checksum;
  }

  // write checksum
  Serial.write(highByte(checksum));
  Serial.write(lowByte(checksum));
  Serial.print('$');
  Serial.flush();

} // END readRom()

void setup_GB_Cart() {

  // pinout
  // A0-A7 (Port F) go to A0-A7 on GB connector
  // A8-A15 (Port K) go to A8-A15 on GB connector
  // D37-D30 (Port C) go to D0-D7
  // D53 (Port B, 0) = CLK
  // D52 (Port B, 1) = WR
  // D51 (Port B, 2) = RD
  // D50 (Port B, 3) = CS1
  // D23 (Port A, 1) = RST
  // D22 (Port A, 0) = AUD (skip)
 
  // Set A0-A7 Pins to Output and set them low
  DDRF = 0xFF;
  PORTF = 0x00;
  // Set A8-A15 Pins to Output and set them low
  DDRK = 0xFF;
  PORTK = 0x00;
  // Set Control Pins to Output CLK(D53) WR(D52) RD(D51) CS(D50)
  DDRB |= (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
  // Set Control Pins to Output RST(D23)
  DDRA |= (1 << 1);
  // Output a high signal on WR, RD, CS pins, pins are active low therefore everything is disabled now
  PORTB |= (1 << 1) | (1 << 2) | (1 << 3);
  // Output a low signal on CLK to disable writing GB Camera RAM
  PORTB &= ~(1 << 0);
  // Output a low signal on RST to initialize MMC correctly
  PORTA &= ~(1 << 1);

  // Set Audio-In to Input
  DDRA &= ~(1 << 0);
  PORTA |= (1 << 0);

  // Set Data Pins (D0-D7) to Input
  DDRC = 0x00;
  // Enable Internal Pullups
  PORTC = 0xFF;

  // Wait until all is stable
  delay(400);

  // RST to HIGH
  PORTA |= (1 << 1);

} // END setup_GB_cart()

byte readByte_GB(word myAddress) {

  // Set address
  PORTF = myAddress & 0xFF;
  PORTK = (myAddress >> 8) & 0xFF;
  // Switch data pins to input
  DDRC = 0x00;
  // Enable pullups
  PORTC = 0xFF;

  __asm__("nop\n\t"
          "nop\n\t"
          "nop\n\t"
          "nop\n\t");

  // Switch RD to LOW
  PORTB &= ~(1 << 2);

  __asm__("nop\n\t"
          "nop\n\t"
          "nop\n\t"
          "nop\n\t");

  // Read
  byte tempByte = PINC;

  // Switch and RD to HIGH
  PORTB |= (1 << 2);

  __asm__("nop\n\t"
          "nop\n\t"
          "nop\n\t"
          "nop\n\t");

  return tempByte;

} // END readByte_GB()

void writeByte_GB(int myAddress, byte myData) {

  // Set address
  PORTF = myAddress & 0xFF;
  PORTK = (myAddress >> 8) & 0xFF;
  // Set data
  PORTC = myData;
  // Switch data pins to output
  DDRC = 0xFF;

  // Wait till output is stable
  __asm__("nop\n\t"
          "nop\n\t"
          "nop\n\t"
          "nop\n\t");

  // Pull WR low
  PORTB &= ~(1 << 1);

  // Leave WR low for at least 60ns
  __asm__("nop\n\t"
          "nop\n\t"
          "nop\n\t"
          "nop\n\t");

  // Pull WR HIGH
  PORTB |= (1 << 1);

  // Leave WR high for at least 50ns
  __asm__("nop\n\t"
          "nop\n\t"
          "nop\n\t"
          "nop\n\t");

  // Switch data pins to input
  DDRC = 0x00;
  // Enable pullups
  PORTC = 0xFF;

} // END writeByte_GB()