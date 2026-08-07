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