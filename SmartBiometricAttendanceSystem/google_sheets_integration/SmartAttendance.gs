// ====== CONFIGURATION ======
const FIREBASE_URL = " "; //your firebase url
const FIREBASE_SECRET = " "; //your firebase secret key

function onOpen() {
  SpreadsheetApp.getUi().createMenu("🔥 Smart Attendance")
    .addItem("⬆️ Push Full Sheet to Firebase", "pushSheetDataToFirebase")
    .addItem("📌 Patch Only New Attendance", "patchNewAttendanceToFirebase")
    .addItem("⬇️ Fetch Attendance from Firebase", "appendNewAttendanceFromFirebase")
    .addToUi();
}

function formatTime(t) {
  if (!t) return "";
  if (typeof t === "string") return t;
  return Utilities.formatDate(new Date(t), Session.getScriptTimeZone(), "HH:mm:ss");
}

function formatDate(date) {
  return Utilities.formatDate(new Date(date), Session.getScriptTimeZone(), "yyyy-MM-dd");
}

function findOrCreateAttendanceRow(sheet, id, dateStr) {
  const data = sheet.getDataRange().getValues();
  for (let i = 1; i < data.length; i++) {
    if (data[i][0] == id && formatDate(data[i][17]) == dateStr) {
      return i;
    }
  }
  const newRow = [id, "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", new Date(dateStr), "", "", ""];
  sheet.appendRow(newRow);
  return sheet.getLastRow() - 1;
}


// ========== PUSH FULL SHEET TO FIREBASE ==========
function pushSheetDataToFirebase() {
  try {
    const sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName("Students");
    const data = sheet.getDataRange().getValues();
    let students = {};
    let currentID = null;

    for (let i = 1; i < data.length; i++) {
      const row = data[i];

      if (row[0] && row[1] && row[2] && row[3] && !isNaN(row[0])) {
        currentID = String(parseInt(row[0]));
        students[currentID] = {
          name: String(row[1]),
          pin: String(row[2]),
          phone_number: String(row[3]),
          finger_index: parseInt(row[4]),
          payments: {
            fee_jan: String(row[5] || ""),
            fee_feb: String(row[6] || ""),
            fee_mar: String(row[7] || ""),
            fee_apr: String(row[8] || ""),
            fee_may: String(row[9] || ""),
            fee_jun: String(row[10] || ""),
            fee_jul: String(row[11] || ""),
            fee_aug: String(row[12] || ""),
            fee_sep: String(row[13] || ""),
            fee_oct: String(row[14] || ""),
            fee_nov: String(row[15] || ""),
            fee_dec: String(row[16] || "")
          },
          attendance: {}
        };
      }

      // Check if attendance data is present
      else if (currentID && row[17] && row[18] && row[19]) {
        const dateStr = formatDate(row[17]);
        const time_in = formatTime(row[18]);
        const time_out = formatTime(row[19]);
        const method = String(row[20] || "Manual").toLowerCase();

        if (time_in === "" || time_out === "") continue;

        students[currentID].attendance[dateStr] = {
          time_in: time_in,
          time_out: time_out,
          method: method
        };

        Utilities.sleep(100); // Avoid quota issues
      }
    }

    const url = `${FIREBASE_URL}/students.json?auth=${FIREBASE_SECRET}`;
    const options = {
      method: "patch",
      contentType: "application/json",
      payload: JSON.stringify(students)
    };

    const response = UrlFetchApp.fetch(url, options);
    Logger.log(response.getContentText());
    SpreadsheetApp.getUi().alert("✅ Data PATCHED to Firebase successfully.");
  } catch (error) {
    Logger.log("❌ Firebase push error: " + error);
    SpreadsheetApp.getUi().alert("❌ Failed to push data.");
  }
}


function patchNewAttendanceToFirebase() {
  const sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName("Students");
  const data = sheet.getDataRange().getValues();
  let currentID = null;
  let newRecords = 0;

  for (let i = 1; i < data.length; i++) {
    const row = data[i];
    if (row[0]) {
      currentID = String(parseInt(row[0]));
    } else if (currentID && row[17]) {
      const dateStr = formatDate(row[17]);
      const attendance = {
        time_in: formatTime(row[18]),
        time_out: formatTime(row[19]),
        method: String(row[20] || "Manual")
      };

      const url = `${FIREBASE_URL}/students/${currentID}/attendance/${dateStr}.json?auth=${FIREBASE_SECRET}`;
      const options = {
        method: "patch",
        contentType: "application/json",
        payload: JSON.stringify(attendance)
      };

      try {
        UrlFetchApp.fetch(url, options);
        newRecords++;
      } catch (err) {
        Logger.log(`❌ Patch failed for ID ${currentID} on ${dateStr}`);
      }

      Utilities.sleep(100);
    }
  }

  SpreadsheetApp.getUi().alert(`✅ ${newRecords} attendance records patched to Firebase.`);
}

function appendNewAttendanceFromFirebase() {
  const sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName("Students");
  const data = sheet.getDataRange().getValues();

  const idToLastRow = {};  // Track last row in each student block
  const existingKeys = new Set();  // Track already recorded attendance "id_date"
  let currentID = null;

  // Step 1: Map last attendance row for each student
  for (let i = 1; i < data.length; i++) {
    const row = data[i];
    if (row[0]) {
      currentID = String(parseInt(row[0]));
    }
    if (currentID) {
      if (row[17] instanceof Date) {
        const dateKey = `${currentID}_${formatDate(row[17])}`;
        existingKeys.add(dateKey);
        idToLastRow[currentID] = i + 1; // Keep updating with last attendance row
      }
    }
  }

  // Step 2: Fetch attendance from Firebase and insert after last block
  for (let id = 1001; id <= 1100; id++) {
    const studentID = String(id);
    const url = `${FIREBASE_URL}/students/${studentID}/attendance.json?auth=${FIREBASE_SECRET}`;

    try {
      const res = UrlFetchApp.fetch(url);
      const attendanceData = JSON.parse(res.getContentText());
      if (!attendanceData) continue;

      // Sort attendance dates ascending
      const dates = Object.keys(attendanceData).sort();

      for (const dateStr of dates) {
        const key = `${studentID}_${dateStr}`;
        if (existingKeys.has(key)) continue;

        const entry = attendanceData[dateStr];
        const time_in = entry.time_in || "";
        const time_out = entry.time_out || "";
        const method = (entry.method || "Fingerprint").toLowerCase();

        const newRow = [
          "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", 
          new Date(dateStr), time_in, time_out, method
        ];

        const insertAt = (idToLastRow[studentID] || sheet.getLastRow()) + 1;
        sheet.insertRows(insertAt, 1);
        sheet.getRange(insertAt, 1, 1, newRow.length).setValues([newRow]);

        // Update last row for the student
        idToLastRow[studentID] = insertAt;

        Utilities.sleep(100);
      }
    } catch (e) {
      Logger.log(`❌ Error fetching for ID ${studentID}: ${e}`);
    }
  }

  SpreadsheetApp.getUi().alert("✅ New attendance records added at the end of each student block.");
}
