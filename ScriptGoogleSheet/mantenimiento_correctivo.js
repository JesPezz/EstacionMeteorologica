/**
 * SCRIPT DE MANTENIMIENTO CORRECTIVO (Dinámico)
 * ---------------------------------------------
 * 1. Detecta automáticamente el AÑO ACTUAL.
 * 2. Cualquier fecha distinta al año actual se considera ERROR.
 * 3. Corrige fechas erróneas (+1 Hora) y borra duplicados.
 */

function corregirYLimpiarDatos() {
  // --- CONFIGURACIÓN ---
  var archivoId = ""; 
  const telegramBotToken = ""; 
  const telegramChatId = ""; 
  
  var hojas = ["PlantaBaja", "PlantaAlta", "Exterior", "Recamara"];
  // ---------------------

  var spreadsheet = SpreadsheetApp.openById(archivoId);
  var logTelegram = "";
  var totalCorregidos = 0;
  var totalEliminados = 0;
  
  // 🕒 OBTENER AÑO ACTUAL DINÁMICAMENTE
  var fechaHoy = new Date();
  var anioActual = fechaHoy.getFullYear(); 

  hojas.forEach(function(nombreHoja) {
    try {
      // Pasamos el anioActual a la función procesadora
      var resultado = procesarHoja(spreadsheet, nombreHoja, anioActual);
      
      if (resultado.corregidos > 0 || resultado.eliminados > 0) {
        logTelegram += `🔹 <b>${nombreHoja}:</b>\n`;
        if (resultado.corregidos > 0) logTelegram += `   - 🛠️ Fechas corregidas: ${resultado.corregidos}\n`;
        if (resultado.eliminados > 0) logTelegram += `   - 🗑️ Duplicados borrados: ${resultado.eliminados}\n`;
        
        totalCorregidos += resultado.corregidos;
        totalEliminados += resultado.eliminados;
      } else {
        logTelegram += `🔹 <b>${nombreHoja}:</b> Limpia ✅\n`;
      }
    } catch (e) {
      logTelegram += `⚠️ <b>${nombreHoja}:</b> Error (${e.message})\n`;
    }
  });

  if (totalCorregidos > 0 || totalEliminados > 0) {
    var mensaje = `🚑 <b>Mantenimiento Correctivo (${anioActual})</b>\n\n` +
                  `🛠️ Total Corregidos: ${totalCorregidos}\n` +
                  `🗑️ Total Eliminados: ${totalEliminados}\n\n` +
                  `Detalle:\n${logTelegram}`;
    enviarTelegram(telegramBotToken, telegramChatId, mensaje);
  } else {
    Logger.log("Sistema limpio. No se requirieron correcciones.");
  }
}

function procesarHoja(spreadsheet, nombreHoja, anioEsperado) {
  var hoja = spreadsheet.getSheetByName(nombreHoja);
  if (!hoja) return { corregidos: 0, eliminados: 0 };

  var datos = hoja.getDataRange().getValues();
  if (datos.length <= 1) return { corregidos: 0, eliminados: 0 };

  var filasParaActualizar = []; 
  var filasParaBorrar = [];     

  var ultimoFechaValida = null; 
  var ONE_HOUR_MS = 3600 * 1000;

  for (var i = 1; i < datos.length; i++) {
    var fila = datos[i];
    var fechaLeida = new Date(fila[0]);
    
    // --- LÓGICA DINÁMICA ---
    // ¿La fecha coincide con el año en curso?
    // Si estamos en 2025, solo aceptamos 2025. 1970 o 2024 son errores.
    var esFechaValida = !isNaN(fechaLeida.getTime()) && fechaLeida.getFullYear() === anioEsperado;

    if (esFechaValida) {
      ultimoFechaValida = fechaLeida;
    } else {
      // FECHA INCORRECTA (Distinta al año actual)
      
      // A) CHECK DUPLICADO
      var esDuplicado = false;
      if (i > 1) {
        var filaAnterior = datos[i-1];
        if (fila[1] === filaAnterior[1] && 
            fila[2] === filaAnterior[2] && 
            fila[3] === filaAnterior[3]) {
          esDuplicado = true;
        }
      }

      if (esDuplicado) {
        filasParaBorrar.push(i);
        continue; 
      }

      // B) CÁLCULO DE FECHA CORRECTA (+1 Hora)
      var nuevaFecha = null;
      
      if (ultimoFechaValida !== null) {
        nuevaFecha = new Date(ultimoFechaValida.getTime() + ONE_HOUR_MS);
      } else {
        // Mirar al futuro buscando una fecha que SÍ sea del año actual
        var fechaFutura = null;
        var distancia = 0;
        for (var j = i + 1; j < datos.length; j++) {
           var f = new Date(datos[j][0]);
           if (!isNaN(f.getTime()) && f.getFullYear() === anioEsperado) {
             fechaFutura = f;
             distancia = j - i;
             break;
           }
        }
        if (fechaFutura) {
          nuevaFecha = new Date(fechaFutura.getTime() - (distancia * ONE_HOUR_MS));
        }
      }

      if (nuevaFecha) {
        filasParaActualizar.push({ indice: i, fecha: nuevaFecha });
        ultimoFechaValida = nuevaFecha; 
      }
    }
  }

  // EJECUCIÓN: ACTUALIZAR
  filasParaActualizar.forEach(function(item) {
    hoja.getRange(item.indice + 1, 1).setValue(item.fecha);
  });

  // EJECUCIÓN: BORRAR (Orden inverso)
  filasParaBorrar.sort(function(a, b) { return b - a; });
  filasParaBorrar.forEach(function(indiceArray) {
    hoja.deleteRow(indiceArray + 1);
  });

  return { corregidos: filasParaActualizar.length, eliminados: filasParaBorrar.length };
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