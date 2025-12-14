function copia() {
  // --- CONFIGURACIÓN SEGURA ---
  const scriptProperties = PropertiesService.getScriptProperties();
  
  const telegramBotToken = scriptProperties.getProperty('TELEGRAM_TOKEN');
  const telegramChatId = scriptProperties.getProperty('TELEGRAM_CHAT_ID');
  const idLibro = scriptProperties.getProperty('SHEET_ID');
  // Si no configuraste el email en propiedades, usa este por defecto:
  const emailDestino = scriptProperties.getProperty('EMAIL_DESTINO') || "jespezz@hotmail.com";
  // ------------------------------

  try {
    var fechaActual = new Date();

    // Verificamos si es 1 de Enero (Mes 0, Día 1)
    if (fechaActual.getDate() === 1 && fechaActual.getMonth() === 0) {
      
      var libroActual = SpreadsheetApp.openById(idLibro);
      var añoAnterior = fechaActual.getFullYear() - 1;
      
      // Hojas que NO se deben tocar
      var hojasOmitir = ["G.U.P.A", "G.U.P.B", "Comparativas PA y PB", "GraficoPA", "Grafico 2", "Config"]; 

      // 1. Crear la copia de seguridad
      var nombreCopia = "Registro de Temperatura_" + añoAnterior;
      var copiaLibro = libroActual.copy(nombreCopia);
      
      if (!copiaLibro) {
        throw new Error("No se pudo crear la copia de seguridad.");
      }
      
      var hojasOriginales = libroActual.getSheets();
      var errores = [];
      var hojasLimpiadas = [];
      var hojasOmitidasLog = [];

      // 2. Limpiar el libro original
      for (var i = 0; i < hojasOriginales.length; i++) {
        var hoja = hojasOriginales[i];
        var nombreHoja = hoja.getName();

        try {
          if (hojasOmitir.indexOf(nombreHoja) === -1) {
            var ultimaFila = hoja.getLastRow();
            if (ultimaFila > 1) {
              // Borrar desde fila 2 hasta el final
              hoja.getRange(2, 1, ultimaFila - 1, hoja.getLastColumn()).clearContent();
              hojasLimpiadas.push(nombreHoja);
            }
          } else {
            hojasOmitidasLog.push(nombreHoja);
          }
        } catch (e) {
          errores.push('Error en hoja "' + nombreHoja + '": ' + e.message);
        }
      }

      // 3. Preparar Mensajes (VERSIÓN HTML)
      var resumenTelegram = `✅ <b>Mantenimiento Anual ${añoAnterior} Completado</b>\n\n` +
                            `📂 <b>Backup:</b> ${nombreCopia}\n` +
                            `🧹 <b>Hojas limpiadas:</b> ${hojasLimpiadas.length}\n` +
                            `🛡️ <b>Hojas omitidas:</b> ${hojasOmitidasLog.length}\n` +
                            `⚠️ <b>Errores:</b> ${errores.length > 0 ? errores.length : "Ninguno"}`;;

      var cuerpoCorreo = "El proceso de archivo anual ha finalizado.\n\n" +
                         "📂 Backup creado: " + nombreCopia + "\n" +
                         "🔗 URL Backup: " + copiaLibro.getUrl() + "\n\n" +
                         "🧹 Hojas limpiadas: " + hojasLimpiadas.join(", ") + "\n" +
                         "🛡️ Hojas omitidas: " + hojasOmitidasLog.join(", ") + "\n" +
                         "⚠️ Errores: " + (errores.length > 0 ? errores.join("\n") : "Ninguno");

      // Enviar Notificaciones (Usando variables)
      MailApp.sendEmail(emailDestino, "✅ Mantenimiento Anual Completado: " + añoAnterior, cuerpoCorreo);
      enviarTelegram(telegramBotToken, telegramChatId, resumenTelegram);

      Logger.log("Proceso terminado exitosamente.");

    } else {
      Logger.log('Hoy no es 1 de enero. No se requiere mantenimiento.');
    }
  } catch (e) {
    // Notificación de Error Crítico
    var msgError = `❌ *ERROR CRÍTICO EN MANTENIMIENTO ANUAL*\n\nEl script ha fallado: ${e.message}`;
    MailApp.sendEmail(emailDestino, "❌ ERROR CRÍTICO en Script Anual", e.message);
    enviarTelegram(telegramBotToken, telegramChatId, msgError);
    Logger.log('Error fatal: ' + e.message);
  }
}

// --- Función Auxiliar para Telegram ---
function enviarTelegram(token, chatId, mensaje) {
  if (!token || !chatId) return; // Validación simple
  
  try {
    const url = `https://api.telegram.org/bot${token}/sendMessage`;
    const payload = {
      'chat_id': chatId,
      'text': mensaje,
      'parse_mode': 'HTML'
    };
    const options = {
      'method': 'post',
      'contentType': 'application/json',
      'payload': JSON.stringify(payload)
    };
    UrlFetchApp.fetch(url, options);
  } catch (e) {
    Logger.log("Error enviando Telegram: " + e.message);
  }
}