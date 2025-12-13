var sheet_id = "Id de hoja sheet";  // ID del spreadsheet

function doGet(e) {
  var ss = SpreadsheetApp.openById(sheet_id);
  
  // Verificar si el parámetro 'location' fue proporcionado
  var sheet_name = e.parameter.location;
  if (!sheet_name) {
    return ContentService.createTextOutput("Error: 'location' no especificado.").setMimeType(ContentService.MimeType.TEXT);
  }
  
  var sheet = ss.getSheetByName(sheet_name);
  if (!sheet) {
    sheet = ss.insertSheet(sheet_name);
    if (!sheet) {
      return ContentService.createTextOutput("Error: No se pudo crear la hoja '" + sheet_name + "'.").setMimeType(ContentService.MimeType.TEXT);
    }
    sheet.appendRow(["Fecha y Hora", "Temperatura", "Humedad", "Presión", "IAQ", "Precisión IAQ", "IAQ Estático", "CO2 Equivalente", "VOC Equivalente", "Temperatura Raw", "Humedad Raw", "Resistencia Gas", "Estab. Status", "Run-in Status", "Porcentaje Gas"]);
  }
  
  // --- Lógica de Fecha Híbrida CORREGIDA ---
  var fechaHora;
  
  // 1. Detección robusta de Offline
  var isOffline = String(e.parameter.offline_flag).toLowerCase() === 'true';

  // 2. BUSCAMOS LA FECHA EN AMBOS NOMBRES (fechaHora O timestamp)
  // Esto arregla el problema de comunicación
  var fechaRecibida = e.parameter.fechaHora || e.parameter.timestamp;

  if (isOffline && fechaRecibida) {
    // Limpieza: Quitamos %20, %3A, T, Z
    var horaLimpia = fechaRecibida.replace(/%20/g, " ").replace(/%3A/g, ":").replace("T", " ").replace("Z", "");
    
    // Formateo Latino: YYYY-MM-DD HH:MM:SS -> DD/MM/YYYY HH:MM:SS
    var partes = horaLimpia.split(" ");
    var fechaPartes = partes[0].split("-");
    
    // Validamos que la fecha tenga sentido antes de procesarla
    if (fechaPartes.length === 3) {
         var fechaFormateada = fechaPartes[2] + "/" + fechaPartes[1] + "/" + fechaPartes[0] + " " + partes[1];
         fechaHora = "'" + fechaFormateada; // ' para forzar texto
    } else {
         fechaHora = "'" + horaLimpia; // Si falla el formato, poner lo que llegó
    }

  } else {
    // En Vivo -> Hora del servidor
    fechaHora = new Date(); 
  }
  
  // Parseo de números
  var temperature = parseNumber(e.parameter.temperature);
  var humidity = parseNumber(e.parameter.humidity);
  var pressure = parseNumber(e.parameter.pressure);
  var iaq = parseNumber(e.parameter.iaq);
  var iaqAccuracy = parseNumber(e.parameter.iaqAccuracy);
  var staticIaq = parseNumber(e.parameter.staticIaq);
  var co2Equivalent = parseNumber(e.parameter.co2Equivalent);
  var breathVocEquivalent = parseNumber(e.parameter.breathVocEquivalent);
  var rawTemperature = parseNumber(e.parameter.rawTemperature);
  var rawHumidity = parseNumber(e.parameter.rawHumidity);
  var gasResistance = parseNumber(e.parameter.gasResistance);
  var stabStatus = parseNumber(e.parameter.stabStatus);
  var runInStatus = parseNumber(e.parameter.runInStatus);
  var gasPercentage = e.parameter.gasPercentage || "";

  sheet.appendRow([
    fechaHora, 
    temperature, humidity, pressure, iaq, iaqAccuracy, staticIaq, co2Equivalent, breathVocEquivalent, 
    rawTemperature, rawHumidity, gasResistance, stabStatus, runInStatus, gasPercentage
  ]);
  
  return ContentService.createTextOutput("Datos recibidos. Offline: " + isOffline).setMimeType(ContentService.MimeType.TEXT);
}

function parseNumber(value) {
  if (value === null || value === undefined || value === "") return "";
  var num = Number(value);
  return isNaN(num) ? "" : num;
}