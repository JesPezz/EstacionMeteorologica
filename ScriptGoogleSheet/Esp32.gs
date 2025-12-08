var sheet_id = "12ttM1jJPRWgpgqWCg6ApXyouKTo7fzviuX42mmqxS1Q";  // ID del spreadsheet

function doGet(e) {
  var ss = SpreadsheetApp.openById(sheet_id);
  
  // Verificar si el parámetro 'location' fue proporcionado
  var sheet_name = e.parameter.location;
  if (!sheet_name) {
    return ContentService.createTextOutput("Error: 'location' no especificado.").setMimeType(ContentService.MimeType.TEXT);
  }
  
  // Intentar obtener la hoja correspondiente
  var sheet = ss.getSheetByName(sheet_name);
  
  // Si la hoja no existe, crearla
  if (!sheet) {
    sheet = ss.insertSheet(sheet_name);
    
    // Si no se puede crear más hojas, lanzar un error
    if (!sheet) {
      return ContentService.createTextOutput("Error: No se pudo crear la hoja '" + sheet_name + "'.").setMimeType(ContentService.MimeType.TEXT);
    }
    
    // Agregar los encabezados a la nueva hoja (opcional)
    sheet.appendRow([
      "Fecha y Hora", 
      "Temperatura", 
      "Humedad", 
      "Presión", 
      "IAQ", 
      "Precisión IAQ", 
      "IAQ Estático", 
      "CO2 Equivalente", 
      "VOC Equivalente", 
      "Temperatura Raw", 
      "Humedad Raw", 
      "Resistencia Gas", 
      "Estab. Status", 
      "Run-in Status", 
      "Porcentaje Gas"
    ]);
  }
  
  // --- Lógica de Fecha Híbrida ---
  var fechaHora;
  
  // Verificamos si es un dato recuperado (Offline)
  var isOffline = e.parameter.offline_flag === 'true';

  if (isOffline && e.parameter.fechaHora) {
    // CASO A: Es Offline -> Respetamos la hora del ESP32
    // Asume formato "YYYY-MM-DD HH:MM:SS" y añade la Z para UTC o ajusta según tu zona
    fechaHora = new Date(e.parameter.fechaHora.replace(" ", "T")); 
  } else {
    // CASO B: Es En Vivo (o no trae fecha) -> Usamos la hora de recepción (Servidor Google)
    fechaHora = new Date(); 
  }
  
  // Obtener los parámetros y convertirlos en números cuando sea necesario
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

  // Escribir los datos en la hoja correspondiente
  sheet.appendRow([
    fechaHora, // Fecha y hora
    temperature, 
    humidity, 
    pressure, 
    iaq, 
    iaqAccuracy, 
    staticIaq, 
    co2Equivalent, 
    breathVocEquivalent, 
    rawTemperature, 
    rawHumidity, 
    gasResistance, 
    stabStatus, 
    runInStatus, 
    gasPercentage
  ]);
  
  // Devolver una respuesta de éxito
  return ContentService.createTextOutput("Datos recibidos y registrados en la hoja '" + sheet_name + "'").setMimeType(ContentService.MimeType.TEXT);
}

// Función para convertir un valor a número (o devolver null si no es válido)
function parseNumber(value) {
  if (value === null || value === undefined || value === "") {
    return "";
  }
  var num = Number(value);
  return isNaN(num) ? "" : num;
}
