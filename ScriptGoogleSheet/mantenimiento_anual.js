function copia() {
  try {
    var fechaActual = new Date();

    // Verificamos si es 1 de Enero
    if (fechaActual.getDate() === 1 && fechaActual.getMonth() === 0) {
      
      var idLibro = 'ID';
      var libroActual = SpreadsheetApp.openById(idLibro);
      var añoAnterior = fechaActual.getFullYear() - 1; // Calculamos el año que acabamos de cerrar
      
      // Hojas que NO se deben tocar (Dashboards, Configuración, etc.)
      var hojasOmitir = ["G.U.P.A", "G.U.P.B", "Comparativas PA y PB", "GraficoPA", "Grafico 2", "Config"]; 

      // 1. Crear la copia de seguridad
      var nombreCopia = "Registro de Temperatura_" + añoAnterior;
      var copiaLibro = libroActual.copy(nombreCopia);
      
      // Verificación de seguridad: ¿Existe la copia?
      if (!copiaLibro) {
        throw new Error("No se pudo crear la copia de seguridad. Se aborta la limpieza.");
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
          // Si la hoja NO está en la lista de omitir
          if (hojasOmitir.indexOf(nombreHoja) === -1) {
            
            var ultimaFila = hoja.getLastRow();
            
            // Si tiene datos más allá de la fila 1 (encabezados)
            if (ultimaFila > 1) {
              // Borrar desde la fila 2 hasta el final
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

      // 3. Enviar Informe
      var destinatario = "jespezz@hotmail.com";
      var asunto = "✅ Mantenimiento Anual Completado: " + añoAnterior;
      var cuerpo = "El proceso de archivo anual ha finalizado.\n\n" +
                   "📂 Backup creado: " + nombreCopia + "\n" +
                   "🔗 URL Backup: " + copiaLibro.getUrl() + "\n\n" +
                   "🧹 Hojas limpiadas: " + hojasLimpiadas.join(", ") + "\n" +
                   "🛡️ Hojas omitidas: " + hojasOmitidasLog.join(", ") + "\n" +
                   "⚠️ Errores: " + (errores.length > 0 ? errores.join("\n") : "Ninguno");

      MailApp.sendEmail(destinatario, asunto, cuerpo);
      Logger.log("Proceso terminado exitosamente.");

    } else {
      Logger.log('Hoy no es 1 de enero. No se requiere mantenimiento.');
    }
  } catch (e) {
    MailApp.sendEmail("jespezz@hotmail.com", "❌ ERROR CRÍTICO en Script Anual", e.message);
    Logger.log('Error fatal: ' + e.message);
  }
}