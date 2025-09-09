/*
 * NFC Tag Writer using Arduino Mega ADK, RFID-RC522, LCD and Keypad
 * This code can write data to MIFARE Classic cards with LCD interface
 * 
 * Hardware Connections:
 * RC522 Module    Arduino Mega ADK
 * SDA             Pin 53 (SS)
 * SCK             Pin 52
 * MOSI            Pin 51
 * MISO            Pin 50
 * IRQ             Not connected
 * GND             GND
 * RST             Pin 49
 * 3.3V            3.3V
 * 
 * LCD Keypad Shield connections (standard shield pins):
 * LCD RS          Pin 8
 * LCD Enable      Pin 9
 * LCD D4          Pin 4
 * LCD D5          Pin 5
 * LCD D6          Pin 6
 * LCD D7          Pin 7
 * LCD Backlight   Pin 10
 * Keypad Analog   Pin A0
 */

#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>

#define RST_PIN         49
#define SS_PIN          53

// LCD pins
#define LCD_RS          8
#define LCD_ENABLE      9
#define LCD_D4          4
#define LCD_D5          5
#define LCD_D6          6
#define LCD_D7          7
#define LCD_BACKLIGHT   10

// Keypad analog pin
#define KEYPAD_PIN      A0

// Keypad button values (adjust these based on your specific LCD shield)
#define BTN_RIGHT       0
#define BTN_UP          1
#define BTN_DOWN        2
#define BTN_LEFT        3
#define BTN_SELECT      4
#define BTN_NONE        5

// Menu states
enum MenuState {
  MAIN_MENU,
  WRITE_DEFAULT,
  WRITE_CUSTOM,
  READ_CARD,
  FORMAT_CARD,
  CHANGE_KEY,
  TEXT_INPUT,
  KEY_INPUT,
  PROCESSING
};

// Character set for input
const char charSet[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                       "abcdefghijklmnopqrstuvwxyz"
                       "0123456789"
                       "!@#$%^&*()_+-=[]{}|;:,.<>?";
const int charSetSize = sizeof(charSet) - 1;

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal lcd(LCD_RS, LCD_ENABLE, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

// Default MIFARE key (factory default)
MFRC522::MIFARE_Key key;

// Menu variables
MenuState currentState = MAIN_MENU;
int menuIndex = 0;
const int maxMenuItems = 5;
String menuItems[] = {
  "Write Default",
  "Write Custom",
  "Read Card",
  "Format Card",
  "Change Auth Key"
};

// Text input variables
String inputText = "";
int cursorPos = 0;
int charIndex = 0;
bool inputComplete = false;

// Key input variables
String keyInput = "";
int keyPos = 0;
int keyCharIndex = 0;

// Button handling
unsigned long lastButtonTime = 0;
const unsigned long buttonDelay = 200;

void setup() {
  Serial.begin(9600);
  
  // Initialize LCD
  pinMode(LCD_BACKLIGHT, OUTPUT);
  digitalWrite(LCD_BACKLIGHT, HIGH);
  lcd.begin(16, 2);
  lcd.clear();
  
  SPI.begin();
  mfrc522.PCD_Init();
  
  // Prepare the key (used both as key A and key B)
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  
  // Display startup message
  lcd.setCursor(0, 0);
  lcd.print("NFC Tag Writer");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(2000);
  
  displayMainMenu();
}

void loop() {
  int button = readButton();
  
  if (button != BTN_NONE && millis() - lastButtonTime > buttonDelay) {
    lastButtonTime = millis();
    handleButton(button);
  }
  
  delay(50);
}

int readButton() {
  int adc = analogRead(KEYPAD_PIN);
  
  // Adjust these values based on your LCD shield
  if (adc > 1000) return BTN_NONE;
  if (adc < 50)   return BTN_RIGHT;
  if (adc < 195)  return BTN_UP;
  if (adc < 380)  return BTN_DOWN;
  if (adc < 555)  return BTN_LEFT;
  if (adc < 790)  return BTN_SELECT;
  
  return BTN_NONE;
}

void handleButton(int button) {
  switch (currentState) {
    case MAIN_MENU:
      handleMainMenuButton(button);
      break;
    case TEXT_INPUT:
      handleTextInputButton(button);
      break;
    case KEY_INPUT:
      handleKeyInputButton(button);
      break;
    case PROCESSING:
      // Do nothing during processing
      break;
    default:
      // For other states, return to main menu on any button
      if (button == BTN_SELECT) {
        currentState = MAIN_MENU;
        displayMainMenu();
      }
      break;
  }
}

void handleMainMenuButton(int button) {
  switch (button) {
    case BTN_UP:
      menuIndex = (menuIndex - 1 + maxMenuItems) % maxMenuItems;
      displayMainMenu();
      break;
    case BTN_DOWN:
      menuIndex = (menuIndex + 1) % maxMenuItems;
      displayMainMenu();
      break;
    case BTN_SELECT:
      executeMenuOption();
      break;
  }
}

void handleTextInputButton(int button) {
  switch (button) {
    case BTN_UP:
      charIndex = (charIndex + 1) % charSetSize;
      updateCurrentCharacter();
      displayTextInput();
      break;
    case BTN_DOWN:
      charIndex = (charIndex - 1 + charSetSize) % charSetSize;
      updateCurrentCharacter();
      displayTextInput();
      break;
    case BTN_LEFT:
      if (cursorPos > 0) {
        cursorPos--;
        // Get the character at the new cursor position
        if (cursorPos < inputText.length()) {
          char currentChar = inputText[cursorPos];
          // Find this character in our character set
          for (int i = 0; i < charSetSize; i++) {
            if (charSet[i] == currentChar) {
              charIndex = i;
              break;
            }
          }
        } else {
          charIndex = 0; // Default to space
        }
        displayTextInput();
      }
      break;
    case BTN_RIGHT:
      if (cursorPos < 16) {
        cursorPos++;
        // Get the character at the new cursor position
        if (cursorPos < inputText.length()) {
          char currentChar = inputText[cursorPos];
          // Find this character in our character set
          for (int i = 0; i < charSetSize; i++) {
            if (charSet[i] == currentChar) {
              charIndex = i;
              break;
            }
          }
        } else {
          charIndex = 0; // Default to space
        }
        displayTextInput();
      }
      break;
    case BTN_SELECT:
      // Write the text to card
      executeTextInput();
      break;
  }
}

void updateCurrentCharacter() {
  // Update the character at the current cursor position
  char newChar = charSet[charIndex];
  
  // Extend string if cursor is beyond current length
  while (inputText.length() <= cursorPos) {
    inputText += ' '; // Add spaces to reach cursor position
  }
  
  // Update the character at cursor position
  inputText[cursorPos] = newChar;
  
  // Remove trailing spaces for display cleanliness
  while (inputText.length() > 0 && inputText[inputText.length() - 1] == ' ' && inputText.length() > cursorPos + 1) {
    inputText = inputText.substring(0, inputText.length() - 1);
  }
}

void handleKeyInputButton(int button) {
  switch (button) {
    case BTN_UP:
      keyCharIndex = (keyCharIndex + 1) % 16; // 0-9, A-F
      displayKeyInput();
      break;
    case BTN_DOWN:
      keyCharIndex = (keyCharIndex - 1 + 16) % 16;
      displayKeyInput();
      break;
    case BTN_LEFT:
      if (keyPos > 0) {
        keyPos--;
        displayKeyInput();
      }
      break;
    case BTN_RIGHT:
      if (keyPos < 11) { // 12 hex characters - 1
        keyPos++;
        displayKeyInput();
      }
      break;
    case BTN_SELECT:
      // Update hex character at current position
      char hexChar = (keyCharIndex < 10) ? '0' + keyCharIndex : 'A' + (keyCharIndex - 10);
      
      if (keyPos >= keyInput.length()) {
        keyInput += hexChar;
      } else {
        keyInput[keyPos] = hexChar;
      }
      
      // Move to next position or finish
      if (keyPos < 11) {
        keyPos++;
        displayKeyInput();
      } else {
        executeKeyInput();
      }
      break;
  }
}

void displayMainMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("NFC Menu:");
  lcd.setCursor(0, 1);
  lcd.print(">");
  lcd.print(menuItems[menuIndex]);
}

void displayTextInput() {
  lcd.clear();
  lcd.setCursor(0, 0);
  
  // Display "Text:" and the input string
  lcd.print("Text:");
  String displayStr = inputText;
  if (displayStr.length() == 0) {
    displayStr = " "; // Show at least one space
  }
  
  // Show up to 11 characters (16 - 5 for "Text:")
  if (displayStr.length() > 11) {
    int startPos = max(0, cursorPos - 5); // Center cursor in view
    displayStr = displayStr.substring(startPos, startPos + 11);
    lcd.print(displayStr);
  } else {
    lcd.print(displayStr);
  }
  
  // Second line: show current character and position
  lcd.setCursor(0, 1);
  lcd.print("Char:");
  lcd.print(charSet[charIndex]);
  lcd.print(" Pos:");
  lcd.print(cursorPos);
  lcd.print("/16");
  
  // Show cursor position indicator on first line if possible
  if (cursorPos < 11 && inputText.length() <= 11) {
    lcd.setCursor(5 + cursorPos, 0); // 5 offset for "Text:"
    lcd.cursor();
  } else {
    lcd.noCursor();
  }
}

void displayKeyInput() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Key: ");
  lcd.print(keyInput);
  
  lcd.setCursor(0, 1);
  char hexChar = (keyCharIndex < 10) ? '0' + keyCharIndex : 'A' + (keyCharIndex - 10);
  lcd.print("Hex: ");
  lcd.print(hexChar);
  lcd.print(" Pos:");
  lcd.print(keyPos);
}

void executeMenuOption() {
  currentState = PROCESSING;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Processing...");
  
  switch (menuIndex) {
    case 0: // Write Default
      writeCard();
      break;
    case 1: // Write Custom
      startTextInput();
      return; // Don't return to main menu yet
    case 2: // Read Card
      readCard();
      break;
    case 3: // Format Card
      formatCard();
      break;
    case 4: // Change Auth Key
      startKeyInput();
      return; // Don't return to main menu yet
  }
  
  // Return to main menu after 2 seconds
  delay(2000);
  currentState = MAIN_MENU;
  displayMainMenu();
}

void startTextInput() {
  currentState = TEXT_INPUT;
  inputText = "";
  cursorPos = 0;
  charIndex = 0;
  inputComplete = false;
  lcd.noCursor();
  displayTextInput();
}

void startKeyInput() {
  currentState = KEY_INPUT;
  keyInput = "";
  keyPos = 0;
  keyCharIndex = 0;
  displayKeyInput();
}

void executeTextInput() {
  currentState = PROCESSING;
  lcd.noCursor(); // Hide cursor during processing
  
  // Trim trailing spaces for actual writing
  String textToWrite = inputText;
  while (textToWrite.length() > 0 && textToWrite[textToWrite.length() - 1] == ' ') {
    textToWrite = textToWrite.substring(0, textToWrite.length() - 1);
  }
  
  writeCustomText(textToWrite);
  
  // Return to main menu
  delay(2000);
  currentState = MAIN_MENU;
  displayMainMenu();
}

void executeKeyInput() {
  currentState = PROCESSING;
  changeKey();
  
  // Return to main menu
  delay(2000);
  currentState = MAIN_MENU;
  displayMainMenu();
}

void writeCard() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place card...");
  
  if (!waitForCard()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Failed!");
    return;
  }
  
  // Data to write (16 bytes per block)
  byte dataBlock[] = {
    'H', 'e', 'l', 'l', 'o', ' ', 'N', 'F', 'C', '!', ' ', ' ', ' ', ' ', ' ', ' '
  };
  
  byte block = 4;
  byte trailerBlock = 7;
  
  MFRC522::StatusCode status;
  
  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Auth Failed!");
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }
  
  status = mfrc522.MIFARE_Write(block, dataBlock, 16);
  if (status != MFRC522::STATUS_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Write Failed!");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Write Success!");
    lcd.setCursor(0, 1);
    lcd.print("Hello NFC!");
  }
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void readCard() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place card...");
  
  if (!waitForCard()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Failed!");
    return;
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UID:");
  
  // Display UID (first 8 hex chars that fit)
  String uidStr = "";
  for (byte i = 0; i < mfrc522.uid.size && i < 4; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uidStr += "0";
    uidStr += String(mfrc522.uid.uidByte[i], HEX);
  }
  uidStr.toUpperCase();
  lcd.print(uidStr);
  
  // Try to read block 4
  byte block = 4;
  byte trailerBlock = 7;
  byte buffer[18];
  byte size = sizeof(buffer);
  
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status == MFRC522::STATUS_OK) {
    status = mfrc522.MIFARE_Read(block, buffer, &size);
    if (status == MFRC522::STATUS_OK) {
      lcd.setCursor(0, 1);
      lcd.print("Data:");
      
      // Display readable text
      String text = "";
      for (byte i = 0; i < 11 && i < 16; i++) { // Max 11 chars to fit on display
        if (buffer[i] >= 32 && buffer[i] <= 126) {
          text += (char)buffer[i];
        } else {
          text += ".";
        }
      }
      lcd.print(text);
    } else {
      lcd.setCursor(0, 1);
      lcd.print("Read Failed!");
    }
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Auth Failed!");
  }
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void writeCustomText(String textToWrite) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place card...");
  
  if (!waitForCard()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Failed!");
    return;
  }
  
  // Prepare data block
  byte dataBlock[16];
  memset(dataBlock, ' ', 16); // Fill with spaces
  
  // Copy input text to data block
  for (int i = 0; i < min(textToWrite.length(), 16); i++) {
    dataBlock[i] = textToWrite[i];
  }
  
  byte block = 4;
  byte trailerBlock = 7;
  
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Auth Failed!");
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
    return;
  }
  
  status = mfrc522.MIFARE_Write(block, dataBlock, 16);
  if (status != MFRC522::STATUS_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Write Failed!");
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Write Success!");
    lcd.setCursor(0, 1);
    String displayText = textToWrite.substring(0, 16);
    lcd.print(displayText);
  }
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void changeKey() {
  if (keyInput.length() != 12) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Invalid Key!");
    return;
  }
  
  // Parse hex string to bytes
  for (int i = 0; i < 6; i++) {
    String byteString = keyInput.substring(i * 2, i * 2 + 2);
    key.keyByte[i] = strtoul(byteString.c_str(), NULL, 16);
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Key Updated!");
  lcd.setCursor(0, 1);
  lcd.print(keyInput);
}

void formatCard() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place card...");
  
  if (!waitForCard()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Failed!");
    return;
  }
  
  // Reset key to default
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Formatting...");
  
  // Clear data blocks
  byte emptyBlock[16];
  memset(emptyBlock, 0, 16);
  
  bool success = true;
  for (byte sector = 1; sector < 16 && success; sector++) {
    byte firstBlock = sector * 4;
    byte trailerBlock = firstBlock + 3;
    
    MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
    if (status == MFRC522::STATUS_OK) {
      for (byte blockOffset = 0; blockOffset < 3; blockOffset++) {
        byte blockNumber = firstBlock + blockOffset;
        if (mfrc522.MIFARE_Write(blockNumber, emptyBlock, 16) != MFRC522::STATUS_OK) {
          success = false;
          break;
        }
      }
    }
  }
  
  lcd.clear();
  lcd.setCursor(0, 0);
  if (success) {
    lcd.print("Format Success!");
  } else {
    lcd.print("Format Failed!");
  }
  
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

bool waitForCard() {
  unsigned long startTime = millis();
  const unsigned long timeout = 10000; // 10 second timeout
  
  while (millis() - startTime < timeout) {
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
      if (piccType == MFRC522::PICC_TYPE_MIFARE_MINI ||  
          piccType == MFRC522::PICC_TYPE_MIFARE_1K ||
          piccType == MFRC522::PICC_TYPE_MIFARE_4K) {
        return true;
      }
      mfrc522.PICC_HaltA(); 
      mfrc522.PCD_StopCrypto1();
    }
    delay(50);
  }
  return false;
}

void dumpByteArray(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
  }