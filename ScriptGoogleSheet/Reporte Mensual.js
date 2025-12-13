 function enviarResumenConHTML() {
  // --- CONFIGURACIÓN ---
  const nombresHojas = ["nombrhoja"];
  const emailDestino = "tu email";
  
  // 🤖 CONFIGURACIÓN TELEGRAM
  const telegramBotToken = ""; // Ej: "123456789:AAFw..."
  const telegramChatId = ""; // Ej: "12345678"
  
  // 🚫 COLUMNAS A IGNORAR
  const columnasIgnorar = ["iaqAccuracy", "stabilizationStatus", "runInStatus", "gasPercentage", "gasResistance", "staticIaq", "co2Equivalent", "raw temperature [°C]", "raw humidity [%]", "breathVocEquivalent"];
  
  // Límite de columnas
  const numSensores = 10; 
  // ---------------------

  const nombresMeses = ["Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"];

  const fechaActual = new Date();
  const diaActual = fechaActual.getDate();
  const mesActual = fechaActual.getMonth(); 
  const anioActual = fechaActual.getFullYear();

  let mesResumen, anioResumen;
  
  if (diaActual === 1) {
    mesResumen = mesActual === 0 ? 11 : mesActual - 1; 
    anioResumen = mesActual === 0 ? anioActual - 1 : anioActual; 
  } else {
    mesResumen = mesActual;
    anioResumen = anioActual;
  }

  const nombreMesResumen = nombresMeses[mesResumen];

  // --- HTML PARA CORREO (Estilo Simple) ---
  let cuerpoHTML = `
    <html>
      <head>
        <style>
          body { font-family: 'Segoe UI', Arial, sans-serif; background-color: #f4f4f4; color: #333; }
          .container { max-width: 700px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; }
          h1 { color: #2E7D32; text-align: center; border-bottom: 2px solid #2E7D32; }
          h2 { color: #1565C0; margin-top: 25px; border-left: 5px solid #1565C0; padding-left: 10px; }
          table { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 0.9em; }
          th { background-color: #444; color: white; padding: 10px; text-align: center; }
          td { padding: 8px; border-bottom: 1px solid #ddd; text-align: center; }
          td:first-child { text-align: left; font-weight: bold; }
        </style>
      </head>
      <body>
        <div class="container">
          <h1>📊 Clima: ${nombreMesResumen} ${anioResumen}</h1>
  `;

  // --- TELEGRAM (Encabezado con emojis) ---
  let cuerpoTelegram = `📅 *RESUMEN MENSUAL: ${nombreMesResumen.toUpperCase()} ${anioResumen}*\n`;
  
  let datosEncontradosTotal = false;

  nombresHojas.forEach(nombreHoja => {
    const sheet = SpreadsheetApp.getActiveSpreadsheet().getSheetByName(nombreHoja);
    if (!sheet) return;

    const datos = sheet.getDataRange().getValues();
    if (datos.length < 2) return;

    const encabezados = datos[0]; 

    // Filtro de fecha
    const datosMesResumen = datos.filter((fila, index) => {
      if (index === 0) return false;
      const fechaDato = new Date(fila[0]);
      return (fechaDato.getMonth() === mesResumen && fechaDato.getFullYear() === anioResumen);
    });

    if (datosMesResumen.length === 0) return;
    
    datosEncontradosTotal = true;

    // --- SECCIÓN TELEGRAM ---
    // Agregamos línea separadora para limpieza visual
    cuerpoTelegram += `\n📍 *${nombreHoja.toUpperCase()}*\n━━━━━━━━━━━━━━━━\n`;
    
    // --- SECCIÓN HTML ---
    cuerpoHTML += `<h2>${nombreHoja}</h2><table><thead><tr><th>Variable</th><th>Promedio</th><th>Máx</th><th>Mín</th></tr></thead><tbody>`;

    const limiteColumnas = Math.min(numSensores, encabezados.length - 1);

    for (let i = 1; i <= limiteColumnas; i++) {
      const nombreColumna = encabezados[i];
      if (columnasIgnorar.includes(nombreColumna)) continue;

      const valoresSensor = datosMesResumen.map(fila => fila[i]).filter(valor => typeof valor === 'number' && !isNaN(valor));
      if (valoresSensor.length === 0) continue;

      const suma = valoresSensor.reduce((acc, valor) => acc + valor, 0);
      const promedio = (suma / valoresSensor.length).toFixed(1);
      const maximo = Math.max(...valoresSensor).toFixed(1);
      const minimo = Math.min(...valoresSensor).toFixed(1);

      // --- DETECCIÓN DE UNIDADES E ICONOS ---
      let icono = "🔹";
      let unidad = "";
      const colLower = nombreColumna.toLowerCase();

      if (colLower.includes("temp")) { icono = "🌡️"; unidad = "°C"; }
      else if (colLower.includes("hum")) { icono = "💧"; unidad = "%"; }
      else if (colLower.includes("pres") || colLower.includes("atm")) { icono = "⏲️"; unidad = " hPa"; }
      else if (colLower.includes("iaq")) { icono = "🍃"; unidad = " pts"; }
      else if (colLower.includes("co2")) { icono = "☁️"; unidad = " ppm"; }

      // --- FORMATO TELEGRAM (Estilizado) ---
      // Usamos negrita (*) para el nombre y el promedio
      // Usamos una segunda línea con sangría para min/max
      cuerpoTelegram += `${icono} *${nombreColumna}:* *${promedio}${unidad}*\n      └ 📉 ${minimo}  •  📈 ${maximo}\n`;

      // --- FORMATO HTML ---
      cuerpoHTML += `
        <tr>
          <td>${icono} ${nombreColumna}</td>
          <td style="font-weight:bold; color:#2E7D32;">${promedio}${unidad}</td>
          <td style="color:#C62828;">${maximo}</td>
          <td style="color:#1565C0;">${minimo}</td>
        </tr>`;
    }

    cuerpoHTML += `</tbody></table>`;
  });

  cuerpoHTML += `<div style="margin-top:20px; text-align:center; color:#888; font-size:12px;">Generado automáticamente • ${new Date().toLocaleString()}</div></div></body></html>`;

  if (!datosEncontradosTotal) {
    Logger.log("No hay datos.");
    return;
  }

  // ENVIAR CORREO
  try {
    MailApp.sendEmail({
      to: emailDestino,
      subject: `☁️ Resumen Climático - ${nombreMesResumen} ${anioResumen}`,
      htmlBody: cuerpoHTML,
    });
    Logger.log("📧 Correo enviado.");
  } catch (e) {
    Logger.log("❌ Error enviando correo: " + e.message);
  }

  // ENVIAR TELEGRAM
  if (telegramBotToken !== "TU_TOKEN_AQUI") {
    try {
      const url = `https://api.telegram.org/bot${telegramBotToken}/sendMessage`;
      const payload = {
        'chat_id': telegramChatId,
        'text': cuerpoTelegram,
        'parse_mode': 'Markdown' // Importante para que las negritas funcionen
      };
      
      const options = {
        'method': 'post',
        'contentType': 'application/json',
        'payload': JSON.stringify(payload)
      };
      
      UrlFetchApp.fetch(url, options);
      Logger.log("✈️ Telegram enviado.");
    } catch (e) {
      Logger.log("❌ Error enviando Telegram: " + e.message);
    }
  }
}