#include "symbols.h"

/**
 * adapted from palto42's code for the ssd1306 display https://github.com/palto42/komoot-navi
 */

#include "BLEDevice.h"
#define ENABLE_GxEPD2_GFX 0
#include <GxEPD2_BW.h> // including both doesn't hurt
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

// copy constructor for your e-paper from GxEPD2_Example.ino, and for AVR needed #defines
#define MAX_DISPLAY_BUFFER_SIZE 800 // 
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)); // GDEH0213B73

std::string value = "Start";
int timer = 0 ;
// The remote service we wish to connect to.
static BLEUUID serviceUUID("71C1E128-D92F-4FA8-A2B2-0F171DB3436C");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("503DD605-9BCB-4F6E-B235-270A57483026");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

int16_t connectTextBoundsX, connectTextBoundsY;
uint16_t connectTextBoundsW, connectTextBoundsH;
const char connectText[] = "Pair to continue...";

int16_t connectedTextBoundsX, connectedTextBoundsY;
uint16_t connectedTextBoundsW, connectedTextBoundsH;
const char connectedText[] = "Connected";

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
//  Serial.print("Notify callback for characteristic ");
//  Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
//  Serial.print(" of data length ");
//  Serial.println(length);
//  Serial.print("data: ");
//  Serial.println((char*)pData);
}

class MyClientCallback : public BLEClientCallbacks {

  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

bool connectToServer() {

  Serial.print("Forming a connection to ");
  Serial.println(myDevice->getAddress().toString().c_str());
  
  BLEClient* pClient = BLEDevice::createClient();
  Serial.println(" - Created client");

  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remove BLE Server.
  pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
  Serial.println(" - Connected to server");

  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(serviceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our service");


  // Obtain a reference to the characteristic in the service of the remote BLE server.
  pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID: ");
    Serial.println(charUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }
  Serial.println(" - Found our characteristic");

  // Read the value of the characteristic.
  if(pRemoteCharacteristic->canRead()) {
    std::string value = pRemoteCharacteristic->readValue();
    if(pRemoteCharacteristic->canNotify()) {
      pRemoteCharacteristic->registerForNotify(notifyCallback);
      Serial.println(" - Registered for notifications");
      connected = true;
      return true;
    }
    Serial.println("Failed to register for notifications");
  } else {
    Serial.println("Failed to read our characteristic");
  }
  
  pClient->disconnect();
  return false;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    } 
  }
}; 

void showPartialUpdate_dir(uint8_t dir) {
  display.fillRect(0, 0, 60, 60, GxEPD_WHITE);
  display.drawBitmap(0, 0, symbols[dir].bitmap, 60, 60, GxEPD_BLACK);
  display.refresh(0, 0, 60, 60);
}

void showPartialUpdate_street(std::string street) {
  int16_t box_x, box_y;
  uint16_t box_w, box_h;
  display.fillRect(0, 80, 250, 48, GxEPD_WHITE);
  if (street.size() > 8) {
    display.setFont(&FreeMonoBold9pt7b);
  } else {
    display.setFont(&FreeMonoBold12pt7b);
  }
  display.getTextBounds(street.c_str(), 0, 128, &box_x, &box_y, &box_w, &box_h);
  uint16_t x = ((250 - box_w) / 2) - box_x;
  display.setCursor(x, box_y);
  display.print(street.c_str());
  display.refresh(0, 80, 250, 48);
}


void showPartialUpdate_dist(uint32_t dist) {
  display.setFont(&FreeMonoBold18pt7b);
  display.fillRect(125, 30, 125, 35, GxEPD_WHITE);
  display.setCursor(125, 60);
  display.print(dist);
  display.print("m");
  display.refresh(125, 30, 125, 35);
}

void calculateTextBounds() {
  connectTextBoundsX = 35;
  connectTextBoundsY = 100;
  
  display.setFont(&FreeMonoBold9pt7b);
  display.getTextBounds(connectText, connectTextBoundsX, connectTextBoundsY,
    &connectTextBoundsX, &connectTextBoundsY, &connectTextBoundsW, &connectTextBoundsH);

  connectedTextBoundsX = 35;
  connectedTextBoundsY = 100;
  
  display.setFont(&FreeMonoBold18pt7b);
  display.getTextBounds(connectedText, connectedTextBoundsX, connectedTextBoundsY,
    &connectedTextBoundsX, &connectedTextBoundsY, &connectedTextBoundsW, &connectedTextBoundsH);
}

void setup() {
  // enable debug output
//  Serial.begin(115200);
  // Display start
  display.init();
  display.setFullWindow();
  display.firstPage();
  display.setTextColor(GxEPD_BLACK);
  display.setRotation(1); //orientation set to 1 to flip the display
  display.fillScreen(GxEPD_WHITE);

  calculateTextBounds();
  
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(40,50);
  display.print("BLe Navi v1.0"); //Boot Image
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(35, 100);
  display.print(connectText);
  display.display(true);
  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("Superam");
  // Display end
  
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
} // End of setup.

std::string old_street = "old"; 
uint8_t dir = 255;
uint32_t dist2 = 4294967295;
bool updated;

void loop() {
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
      //display connected status
      display.setFont(&FreeMonoBold18pt7b);
      display.fillRect(connectTextBoundsX, connectTextBoundsY, connectTextBoundsW, connectTextBoundsH, GxEPD_WHITE);
      display.setCursor(35, 100);
      display.print(connectedText);
      display.display(true);
      delay(500);
      display.fillScreen(GxEPD_WHITE);
      display.display(false);
    } else {
      Serial.println("We have failed to connect to the server; there is nothing more we will do.");
    }
    doConnect = false;
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected) {
    std::string value = pRemoteCharacteristic->readValue();//this crashes sometimes, receives the whole data packet
    if (value.length() > 4) {
      //in case we have update flag but characteristic changed due to navigation stop between
      updated = false;

      std::string street;
      std::string firstWord;
      street = value.substr(9);//this causes abort when there are not at least 9 bytes available
      if (street != old_street) {
        old_street = street;
        firstWord = street.substr(0, street.find(", "));
        showPartialUpdate_street(firstWord);
        Serial.print("Street update: ");
        Serial.println(firstWord.c_str());
        updated = true;
      } //extracts the firstword of the street name and displays it

      std::string direction;
      direction = value.substr(4,4);
      uint8_t d=direction[0];
      if (d != dir) {
        dir = d;
        showPartialUpdate_dir(dir);
        Serial.print("Direction update: ");
        Serial.println(d);
        updated = true;
      } //display direction

      std::string distance;
      distance = value.substr(5,8);
      uint32_t dist=distance[0] | distance[1] << 8 | distance[2] << 16 | distance[3] << 24;
      if (dist2 != dist) {
        dist2 = dist;
        showPartialUpdate_dist(dist2);
        Serial.print("Distance update: ");
        Serial.println(dist2);
        updated = true;
      } //display distance in metres

      if (updated) {
        display.display(true);
      }
    } else if (doScan) {
      BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
    }
    delay(1000); // Delay a second between loops.
  }
} // End of loop
