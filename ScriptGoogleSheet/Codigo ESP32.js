var sheet_id = "xxxxxxxxxx";  // ID del spreadsheet

function doGet(e) {
  var ss = SpreadsheetApp.openById(sheet_id);
  
  // Obtener el nombre de la hoja de la solicitud (parámetro "location")
  var sheet_name = e.parameter.location;
  
  // Verificar si el parámetro 'location' fue proporcionado
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
    sheet.appendRow(["Fecha", "Temperatura", "Humedad", "Presión", "IAQ", "Precisión IAQ", "IAQ Estático", "CO2 Equivalente", "VOC Equivalente", "Temperatura Raw", "Humedad Raw", "Resistencia Gas", "Estab. Status", "Run-in Status", "Porcentaje Gas"]);
  }
  
  // Obtener los parámetros y convertirlos en números cuando sea necesario
  var temperature = Number(e.parameter.temperature);
  var humidity = e.parameter.humidity ? Number(e.parameter.humidity) : "";
  var pressure = e.parameter.pressure ? Number(e.parameter.pressure) : "";
  var iaq = e.parameter.iaq ? Number(e.parameter.iaq) : "";
  var iaqAccuracy = e.parameter.iaqAccuracy ? Number(e.parameter.iaqAccuracy) : "";
  var staticIaq = e.parameter.staticIaq ? Number(e.parameter.staticIaq) : "";
  var co2Equivalent = e.parameter.co2Equivalent ? Number(e.parameter.co2Equivalent) : "";
  var breathVocEquivalent = e.parameter.breathVocEquivalent ? Number(e.parameter.breathVocEquivalent) : "";
  var rawTemperature = e.parameter.rawTemperature ? Number(e.parameter.rawTemperature) : "";
  var rawHumidity = e.parameter.rawHumidity ? Number(e.parameter.rawHumidity) : "";
  var gasResistance = e.parameter.gasResistance ? Number(e.parameter.gasResistance) : "";
  var stabStatus = e.parameter.stabStatus ? Number(e.parameter.stabStatus) : "";
  var runInStatus = e.parameter.runInStatus ? Number(e.parameter.runInStatus) : "";
  var gasPercentage = e.parameter.gasPercentage ? e.parameter.gasPercentage : "";

  // Escribir los datos en la hoja correspondiente
  sheet.appendRow([new Date(), temperature, humidity, pressure, iaq, iaqAccuracy, staticIaq, co2Equivalent, breathVocEquivalent, rawTemperature, rawHumidity, gasResistance, stabStatus, runInStatus, gasPercentage]);
  
  // Devolver una respuesta de éxito
  return ContentService.createTextOutput("Datos recibidos y registrados en la hoja '" + sheet_name + "'").setMimeType(ContentService.MimeType.TEXT);
}
