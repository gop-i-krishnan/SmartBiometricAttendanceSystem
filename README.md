# Smart Biometric Attendance System 📚🔐

> 🔥 A fingerprint-based IoT attendance system integrated with Firebase Realtime Database and Google Sheets, built using ESP32 and R307 sensor.

---

## 📌 Project Overview

This is a complete **smart biometric attendance system** designed to digitize and automate the attendance process for educational institutions or small organizations.

The system uses:
- ✅ **Fingerprint authentication** using the R307 module
- ✅ **ESP32** to handle logic, LCD, keypad, and Firebase communication
- ✅ **Google Sheets integration** via **Apps Script**
- ✅ **Admin features** (Enroll/Delete fingerprints with button + PIN)
- ✅ **Fee validation system** (marks blocked if >1 month unpaid)
- ✅ **Buzzer + LED feedback** for instant alerts
- ✅ **Full LCD prompts** to guide users

---

## 🧠 Features

- 🔐 Fingerprint-based attendance marking
- 🧾 PIN-based login (via 4x4 keypad)
- 📊 Fee status check per month (Jan–Dec)
- ✅ "Welcome", "Blocked", or "1-month pending" alerts
- 💡 LEDs: Green = OK, Orange = 1-month pending, Red = Blocked
- 📤 Logs `time_in`, `time_out`, and method of login
- 📈 Realtime data in Firebase & synced to Google Sheets

---

## 📦 Folder Structure

```

SmartBiometricAttendanceSystem/
│
├── source\_code/
│   ├── main\_code.ino             # Arduino ESP32 code
│   ├── SmartAttendance.gs        # Google Apps Script for Sheet sync
│
├── data/
│   ├── Firebase\_StudentData.json # Sample Firebase DB format
│   └── SmartInstitute\_StudentDetails\_2025(firebasesync).xlsx
│
├── documentation/
│   ├── README.md                 # This file
│   ├── Screenshots/             # Optional: Images of LCD/Firebase
│
├── video/
│   └── Final\_Working\_Demo.mp4   # Malayalam explanation video
│
└── LICENSE                      # MIT license

````

---

## 🔧 Tech Stack

| Component | Description |
|----------|-------------|
| ESP32 | Core controller |
| R307 | Fingerprint sensor |
| Firebase Realtime DB | Cloud storage |
| Google Apps Script | Sync with Google Sheets |
| I2C LCD 16x2 | User display |
| 4x4 Keypad | PIN entry |
| LEDs + Buzzer | Feedback system |

---

## 🔌 Hardware Connections

| Component | Pins |
|----------|------|
| R307 Fingerprint | TX → GPIO16, RX → GPIO17 |
| I2C LCD | SDA → GPIO21, SCL → GPIO22 |
| Keypad | Rows → GPIO32,33,25,26; Cols → GPIO27,14,12,13 |
| LEDs | Green → GPIO4, Orange → GPIO18, Red → GPIO5 |
| Buzzer | GPIO19 |
| Push Button | GPIO15 |

---

## 🔗 Firebase Setup

1. Create a Firebase Realtime DB project
2. Structure:

```json
students: {
  1001: {
    name: "John",
    pin: "1234",
    finger_index: 1,
    fee_jan: "paid",
    ...
  },
  ...
}
````

3. Add your `FIREBASE_HOST` and `FIREBASE_AUTH` in `main_code.ino`
   🔐 **DO NOT commit your actual credentials. Mask them or use a `secrets.h` file.**

---

## 📑 Google Sheet Integration

* A Google Sheet is used to mirror student details and attendance.
* Linked using the **`SmartAttendance.gs`** Apps Script file.
* Automatically updates fields like `attendance`, `time_in`, `time_out`, etc.

---

## 📽️ Video Demonstration

🎥 Watch the full system in action (Malayalam explanation):
**[Final\_Working\_Demo.mp4](video/Final_Working_Demo.mp4)**

---

## 👨‍💻 Author

* **Name:** Gopi (aka LifeCoder0)
* **Branch:** ECE 4th Semester, Kerala
* **Vision:** Early success through tech + passive income strategies

---

## 📝 License

This project is licensed under the **MIT License** – feel free to fork and build on it!

---

## 💡 Future Improvements

* Admin dashboard web app
* Auto email/SMS alerts on blocked status
* Multi-classroom or teacher modules
* Voice feedback via speaker

---

## 📬 Contact

📧 Reach me via [LinkedIn](https://www.linkedin.com/)

```

