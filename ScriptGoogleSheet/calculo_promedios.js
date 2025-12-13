/**
 * SCRIPT DE CÁLCULO DE PROMEDIOS HISTÓRICOS v3.1 (Edición Dinámica)
 * ------------------------------------------------------------------
 * - Genera hojas de "Promedios Mensuales [Nombre]" agrupando por Mes y Año.
 * - Detecta automáticamente el AÑO ACTUAL del sistema.
 * - Corrige fechas inválidas (distintas al año actual) usando lógica de +1 Hora.
 * - Elimina duplicados basura en fechas inválidas.
 * - Notifica resultados a Telegram.
 */

function calcularPromediosMensuales() {
  // --- CONFIGURACIÓN ---
  var archivoId = ""; 
  const telegramBotToken = ""; 
  const telegramChatId = ""; 
  
  // Hojas a procesar
  var hojas = ["PlantaBaja", "PlantaAlta", "Exterior", "Recamara"];
  // ---------------------

  var archivoSpreadsheet = SpreadsheetApp.openById(archivoId);
  var hojasActualizadas = 0;
  var totalCorregidos = 0;
  var totalIgnorados = 0;
  var logTelegram = "";

  // 🕒 OBTENER AÑO ACTUAL DINÁMICAMENTE
  // Esto hace que el script funcione para siempre (2025, 2026...)
  var anioSistema = new Date().getFullYear();

  // Procesar cada hoja
  hojas.forEach(function(hojaRegNombre) {
    try {
      // Pasamos el anioSistema a la función
      var resultado = calcularPromediosParaHoja(archivoSpreadsheet, hojaRegNombre, anioSistema);
      
      if (resultado.exito) {
        hojasActualizadas++;
        var detalles = [];
        if (resultado.corregidos > 0) detalles.push(`🛠️ ${resultado.corregidos} fixed`);
        if (resultado.ignorados > 0) detalles.push(`🗑️ ${resultado.ignorados} dups`);
        
        var textoExtra = detalles.length > 0 ? `(${detalles.join(", ")})` : "✅ OK";
        logTelegram += `🔹 <b>${hojaRegNombre}:</b> ${textoExtra}\n`;
        
        totalCorregidos += resultado.corregidos;
        totalIgnorados += resultado.ignorados;
      }
    } catch (e) {
      logTelegram += `⚠️ <b>${hojaRegNombre}:</b> Error (${e.message})\n`;
    }
  });

  // Notificar a Telegram
  var mensaje = `📉 <b>Cálculo de Promedios Finalizado (${anioSistema})</b>\n\n` +
                `✅ Hojas procesadas: ${hojasActualizadas}/${hojas.length}\n` +
                `🔧 Fechas reparadas: ${totalCorregidos}\n` +
                `🚫 Duplicados eliminados: ${totalIgnorados}\n\n` +
                `Detalle:\n${logTelegram}`;
                
  enviarTelegram(telegramBotToken, telegramChatId, mensaje);
}

function calcularPromediosParaHoja(archivoSpreadsheet, hojaRegNombre, anioValido) {
  var hojaPromediosNombre = "Promedios Mensuales " + hojaRegNombre;
  
  var hojaReg = archivoSpreadsheet.getSheetByName(hojaRegNombre);
  if (!hojaReg) return { exito: false, corregidos: 0, ignorados: 0 };
  
  // Crear hoja si no existe
  var hojaPromedios = archivoSpreadsheet.getSheetByName(hojaPromediosNombre);
  if (!hojaPromedios) {
    hojaPromedios = archivoSpreadsheet.insertSheet(hojaPromediosNombre);
    archivoSpreadsheet.setActiveSheet(hojaPromedios);
    archivoSpreadsheet.moveActiveSheet(archivoSpreadsheet.getSheets().length);
  }
  
  // Encabezados
  var encabezados = [["Mes", "Año", "Prom. Temp", "Prom. Hum", "Prom. Pres", "Prom. IAQ"]];
  if (hojaPromedios.getLastRow() === 0 || hojaPromedios.getRange(1, 1).getValue() !== "Mes") {
    hojaPromedios.getRange(1, 1, 1, 6).setValues(encabezados);
    hojaPromedios.getRange(1, 1, 1, 6).setFontWeight("bold").setBackground("#E0E0E0");
  }
  
  var datosHojaReg = hojaReg.getDataRange().getValues();
  if (datosHojaReg.length <= 1) return { exito: false, corregidos: 0, ignorados: 0 };

  var promedios = [];
  var mesActual = -1;
  var anioActual = -1;
  var sumas = { temp: 0, hum: 0, pres: 0, iaq: 0, count: 0 };
  
  // --- VARIABLES DE LÓGICA DE TIEMPO ---
  var ultimoFechaValida = null; 
  var ONE_HOUR_MS = 3600 * 1000;
  var contadorCorrecciones = 0;
  var contadorIgnorados = 0; 

  for (var i = 1; i < datosHojaReg.length; i++) {
    var fila = datosHojaReg[i];
    var fechaLeida = new Date(fila[0]);
    var fechaProcesada = null; 

    if (isNaN(fechaLeida.getTime())) continue;

    // =================================================================
    // 🧠 CEREBRO INTELIGENTE: CORRECCIÓN DINÁMICA
    // =================================================================
    
    // CASO 1: FECHA VÁLIDA (Coincide con el año actual del sistema)
    if (fechaLeida.getFullYear() === anioValido) {
      fechaProcesada = fechaLeida;
      ultimoFechaValida = fechaLeida;
    } 
    // CASO 2: FECHA INVÁLIDA (1970, año pasado, etc.)
    else {
      // A) DETECCIÓN DE DUPLICADOS
      if (i > 1) {
        var filaAnterior = datosHojaReg[i-1];
        if (fila[1] === filaAnterior[1] && 
            fila[2] === filaAnterior[2] && 
            fila[3] === filaAnterior[3]) {
              contadorIgnorados++;
              continue; // 🚫 SALTAR FILA
        }
      }

      // B) CORRECCIÓN DE TIEMPO (+1 Hora)
      if (ultimoFechaValida !== null) {
        fechaProcesada = new Date(ultimoFechaValida.getTime() + ONE_HOUR_MS);
        ultimoFechaValida = fechaProcesada; 
        contadorCorrecciones++;
      } else {
        // Mirar al futuro buscando una fecha del AÑO ACTUAL
        var fechaFutura = null;
        var distanciaFilas = 0;
        
        for (var j = i + 1; j < datosHojaReg.length; j++) {
           var f = new Date(datosHojaReg[j][0]);
           // Solo aceptamos fechas futuras que coincidan con el año actual
           if (!isNaN(f.getTime()) && f.getFullYear() === anioValido) {
             fechaFutura = f;
             distanciaFilas = j - i;
             break;
           }
        }

        if (fechaFutura) {
          fechaProcesada = new Date(fechaFutura.getTime() - (distanciaFilas * ONE_HOUR_MS));
          ultimoFechaValida = fechaProcesada;
          contadorCorrecciones++;
        } else {
          continue; 
        }
      }
    }
    // =================================================================

    var mes = fechaProcesada.getMonth();
    var anio = fechaProcesada.getFullYear();

    if (mes !== mesActual || anio !== anioActual) {
      if (mesActual !== -1) {
        pushPromedio(promedios, mesActual, anioActual, sumas);
      }
      mesActual = mes;
      anioActual = anio;
      sumas = { temp: 0, hum: 0, pres: 0, iaq: 0, count: 0 };
    }
    
    if (typeof fila[1] === 'number') sumas.temp += fila[1];
    if (typeof fila[2] === 'number') sumas.hum += fila[2];
    if (typeof fila[3] === 'number') sumas.pres += fila[3];
    var valIaq = (typeof fila[4] === 'number') ? fila[4] : 0; 
    sumas.iaq += valIaq;
    sumas.count++;
  }
  
  if (sumas.count > 0) {
    pushPromedio(promedios, mesActual, anioActual, sumas);
  }
  
  if (hojaPromedios.getLastRow() > 1) {
    hojaPromedios.getRange(2, 1, hojaPromedios.getLastRow() - 1, 6).clearContent();
  }
  
  if (promedios.length > 0) {
    hojaPromedios.getRange(2, 1, promedios.length, 6).setValues(promedios);
  }
  
  return { exito: true, corregidos: contadorCorrecciones, ignorados: contadorIgnorados };
}

function pushPromedio(arrayPromedios, mes, anio, sumas) {
  var count = sumas.count > 0 ? sumas.count : 1;
  arrayPromedios.push([
    obtenerNombreMes(mes),
    anio,
    redondear(sumas.temp / count),
    redondear(sumas.hum / count),
    redondear(sumas.pres / count),
    redondear(sumas.iaq / count)
  ]);
}

function obtenerNombreMes(mes) {
  var meses = ["Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"];
  return meses[mes] || "Desc";
}

function redondear(valor) {
  return Math.round(valor * 100) / 100;
}

function enviarTelegram(token, chatId, mensaje) {
  if (token.length < 20) return;
  try {
    const url = `https://api.telegram.org/bot${token}/sendMessage`;
    const payload = { 'chat_id': chatId, 'text': mensaje, 'parse_mode': 'HTML' };
    const options = { 'method': 'post', 'contentType': 'application/json', 'payload': JSON.stringify(payload) };
    UrlFetchApp.fetch(url, options);
  } catch (e) {
    Logger.log("Error Telegram: " + e.message);
  }
}