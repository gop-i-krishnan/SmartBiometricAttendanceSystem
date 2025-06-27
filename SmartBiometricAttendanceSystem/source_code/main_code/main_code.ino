//******************************* Libraries *******************************
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <Keypad.h>

//******************************* Firebase Setup *******************************
FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;
// WARNING: Do not commit this key to public repositories!
#define FIREBASE_HOST " "  // Firebase project URL
#define FIREBASE_AUTH " "  // Firebase secret key

//******************************* WiFi Setup *******************************
const char* ssid = " ";       // Your WiFi SSID
const char* password = " ";    // Your WiFi password

//******************************* Fingerprint Sensor *******************************
#define Finger_Tx 17
#define Finger_Rx 16

HardwareSerial mySerial(2);  // Using UART2 on ESP32
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

//******************************* LCD Setup *******************************
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C LCD

//******************************* Keypad Setup *******************************
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {32, 33, 25, 26};    // R1 to R4
byte colPins[COLS] = {27, 14, 12, 13};    // C1 to C4

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//******************************* LED + Buzzer + Push Button *******************************
#define GREEN_LED   4
#define ORANGE_LED  18
#define RED_LED     5
#define BUZZER      19
#define PUSH_BUTTON 15

//******************************* Global Variables *******************************
bool fingerprintSuccess = false;
String currentDate = "";
String firebasePath = "/students";
bool isTypingPIN = false;

#include <map>
std::map<int, int> templateToID;  // templateID → studentID

unsigned long feedbackStart = 0;
String feedbackType = "";
bool feedbackActive = false;

// ==================== PUSH BUTTON FUNCTIONALITY ====================
unsigned long pressStart = 0;
bool pressed = false;
bool longPressHandled = false;
const int MIN_ENROLL_ID = 1001;
int nextEnrollID = 1001;
const int MAX_ENROLL_ID = 1127;  // Max supported by R307

// ==================== Menu Mode for Push Button ====================
enum MenuMode {
  MODE_IDLE,
  MODE_ENROLL,
  MODE_DELETE
};

MenuMode currentMode = MODE_IDLE;

//******************************* Function Declarations *******************************
void showWelcomeScreen();

// ===========================================================
// Setup Function
// - Initializes LCD, WiFi, Firebase, Fingerprint Sensor, I/O
// ===========================================================
void setup() {
  // ----------------------- 1. Serial Debugging Init -----------------------
  Serial.begin(115200);
  delay(100);
  Serial.println("🔧 Setup started...");

  // ----------------------- 2. LCD + I2C Init ------------------------------
  Wire.begin(21, 22);  // I2C: SDA = GPIO21, SCL = GPIO22
  lcd.init(); lcd.backlight();
  showWelcomeScreen();

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Initializing...");
  delay(1500);

  // ----------------------- 3. WiFi Connection -----------------------------
  WiFi.begin(ssid, password);
  lcd.clear(); lcd.setCursor(0, 0); lcd.print("Connecting WiFi");
  Serial.print("Connecting to WiFi");

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    lcd.print("."); Serial.print(".");
    retry++; yield();
  }

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected");
    Serial.print("IP Address: "); Serial.println(WiFi.localIP());
    lcd.setCursor(0, 0); lcd.print("WiFi OK:");
    lcd.setCursor(0, 1); lcd.print(WiFi.localIP());
  } else {
    Serial.println("\n❌ WiFi Failed");
    lcd.setCursor(0, 0); lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1); lcd.print("Check Network!");
    delay(3000);
  }

  // ----------------------- 4. Time Sync + Firebase Init ------------------
  configTime(19800, 0, "pool.ntp.org", "time.nist.gov");  // UTC+5:30 (IST)
  delay(2000);  // Allow time sync before Firebase begins

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  // ----------------------- 5. Fingerprint Sensor Setup -------------------
  mySerial.begin(57600, SERIAL_8N1, Finger_Rx, Finger_Tx);
  finger.begin(57600);
  bool sensorOk = false;

  for (int i = 0; i < 5; i++) {
    if (finger.verifyPassword()) {
      sensorOk = true;
      break;
    }
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Sensor Error!");
    lcd.setCursor(0, 1); lcd.print("Retrying...");
    Serial.println("Retrying fingerprint sensor...");
    delay(1000); yield();
  }

  if (!sensorOk) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Sensor Failed!");
    lcd.setCursor(0, 1); lcd.print("Restarting...");
    delay(2000); yield();
    ESP.restart();  // Emergency restart
  }

  Serial.println("✅ Fingerprint sensor OK");

  int totalTemplates = finger.getTemplateCount();
  Serial.print("Templates stored: ");
  Serial.println(totalTemplates);

  // ----------------------- 6. Optional: Load Finger Index Mapping ---------
  /*
  Serial.println("🔁 Loading template-ID mappings from Firebase...");
  for (int id = 1001; id <= 1127; id++) {
    String path = firebasePath + "/" + String(id) + "/finger_index";
    if (Firebase.RTDB.getInt(&firebaseData, path)) {
      int templateID = firebaseData.intData();
      templateToID[templateID] = id;
      Serial.println("✅ Mapped template " + String(templateID) + " to ID: " + String(id));
    } else {
      Serial.print("⚠️ No finger_index for ID: ");
      Serial.println(id);
      Serial.println("Reason: " + firebaseData.errorReason());
    }
    delay(50); yield();
  }
  */

  // ----------------------- 7. Show Occupied Slots -------------------------
  Serial.println("🔍 Checking all stored template slots...");
  for (int i = 1; i <= 127; i++) {
    if (finger.loadModel(i) == FINGERPRINT_OK) {
      Serial.print("✅ Slot in use: "); Serial.println(i);
    }
  }

  // ----------------------- 8. I/O Pin Config ------------------------------
  pinMode(GREEN_LED, OUTPUT);
  pinMode(ORANGE_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(PUSH_BUTTON, INPUT_PULLUP);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(ORANGE_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);

  // ----------------------- 9. Final Ready Message -------------------------
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Sensor: OK");
  lcd.setCursor(0, 1); lcd.print("Ready to scan");
  delay(1500);

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("System Ready");
  lcd.setCursor(0, 1); lcd.print("Waiting...");
  Serial.println("✅ Setup Complete. Entering loop...");
  delay(2000); yield();
}

// ===========================================================
// Main Control Loop
// ===========================================================
void loop() {
  // -------------------- 0. Handle Feedback Animations --------------------
  updateFeedback();

  // Debug: Print every 5 seconds
  static unsigned long lastLoopPrint = 0;
  if (millis() - lastLoopPrint > 5000) {
    Serial.println("🔄 Loop running...");
    lastLoopPrint = millis();
  }

  // -------------------- 1. Ensure WiFi + Firebase Connectivity ------------
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  if (!Firebase.ready()) {
    Serial.println("🔁 Firebase not ready. Reconnecting...");
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    delay(500);
  }

  // -------------------- 2. Show "Waiting..." Message ---------------------
  if (!isTypingPIN) {
    showWaitingMessage();
  }

  // -------------------- 3. Fingerprint Cooldown System -------------------
  static unsigned long lastFingerprintTime = 0;
  const unsigned long cooldownDuration = 3000;

  if (millis() - lastFingerprintTime < cooldownDuration) {
    return;  // Cooldown active: skip rest of loop
  }

  // -------------------- 4. Fingerprint Scan (Highest Priority) -----------
  int rawFingerprintID = getFingerprintID();
  if (rawFingerprintID > 0) {
    String studentID = getStudentIDFromTemplate(rawFingerprintID);

    if (studentID != "") {
      lastFingerprintTime = millis();  // Reset cooldown timer
      handleattendance(studentID.toInt());
    } else {
      Serial.println("⚠️ No Firebase mapping for template ID: " + String(rawFingerprintID));
      lcd.clear(); lcd.print("Unmapped Finger");
      playFeedback("error");
      delay(2000); yield();
    }
    return;
  }

  // -------------------- 5. PIN Entry (Keypad Login) ----------------------
  char key = keypad.getKey();
  if (key == '#' || key == 'A') {
    Serial.println("Key pressed: " + String(key));
    handlePINLogin();
    return;
  }

  // -------------------- 6. Admin Mode (Push Button) ----------------------
  checkPushButton();
}

// ===================================================
// FUNCTION: getFingerprintID
// PURPOSE : Scan finger, match with template, return template ID
// Returns  : >0 = Matched template ID
//            0  = No finger
//           -1  = Not found
//           -2  = Error
// ===================================================
int getFingerprintID() {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 3000) {
    Serial.println("[🔍] Waiting for finger...");
    lastPrint = millis();
  }

  // Step 1: Try to capture finger image
  uint8_t result = finger.getImage();
  if (result == FINGERPRINT_NOFINGER) return 0;
  if (result != FINGERPRINT_OK) {
    Serial.println("❌ Failed to capture image");
    return -2;
  }
  Serial.println("✅ Image captured");

  // Step 2: Convert to template buffer 1
  result = finger.image2Tz(1);
  if (result != FINGERPRINT_OK) {
    Serial.println("❌ Failed to convert image");
    return -2;
  }
  Serial.println("✅ Converted to template");

  // Step 3: Full slot-by-slot search (recommended for accuracy)
  result = finger.fingerSearch();
  if (result == FINGERPRINT_OK) {
    Serial.println("✅ Fingerprint match → Template ID: " + String(finger.fingerID));
    return finger.fingerID;
  } 
  else if (result == FINGERPRINT_NOTFOUND) {
    Serial.println("❌ Finger not found");
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Not Recognized");
    playFeedback("warning");
    return -1;
  } 
  else {
    Serial.println("❌ Matching error");
    return -2;
  }
}

// ===================================================
// FUNCTION: getStudentIDFromTemplate
// PURPOSE : Map fingerprint template ID → student ID
// ===================================================
String getStudentIDFromTemplate(int templateID) {
  // 1. First check in local in-memory cache
  if (templateToID.find(templateID) != templateToID.end()) {
    return String(templateToID[templateID]);
  }

  // 2. Fallback: Check Firebase for any matching finger_index
  for (int id = 1001; id <= 1127; id++) {
    String path = firebasePath + "/" + String(id) + "/finger_index";

    // Safe fetch with retry (3 attempts with backoff)
    for (int attempts = 0; attempts < 3; attempts++) {
      if (Firebase.RTDB.getInt(&firebaseData, path)) {
        int index = firebaseData.intData();

        if (index == templateID) {
          templateToID[templateID] = id;  // Update cache
          Serial.println("✅ Match found in Firebase → ID: " + String(id));
          return String(id);
        } else {
          break; // this ID has a different template, skip
        }
      }
      delay(200 * (1 << attempts)); // Exponential backoff
      yield();
    }
  }

  Serial.println("[❌] No matching student for template ID: " + String(templateID));
  return "";
}

// ===========================================================
// HELPER FUNCTION: findFirstFreeTemplateSlot
// PURPOSE       : Returns the first unused template slot index (1-127)
// ===========================================================
int findFirstFreeTemplateSlot() {
  for (int i = 1; i <= 127; i++) {
    if (finger.loadModel(i) != FINGERPRINT_OK) {
      return i;
    }
  }
  return -1; // No free slots available
}

// ===========================================================
// FUNCTION: enrollNewFingerprint()
// PURPOSE : Admin-authenticated fingerprint enrollment with duplication check
// ===========================================================
void enrollNewFingerprint(String idStr, String pin) {
  if (!Firebase.ready()) {
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    yield();
  }

  int id = idStr.toInt();

  // 🔐 1. Verify Admin PIN
  String pinPath = firebasePath + "/" + idStr + "/pin";
  bool verified = false;
  for (int attempt = 0; attempt < 3; attempt++) {
    if (Firebase.RTDB.getString(&firebaseData, pinPath)) {
      if (firebaseData.stringData() == pin) {
        verified = true;
        break;
      }
    }
    delay(200 * (1 << attempt)); yield();
  }

  if (!verified) {
    lcd.clear(); lcd.print("Invalid ID/PIN");
    playFeedback("error");
    return;
  }

  // ✅ 1.1 Check if this student ID already has an enrolled fingerprint
  String indexPath = firebasePath + "/" + idStr + "/finger_index";
  if (Firebase.RTDB.getInt(&firebaseData, indexPath)) {
    int existingTemplate = firebaseData.intData();
    if (finger.loadModel(existingTemplate) == FINGERPRINT_OK) {
      lcd.clear(); lcd.setCursor(0, 0); lcd.print("Already Enrolled");
      lcd.setCursor(0, 1); lcd.print("ID: " + idStr);
      playFeedback("warning");
      Serial.println("⚠️ Student " + idStr + " already has template " + String(existingTemplate));
      return;
    }
  }

  // 👆 2. First scan
  lcd.clear(); lcd.print("Place finger...");
  while (finger.getImage() != FINGERPRINT_OK) delay(50);
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    lcd.clear(); lcd.print("Scan failed (1)");
    playFeedback("error");
    return;
  }

  // 🤔 3. Check if this finger is already enrolled
  if (finger.fingerSearch() == FINGERPRINT_OK) {
    int existingID = finger.fingerID;
    String matchedID = getStudentIDFromTemplate(existingID);
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Already Enrolled");
    lcd.setCursor(0, 1); lcd.print("ID: " + matchedID);
    Serial.println("⚠️ Duplicate finger → ID: " + matchedID);
    playFeedback("warning");
    return;
  }

  // ✋ 4. Second scan
  lcd.clear(); lcd.print("Remove finger");
  delay(1500);
  while (finger.getImage() != FINGERPRINT_NOFINGER) delay(50);

  lcd.clear(); lcd.print("Place again...");
  while (finger.getImage() != FINGERPRINT_OK) delay(50);
  if (finger.image2Tz(2) != FINGERPRINT_OK || finger.createModel() != FINGERPRINT_OK) {
    lcd.clear(); lcd.print("Scan failed (2)");
    playFeedback("error");
    return;
  }

  // 🤔 5. Get or assign template ID
  int templateID = -1;
  if (Firebase.RTDB.getInt(&firebaseData, indexPath)) {
    templateID = firebaseData.intData();
    Serial.println("ℹ️ Existing index for ID " + idStr + ": " + String(templateID));
  } else {
    templateID = findFirstFreeTemplateSlot();
    if (templateID != -1) {
      Firebase.RTDB.setInt(&firebaseData, indexPath, templateID);
      Serial.println("🆕 Assigned new index " + String(templateID) + " to ID " + idStr);
    } else {
      lcd.clear(); lcd.print("No slots left!");
      playFeedback("error");
      Serial.println("❌ No free slot to assign");
      return;
    }
  }

  // 🧽 6. Clear sensor slot just in case (safe overwrite)
  finger.deleteModel(templateID);

  // 🧪 7. Store in sensor and update mapping
  if (finger.storeModel(templateID) == FINGERPRINT_OK) {
    templateToID[templateID] = id;
    Serial.println("✅ Finger stored → Template: " + String(templateID) + " | ID: " + idStr);

    lcd.clear(); lcd.print("Enrolled!");
    lcd.setCursor(0, 1); lcd.print("ID: " + idStr);
    playFeedback("success");
  } else {
    lcd.clear(); lcd.print("Store Failed");
    playFeedback("error");
    Serial.println("❌ Failed to store finger at template ID: " + String(templateID));
  }

  yield();
}

//=================================================
// FUNCTION: handleattendance(int id)
// PURPOSE : Validate fees and mark attendance with time_in/time_out and method
//=================================================
void handleattendance(int id) {
  // [1] Connectivity Check
  if (WiFi.status() != WL_CONNECTED) connectToWiFi();
  if (!Firebase.ready()) {
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    yield();
  }

  // [2] Fetch Fee Data
  String name = "ID: " + String(id);
  String path = firebasePath + "/" + String(id) + "/payments";
  Serial.println("🔥 Fetching from path: " + path);

  FirebaseJson paymentsJson;
  bool success = false;
  for (int i = 0; i < 3; i++) {
    if (Firebase.RTDB.getJSON(&firebaseData, path)) {
      paymentsJson = firebaseData.jsonObject();
      success = true;
      break;
    }
    delay(300); yield();
  }

  if (!success) {
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("No Fee Record");
    playFeedback("error");
    Serial.println("❌ Could not fetch fees for ID: " + String(id));
    return;
  }

  // [3] Count Unpaid Months
  String months[] = {
    "fee_jan", "fee_feb", "fee_mar", "fee_apr", "fee_may", "fee_jun",
    "fee_jul", "fee_aug", "fee_sep", "fee_oct", "fee_nov", "fee_dec"
  };
  int unpaid = 0;
  for (String m : months) {
    FirebaseJsonData fee;
    if (paymentsJson.get(fee, m)) {
      String val = fee.stringValue;
      val.toLowerCase();
      if (val.startsWith("unpaid")) unpaid++;
    }
  }

  // [4] Show Status & Feedback
  String status = "blocked";

  if (unpaid >= 2) {
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("BLOCKED - Fees");
    lcd.setCursor(0, 1); lcd.print("Pending >1 month");
    playFeedback("error");
    delay(2500);
    turnOffAllLEDs();
    return;
  } else if (unpaid == 1) {
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("1 Month Pending");
    lcd.setCursor(0, 1); lcd.print(name);
    playFeedback("warning");
    delay(2000);
    status = "present";
  } else {
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("WELCOME");
    lcd.setCursor(0, 1); lcd.print(name);
    playFeedback("success");
    delay(2000);
    status = "present";
  }

  turnOffAllLEDs(); // Important

  // [5] Mark Attendance (IN or OUT)
  String today = getCurrentDate();
  String now = getCurrentTime();
  String attPath = firebasePath + "/" + String(id) + "/attendance/" + today;

  bool marked = false, hasTimeOut = false;
  if (Firebase.RTDB.getJSON(&firebaseData, attPath)) {
    FirebaseJson& j = firebaseData.jsonObject();
    FirebaseJsonData timeIn, timeOut;
    if (j.get(timeIn, "time_in") && timeIn.stringValue != "") marked = true;
    if (j.get(timeOut, "time_out") && timeOut.stringValue != "") hasTimeOut = true;
  }

  if (marked && hasTimeOut) {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print(" Already Marked ");
    lcd.setCursor(0, 1); lcd.print("   for today   ");
    playFeedback("warning");
    delay(2000);
    return;
  }

  if (marked && !hasTimeOut) {
    Firebase.RTDB.setString(&firebaseData, attPath + "/time_out", now);
    lcd.clear(); lcd.setCursor(0, 0); lcd.print("Goodbye,");
    lcd.setCursor(0, 1); lcd.print(name);
    playFeedback("success");
    Serial.println("📤 time_out updated for ID: " + String(id));
  } else {
    FirebaseJson attJson;
    attJson.set("time_in", now);
    attJson.set("time_out", now);
    attJson.set("method", fingerprintSuccess ? "fingerprint" : "manual");

    if (Firebase.RTDB.setJSON(&firebaseData, attPath, &attJson)) {
      Serial.println("✅ Attendance marked for ID: " + String(id));
    } else {
      Serial.println("❌ Failed to write attendance block");
    }
  }

  // [6] Final Cooldown
  fingerprintSuccess = true;
  delay(300); yield();
}

// ===========================================================
// FUNCTION: checkPushButton()
// PURPOSE : Enroll or delete fingerprints with admin PIN auth
// ===========================================================
void checkPushButton() {
  if (!Firebase.ready()) {
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    delay(200); yield();
  }

  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;
  int buttonState = digitalRead(PUSH_BUTTON);

  if (buttonState == LOW && !pressed && (millis() - lastDebounceTime > debounceDelay)) {
    pressStart = millis();
    pressed = true;
    lastDebounceTime = millis();
  }

  if (buttonState == HIGH && pressed && (millis() - lastDebounceTime > debounceDelay)) {
    unsigned long pressDuration = millis() - pressStart;
    pressed = false;
    lastDebounceTime = millis();

    // 📦 Collect admin ID & PIN
    lcd.clear(); lcd.print("Enter ID:");
    String idStr = getPinFromKeypad("ID");

    lcd.clear(); lcd.print("Admin PIN:");
    String pin = getPinFromKeypad("PIN");

    String pinPath = firebasePath + "/" + idStr + "/pin";
    bool verified = false;

    for (int i = 0; i < 3; i++) {
      if (Firebase.RTDB.getString(&firebaseData, pinPath)) {
        if (firebaseData.stringData() == pin) {
          verified = true;
          break;
        }
      }
      delay(300 * (1 << i)); yield();
    }

    if (!verified) {
      lcd.clear(); lcd.print("Invalid Admin");
      playFeedback("error");
      return;
    }

    // 🔁 SHORT PRESS → Enroll Fingerprint
    if (pressDuration < 1500) {
      enrollNewFingerprint(idStr, pin);
    }

    // 🗑️ LONG PRESS → Delete Fingerprint
    else if (pressDuration >= 2500) {
      String fingerPath = firebasePath + "/" + idStr + "/finger_index";

      if (Firebase.RTDB.getInt(&firebaseData, fingerPath)) {
        int templateID = firebaseData.intData();

        lcd.clear(); lcd.setCursor(0, 0); lcd.print("Del Finger:");
        lcd.setCursor(0, 1); lcd.print("ID: " + idStr);
        delay(1200);

        // Step 1: Confirm via keypad
        lcd.clear(); lcd.print("Press # to CONF");
        unsigned long confirmStart = millis();
        bool confirmed = false;

        while (millis() - confirmStart < 10000) {
          char key = keypad.getKey();
          if (key == '#') {
            confirmed = true;
            break;
          }
          delay(10);
        }

        if (!confirmed) {
          lcd.clear(); lcd.print("Cancelled");
          delay(1000); return;
        }

        // Step 2: Confirm via long button hold
        lcd.clear(); lcd.print("Hold Btn to CONF");
        lcd.setCursor(0, 1); lcd.print("ID: " + idStr);

        while (digitalRead(PUSH_BUTTON) == HIGH) delay(10);  // wait for press
        unsigned long holdStart = millis();

        while (digitalRead(PUSH_BUTTON) == LOW) {
          if (millis() - holdStart >= 2000) break;
          delay(10);
        }

        if (millis() - holdStart < 2000) {
          lcd.clear(); lcd.print("Cancelled");
          delay(1000); return;
        }

        // 🔥 Final Delete from Sensor + Firebase
        if (finger.deleteModel(templateID) == FINGERPRINT_OK) {
          Firebase.RTDB.deleteNode(&firebaseData, fingerPath);
          templateToID.erase(templateID);
          lcd.clear(); lcd.print("FP Deleted");
          lcd.setCursor(0, 1); lcd.print("ID: " + idStr);
          playFeedback("success");
          Serial.println("✅ Fingerprint deleted for ID: " + idStr);
        } else {
          lcd.clear(); lcd.print("Delete Failed");
          playFeedback("error");
          Serial.println("❌ Sensor delete failed for ID: " + idStr);
        }

      } else {
        lcd.clear(); lcd.print("No FP Found");
        playFeedback("warning");
        Serial.println("⚠️ No finger_index for ID: " + idStr);
      }
    }
  }
}

// ===================================================
// FUNCTION: handlePINLogin()
// PURPOSE : Authenticate student via PIN and log attendance
// Triggered when * or # is pressed on keypad
// ===================================================
void handlePINLogin() {
  isTypingPIN = true;

  lcd.clear(); lcd.setCursor(0, 0); lcd.print("Enter ID:");
  String idStr = getPinFromKeypad("ID");
  int id = idStr.toInt();

  lcd.clear(); lcd.setCursor(0, 0); lcd.print("Enter PIN:");
  String pin = getPinFromKeypad("PIN");

  lcd.clear(); lcd.setCursor(0, 0); lcd.print("Checking...");
  delay(100); yield();

  // Ensure Firebase is ready
  if (!Firebase.ready()) {
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    delay(200); yield();
  }

  // 🔐 Verify PIN from Firebase with retry
  String path = firebasePath + "/" + idStr + "/pin";
  String firebasePIN = "";
  bool verified = false;

  for (int i = 0; i < 3; i++) {
    if (Firebase.RTDB.getString(&firebaseData, path)) {
      firebasePIN = firebaseData.stringData();
      verified = true;
      break;
    }
    delay(300 * (1 << i)); yield(); // exponential backoff
  }

  if (!verified || firebasePIN == "") {
    lcd.clear(); lcd.print("ID Not Found");
    playFeedback("error");
    isTypingPIN = false;
    return;
  }

  if (firebasePIN == pin) {
    fingerprintSuccess = false;  // indicates manual login
    handleattendance(id);
  } else {
    lcd.clear(); lcd.print("Wrong PIN");
    playFeedback("error");
    delay(1500);
  }

  isTypingPIN = false;
}

// ==================================================
// FUNCTION: getPinFromKeypad()
// PURPOSE : Prompt 4-digit PIN input via keypad with visual & audio feedback
// RETURNS : 4-digit PIN or "" if timed out
// ==================================================
String getPinFromKeypad(String label) {
  String pin = "";
  unsigned long startTime = millis();
  const unsigned long timeout = 15000;

  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("Enter " + label);
  lcd.setCursor(0, 1); lcd.print("PIN: ");

  while ((millis() - startTime) < timeout && pin.length() < 4) {
    char key = keypad.getKey();

    if (key != NO_KEY) {
      // 🧠 Handle digit input
      if (key >= '0' && key <= '9') {
        playKeyBeep();
        pin += key;
        lcd.setCursor(5 + pin.length() - 1, 1);
        lcd.print("*");
      }
      // 🔙 Backspace support using '#' or '*'
      else if ((key == '#' || key == '*') && pin.length() > 0) {
        playKeyBeep();
        pin.remove(pin.length() - 1);
        lcd.setCursor(5 + pin.length(), 1);
        lcd.print(" ");
        lcd.setCursor(5 + pin.length(), 1);
      }

      // ⏱ Reset timeout after key press
      startTime = millis();
    }

    delay(30);
  }

  if (pin.length() < 4) {
    lcd.clear();
    lcd.setCursor(3, 0); lcd.print("PIN Timed Out");
    playFeedback("error");
    delay(1500);
    return "";
  }

  return pin;
}

// ===================================================
// FUNCTION: getStudentIDFromPIN
// PURPOSE : Match entered PIN to student ID in Firebase
// RETURNS : Matching student ID (String) or "" if not found
// ===================================================
String getStudentIDFromPIN(String pin) {
  for (int id = 1001; id <= 1127; id++) {
    String path = firebasePath + "/" + String(id) + "/pin";

    if (Firebase.RTDB.getString(&firebaseData, path)) {
      if (firebaseData.stringData() == pin) {
        Serial.println("[✅] PIN matched to ID: " + String(id));
        return String(id);
      }
    } else {
      Serial.println("[⚠️] Failed to fetch PIN for ID: " + String(id));
    }

    delay(20);  // Reduce Firebase read flood
  }

  Serial.println("[❌] No student found with PIN: " + pin);
  return "";
}

// ============================================================================
// FUNCTION: getAllFeeKeys
// PURPOSE : Fills array with all 12 Firebase fee keys: "fee_jan", ..., "fee_dec"
// ============================================================================
void getAllFeeKeys(String arr[12]) {
  arr[0] = "fee_jan";
  arr[1] = "fee_feb";
  arr[2] = "fee_mar";
  arr[3] = "fee_apr";
  arr[4] = "fee_may";
  arr[5] = "fee_jun";
  arr[6] = "fee_jul";
  arr[7] = "fee_aug";
  arr[8] = "fee_sep";
  arr[9] = "fee_oct";
  arr[10] = "fee_nov";
  arr[11] = "fee_dec";
}

// =============================================================
// FUNCTION: showWelcomeScreen()
// PURPOSE : Display welcome message on LCD during system boot
// =============================================================
void showWelcomeScreen() {

  // ---------------- Display greeting on LCD ----------------
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(" WELCOME TO");
  lcd.setCursor(0, 1);
  lcd.print(" SMART INSTITUTE");

  // --------------Optional startup feedback---------------
  tone(BUZZER, 1000, 200);   // Short startup beep (optional)
  delay(3000);               // Hold message for 3 seconds
  lcd.clear();               // Clear screen for next message
}

// ===============================================
// Validate 4-digit PIN entered by user
// - Searches Firebase /students/{id}/PIN for match
// - Returns valid ID if match found, else -1
// ===============================================
int validatePIN(String pin) {
  if (Firebase.RTDB.getJSON(&firebaseData, firebasePath)) {
    FirebaseJson &data = firebaseData.jsonObject();

    size_t count = data.iteratorBegin();
    String key, value;
    int type;

    for (size_t i = 0; i < count; i++) {
      data.iteratorGet(i, type, key, value);  // Correct usage

      // Now 'key' is like "1001", "1002", etc.
      FirebaseJsonData pinResult;
      data.get(pinResult, key + "/PIN");

      if (pinResult.success && pinResult.stringValue == pin) {
        data.iteratorEnd();  // Good practice to free memory
        return key.toInt();  // Return matched ID
      }
    }

    data.iteratorEnd();
  }

  return -1;  // No match found
}

// ==========================================================
// FUNCTION: showWaitingMessage()
// PURPOSE : Alternates LCD display between:
//           1. Current Time and Date
//           2. Scan Finger / Enter PIN Prompt
//           Skips display while PIN is being typed.
// ==========================================================
void showWaitingMessage() {
  if (isTypingPIN) return;  // Don't interrupt active input

  static unsigned long lastSwitchTime = 0;
  static bool showClock = true;

  const unsigned long displayDuration = showClock ? 3000 : 4000;  // Time vs Prompt duration

  if (millis() - lastSwitchTime >= displayDuration) {
    lcd.clear();

    if (showClock) {
      // -------- Display Time and Date --------
      time_t now = time(nullptr);
      struct tm* t = localtime(&now);

      char timeStr[17];
      char dateStr[17];

      snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
      snprintf(dateStr, sizeof(dateStr), "%02d-%02d-%04d", t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);

      lcd.setCursor(0, 0);
      lcd.print(timeStr);
      lcd.setCursor(0, 1);
      lcd.print(dateStr);
    } 
    else {
      // -------- Display Prompt --------
      lcd.setCursor(0, 0);
      lcd.print("Scan Finger or");
      lcd.setCursor(0, 1);
      lcd.print(" # to Enter PIN ");
    }

    showClock = !showClock;
    lastSwitchTime = millis();
  }
}

// ==========================================================
// FUNCTION: connectToWiFi()
// PURPOSE : Connect to WiFi with LCD + Serial feedback
//           - Shows animated dots during retry
//           - Limits to 20 attempts (10 seconds)
// ==========================================================
void connectToWiFi() {
  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  Serial.print("Connecting to WiFi");

  int retries = 0;
  lcd.setCursor(0, 1);
  lcd.print("Connecting");

  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    lcd.print(".");
    Serial.print(".");
    delay(500);
    retries++;

    // Limit to 16 characters per line
    if (retries % 16 == 0) {
      lcd.setCursor(0, 1);
      lcd.print("Connecting    ");  // Reset line
    }
  }

  // -------------------- SUCCESS --------------------
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());  // Display IP address
  } 

  // -------------------- FAILURE --------------------
  else {
    Serial.println("\n❌ WiFi Failed!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Check Network!");
    delay(3000);
  }
}

// ===========================================================
// Get Current Date as String
// Format: YYYY-MM-DD
// ===========================================================
String getCurrentDate() {
  time_t now = time(nullptr);                  // Get current time
  struct tm* t = localtime(&now);              // Convert to local time structure

  char buf[11];                                // Buffer to hold date string
  sprintf(buf, "%04d-%02d-%02d", 
          t->tm_year + 1900,                   // Year (adjust from 1900)
          t->tm_mon + 1,                       // Month (0-11, so +1)
          t->tm_mday);                         // Day

  return String(buf);                          // Return formatted date
}

// ===========================================================
// Get Current Time as String
// Format: HH:MM:SS
// ===========================================================
String getCurrentTime() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);

  char buf[9];  // HH:MM:SS
  sprintf(buf, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
  return String(buf);
}

// ===========================================================
// Turn OFF All Status LEDs (Green, Orange, Red)
// ===========================================================
void turnOffAllLEDs() {
  digitalWrite(GREEN_LED, LOW);     // Turn off GREEN (Fee OK indicator)
  digitalWrite(ORANGE_LED, LOW);    // Turn off ORANGE (1-month due indicator)
  digitalWrite(RED_LED, LOW);       // Turn off RED (Blocked or error indicator)
}

// ==================
// FUNCTION: playKeyBeep()
// PURPOSE : Non-blocking audible click when keypad key is pressed
// ==================
void playKeyBeep() {
  tone(BUZZER, 1200);   // Start tone
  delayMicroseconds(50000);  // ≈50ms without blocking too long
  noTone(BUZZER);       // Stop tone
}

// ==================
// FUNCTION: playFeedback(String type)
// ==================
void playFeedback(String type) {
  feedbackType = type;
  feedbackStart = millis();
  feedbackActive = true;

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(ORANGE_LED, LOW);
  digitalWrite(RED_LED, LOW);
  noTone(BUZZER);

  if (type == "success") {
    digitalWrite(GREEN_LED, HIGH);
    tone(BUZZER, 1000);
  } else if (type == "warning") {
    digitalWrite(ORANGE_LED, HIGH);
    tone(BUZZER, 700);
  } else if (type == "error") {
    digitalWrite(RED_LED, HIGH);
    tone(BUZZER, 400);
  }
}
void updateFeedback() {
  if (!feedbackActive) return;

  unsigned long now = millis();
  if (feedbackType == "success" && now - feedbackStart >= 400) {
    noTone(BUZZER); digitalWrite(GREEN_LED, LOW); feedbackActive = false;
  }
  else if (feedbackType == "warning" && now - feedbackStart >= 500) {
    noTone(BUZZER); digitalWrite(ORANGE_LED, LOW); feedbackActive = false;
  }
  else if (feedbackType == "error" && now - feedbackStart >= 800) {
    noTone(BUZZER); digitalWrite(RED_LED, LOW); feedbackActive = false;
  }
}