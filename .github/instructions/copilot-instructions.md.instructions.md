<<<<<<< HEAD
# Reglas de Release y Control de Versiones

1. **Sincronización Obligatoria de Versión en Código:**
   - NUNCA generes un Release en GitHub ni compiles un binario de producción (`firmware.bin`) sin antes buscar y actualizar la variable de versión en el código fuente (por ejemplo: `const char* version`, `APP_VERSION`, `#define VERSION`).
   - El valor de la variable en el código DEBE coincidir carácter por carácter con el Tag del Release (ej. `v4.3.2-MQTT`).
   - Esto es crítico para prevenir bucles infinitos en el sistema de actualización OTA del microcontrolador.

2. **Validación Previa a Compilaciones de Release:**
   - Antes de ejecutar `pio run` o `gh release create`, realiza una búsqueda global en el espacio de trabajo de la versión anterior y reemplázala por la nueva versión objetivo en todos los archivos de configuración y cabecera.

   3. **Siempre usa el idioma español:**

   # Reglas de Versionado e Incremento Automático

1. **Incremento Automático de Versión:**
   - Antes de realizar una compilación para producción (`pio run`), crear un Pull Request (PR) o publicar un Release en GitHub, debes inspeccionar la versión actual registrada en el código y en los Tags del repositorio.
   - Incrementa automáticamente el número consecutivo de versión (patch/subversión).
     - *Ejemplo:* Si la versión actual es `v4.3.3-MQTT`, cámbiala a `v4.3.4-MQTT`.
     - Conserva siempre el formato exacto, prefijos y sufijos existentes (como `-MQTT`).

2. **Excepción por Versión Personalizada:**
   - Si el usuario especifica explícitamente una versión distinta en su solicitud (por ejemplo: *"haz el release v5.0.0"* o *"usa la v4.4.0-MQTT"*), omite el incremento automático y utiliza exactamente la versión indicada por el usuario.

3. **Sincronización en Código:**
   - Recuerda actualizar la variable global de versión en el código fuente (ej. `const char* version`, `APP_VERSION`, `#define VERSION`) para que coincida exactamente con la nueva versión calculada antes de compilar y generar el binario `.bin`.

   # Reglas de Documentación Post-Release

1. **Actualización Obligatoria de `CHANGELOG.md`:**
   - Después de cada release o PR importante, agrega una nueva sección al inicio de `CHANGELOG.md` con la versión generada y la fecha actual (ejemplo: `## [v4.3.4-MQTT] - YYYY-MM-DD`).
   - Clasifica y detalla brevemente los cambios realizados usando el estándar:
     - **Agregado (Added):** Nuevas funcionalidades o endpoints.
     - **Cambiado (Changed):** Modificaciones en lógica existente o flujo UI.
     - **Corregido (Fixed):** Corrección de bugs o parches.

2. **Actualización de `README.md`:**
   - Si la nueva versión incluye características visibles (nuevos campos de configuración, comportamientos de autenticación, endpoints o estados de interfaz), actualiza las secciones correspondientes de `README.md`.
   - Asegúrate de que la portada del repositorio refleje siempre el estado de la versión más reciente.
=======
# Reglas de Compilación, Release y Versionado

### 1. Política de Compilación (`pio run`)
- **NO ejecutes `pio run`** tras realizar cambios menores, refactorizaciones o edición de código durante el desarrollo cotidiano.
- Únicamente ejecuta `pio run` cuando el usuario solicite explícitamente **crear un Release**, **generar el binario de producción** o probar la compilación.

---

### 2. Flujo Integrado de Release (Paso a Paso)
Cuando el usuario pida realizar un release (ej. *"haz el release"* o *"publica la nueva versión"*), ejecuta **estrictamente** esta secuencia:

1. **Obtener la versión actual:**
   - Consulta el último tag/release del repositorio (usando `gh release list` o `git tag`) y la variable de versión actual en el código fuente.

2. **Determinar la nueva versión:**
   - **Modo Automático:** Si el usuario NO especificó una versión, incrementa el consecutivo del número parche/subversión manteniendo el formato exacto, prefijos y sufijos (ejemplo: de `v4.3.4-MQTT` a `v4.3.5-MQTT`).
   - **Modo Personalizado:** Si el usuario indicó una versión explícita (ej. *"haz la v5.0.0"*), usa exactamente esa versión.

3. **Sincronizar el Código Fuente:**
   - Reemplaza el valor de la variable de versión en el código (ej. `const char* version = "v4.3.5-MQTT";` o `#define VERSION ...`) por el valor exacto de la nueva versión.
   - *Nota:* Este paso es obligatorio antes de compilar para evitar bucles infinitos en el sistema OTA del dispositivo.

4. **Compilar el Binario:**
   - Ejecuta `pio run` para generar el firmware actualizado con la versión correcta embebida.

5. **Documentar y Publicar:**
   - Agrega la entrada correspondiente en `CHANGELOG.md` con la fecha actual y detalla las secciones (**Agregado**, **Cambiado**, **Corregido**).
   - Actualiza `README.md` si la entrega incluye características o configuraciones nuevas.
   - Publica el Release en GitHub (`gh release create`) adjuntando el binario recién compilado.

---

### 3. Idioma
- Toda la comunicación, documentación, mensajes de commit y notas de release deben redactarse en **español**.
>>>>>>> 6a7ce66a2d1b77dd88ca8c4c7c925dd72749d9a0
