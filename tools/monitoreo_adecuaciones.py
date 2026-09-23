#!/usr/bin/env python3
"""
Monitoreo de temperaturas - Estación Meteorológica
Compara interior (Planta Alta / Planta Baja) vs Exterior para evaluar
el efecto de las adecuaciones interiores para reducir el calor.

- Lectura en vivo cada N minutos desde las 3 IPs de los ESP32
- Registro en CSV local (Registros/monitoreo_adecuaciones.csv)
- Informe diario con delta interior-exterior vs línea base histórica
"""

import csv
import json
import os
import sys
import time
import urllib.request
from datetime import datetime, timedelta
from statistics import mean

# --------------------------------------------------------------------------
# Configuración
# --------------------------------------------------------------------------
SENSORES = {
    "Exterior":    {"ip": "192.168.1.84"},
    "PlantaAlta":  {"ip": "192.168.1.150"},
    "PlantaBaja":  {"ip": "192.168.1.72"},
    "Recamara":    {"ip": "192.168.1.80"},
}

SHEET_ID = "12ttM1jJPRWgpgqWCg6ApXyouKTo7fzviuX42mmqxS1Q"

# --- Cuenta de servicio (acceso programático permanente) ---
GCLOUD_CRED = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "credenciales", "gcloud-service-account.json"
)
GAPI_SCOPES = ["https://www.googleapis.com/auth/spreadsheets.readonly"]

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REG_DIR = os.path.join(BASE_DIR, "Registros")
CSV_FILE = os.path.join(REG_DIR, "monitoreo_adecuaciones.csv")
INFO_DIR = os.path.join(REG_DIR, "informes")

INTERVALO_MIN = 30          # lectura cada 30 min
HORAS_DIURNAS = (10, 18)    # franja diurna pico de calor
HORAS_NOCTURNAS = (21, 6)   # franja nocturna (retención de calor)
DIAS_MONITOREO = 7          # duración planificada

# --------------------------------------------------------------------------
# Lectura en vivo de los ESP32
# --------------------------------------------------------------------------
def leer_sensor(name, ip, timeout=15):
    url = f"http://{ip}/sensor_data"
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=timeout) as r:
            d = json.loads(r.read().decode())
        return {
            "temp": d.get("temp"),
            "humidity": d.get("humidity"),
            "pressure": d.get("pressure"),
            "iaq": d.get("air_quality"),
            "battery_voltage": d.get("battery_voltage"),
            "battery_status": d.get("battery_status"),
            "version": d.get("version"),
        }
    except Exception as e:
        return {"error": str(e)}

def leer_todos():
    res = {}
    for name, cfg in SENSORES.items():
        res[name] = leer_sensor(name, cfg["ip"])
    return res

# --------------------------------------------------------------------------
# CSV
# --------------------------------------------------------------------------
FIELDNAMES = [
    "timestamp", "sensor", "temperature", "humidity", "pressure",
    "iaq", "battery_voltage", "battery_status", "version", "error"
]

def escribir_csv(registro):
    os.makedirs(REG_DIR, exist_ok=True)
    existe = os.path.exists(CSV_FILE)
    with open(CSV_FILE, "a", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDNAMES)
        if not existe:
            w.writeheader()
        w.writerow(registro)

# --------------------------------------------------------------------------
# Histórico desde Google Sheets (Cuenta de Servicio vía API)
# --------------------------------------------------------------------------
def _get_sheets_service():
    from google.oauth2 import service_account
    from googleapiclient.discovery import build
    creds = service_account.Credentials.from_service_account_file(
        GCLOUD_CRED, scopes=GAPI_SCOPES
    )
    return build("sheets", "v4", credentials=creds)

def leer_sheet(sheet, timeout=60):
    """Devuelve las filas de una pestaña del sheet como lista de listas."""
    svc = _get_sheets_service()
    result = svc.spreadsheets().values().get(
        spreadsheetId=SHEET_ID,
        range=f"'{sheet}'",
    ).execute()
    return result.get("values", [])

def parse_fecha(v):
    # Caso 1: número serial de Google Sheets (días desde 1899-12-30)
    if isinstance(v, (int, float)):
        # 25569 = 1970-01-01 (epoch Unix); añadimos el desfase + usamos UTC local
        return datetime(1899, 12, 30) + timedelta(days=float(v))
    v = str(v)
    # Caso 2: 'Date(2026,0,1,0,0,23)' (formato GViz)
    if v.startswith("Date("):
        v = v.replace("Date(", "").replace(")", "")
        p = [int(x) for x in v.split(",")]
        return datetime(p[0], p[1] + 1, p[2], p[3], p[4], p[5])
    v = v.replace("T", " ")
    # Caso 3: cadenas de fecha en varios formatos
    for fmt in ("%d/%m/%Y %H:%M:%S", "%d/%m/%Y %H:%M", "%m/%d/%Y %H:%M:%S",
                "%m/%d/%Y %H:%M", "%Y-%m-%d %H:%M:%S", "%Y-%m-%d %H:%M",
                "%Y-%m-%dT%H:%M:%S"):
        try:
            return datetime.strptime(v, fmt)
        except ValueError:
            continue
    raise ValueError(f"Fecha no reconocida: {v!r}")

def cargar_historico():
    """Devuelve {sensor: [(datetime, temp), ...]} desde el sheet vía API."""
    data = {}
    for sheet in ["PlantaAlta", "PlantaBaja", "Exterior", "Recamara"]:
        rows = leer_sheet(sheet)
        if not rows:
            data[sheet] = []
            continue
        # Primera fila = cabecera
        header = rows[0]
        try:
            ti = header.index("Fecha y hora")
            tt = header.index("Temperatura")
        except ValueError:
            # Fallback por posición si cambia la cabecera
            ti, tt = 0, 1
        temps = []
        for r in rows[1:]:
            if len(r) <= max(ti, tt):
                continue
            dt_s, temp_s = r[ti], r[tt]
            if not dt_s or temp_s is None or temp_s == "":
                continue
            try:
                temp = float(str(temp_s).replace(",", "").replace("°", ""))
            except ValueError:
                continue
            try:
                dt = parse_fecha(str(dt_s))
            except ValueError:
                continue
            # Descartar filas corruptas (fecha epoch 1969/1970 sin sincronizar).
            # El histórico real de la estación empieza en 2023.
            if dt.year < 2023:
                continue
            temps.append((dt, temp))
        temps.sort()
        data[sheet] = temps
    return data

# --------------------------------------------------------------------------
# Análisis delta interior - exterior
# --------------------------------------------------------------------------
def deltas_por_dia(historico, dias=14):
    """Retorna {fecha: {'diurno': [deltas], 'nocturno': [deltas]}}

    El interior se calcula como la media de los sensores interiores
    disponibles en cada (día, hora): PlantaAlta, PlantaBaja y, cuando
    tiene lectura, Recamara.
    """
    interiores = ["PlantaAlta", "PlantaBaja", "Recamara"]
    sens = {}
    for s in historico:
        d = {}
        for dt, t in historico[s]:
            d[(dt.date(), dt.hour)] = t
        sens[s] = d

    # Claves (día, hora) donde Exterior y al menos un interior tengan dato
    con_ext = set(sens["Exterior"])
    candidatas = set(con_ext)
    for i in interiores:
        if i in sens:
            candidatas |= set(sens[i]) & con_ext
    comunes = sorted(c for c in candidatas if c in sens["Exterior"])

    por_dia = {}
    for (day, h) in comunes:
        temps_int = []
        for i in interiores:
            if i in sens and (day, h) in sens[i]:
                t = sens[i][(day, h)]
                if t is not None:
                    temps_int.append(t)
        if not temps_int:
            continue
        ex = sens["Exterior"][(day, h)]
        delta = mean(temps_int) - ex
        por_dia.setdefault(day, {"diurno": [], "nocturno": []})
        if HORAS_DIURNAS[0] <= h <= HORAS_DIURNAS[1]:
            por_dia[day]["diurno"].append(delta)
        elif h >= HORAS_NOCTURNAS[0] or h <= HORAS_NOCTURNAS[1]:
            por_dia[day]["nocturno"].append(delta)
    return por_dia

def linea_base(por_dia, dias=7):
    """Media del delta nocturno de los últimos N días completos."""
    nocturnos = []
    fechas = sorted(por_dia)
    for d in fechas[-dias:]:
        nocturnos += por_dia[d]["nocturno"]
    if not nocturnos:
        return None
    return mean(nocturnos)

# --------------------------------------------------------------------------
# Informe diario
# --------------------------------------------------------------------------
def generar_informe(historico=None):
    os.makedirs(INFO_DIR, exist_ok=True)
    hoy = datetime.now()

    if historico is None:
        try:
            historico = cargar_historico()
        except Exception as e:
            print(f"⚠️ No se pudo cargar histórico: {e}")
            historico = {}

    por_dia = deltas_por_dia(historico)
    base = linea_base(por_dia, dias=7)

    # Datos en vivo
    vivo = leer_todos()

    lineas = []
    lineas.append("=" * 60)
    lineas.append("INFORME DIARIO - ADECUACIONES / CALOR")
    lineas.append(f"Fecha: {hoy.strftime('%A %d/%m/%Y %H:%M')}")
    lineas.append("=" * 60)
    lineas.append("")

    lineas.append("--- LECTURA EN VIVO ---")
    ints = []
    for name in ["Exterior", "PlantaAlta", "PlantaBaja", "Recamara"]:
        d = vivo.get(name, {})
        if "error" in d:
            lineas.append(f"{name:<12} ERROR: {d['error']}")
        else:
            if name != "Exterior":
                ints.append(d.get("temp"))
            lineas.append(
                f"{name:<12} {d.get('temp', float('nan')):6.1f} °C  "
                f"hum {d.get('humidity', float('nan')):5.1f}%  "
                f"IAQ {d.get('iaq', float('nan')):6.1f}"
            )
    if ints and vivo.get("Exterior", {}).get("temp"):
        delta_vivo = mean(ints) - vivo["Exterior"]["temp"]
        lineas.append(f"\nDelta (interior - exterior) AHORA: {delta_vivo:+.1f} °C")
    lineas.append("")

    lineas.append("--- DELTA INTERIOR - EXTERIOR POR DÍA ---")
    lineas.append(f"(interior = media PA/PB, externo = Exterior; '+' interior más cálido)")
    lineas.append(f"{'Fecha':<12}{'delta diurno':>14}{'delta nocturno':>16}")
    for f in sorted(por_dia)[-14:]:
        dd = mean(por_dia[f]["diurno"]) if por_dia[f]["diurno"] else None
        dn = mean(por_dia[f]["nocturno"]) if por_dia[f]["nocturno"] else None
        dd_s = f"{dd:+.1f}°C" if dd is not None else "--"
        dn_s = f"{dn:+.1f}°C" if dn is not None else "--"
        lineas.append(f"{str(f):<12}{dd_s:>14}{dn_s:>16}")
    lineas.append("")

    if base is not None:
        lineas.append("--- LÍNEA BASE (media delta nocturno últimos 7 días) ---")
        lineas.append(f"Referencia histórica: {base:+.2f} °C")
        lineas.append("")
        lineas.append("Interpretación:")
        if por_dia and hoy.date() in por_dia and por_dia[hoy.date()]["nocturno"]:
            dn_hoy = mean(por_dia[hoy.date()]["nocturno"])
            dif = dn_hoy - base
            lineas.append(f"  Delta nocturno hoy: {dn_hoy:+.2f} °C")
            if dif < -0.5:
                lineas.append(f"  ⬇️ {abs(dif):.1f} °C bajo la base → buena señal (la casa enfría mejor).")
            elif dif > 0.5:
                lineas.append(f"  ⬆️ {dif:.1f} °C sobre la base → la casa retiene más calor.")
            else:
                lineas.append("  ≈ dentro del rango normal. Sin cambio significativo aún.")
            lineas.append("  (Necesario observar varios días para concluir)")
    lineas.append("")
    lineas.append("=" * 60)

    informe = "\n".join(lineas)
    fname = os.path.join(INFO_DIR, f"informe_{hoy.strftime('%Y%m%d')}.txt")
    with open(fname, "w") as f:
        f.write(informe)
    print(informe)
    return informe

# --------------------------------------------------------------------------
# Telegram
# --------------------------------------------------------------------------
def enviar_telegram(texto):
    """Envía un mensaje por Telegram usando las variables de entorno."""
    import urllib.parse
    token = os.environ.get("TELEGRAM_BOT_TOKEN")
    chat_id = os.environ.get("TELEGRAM_CHAT_ID")
    if not token or not chat_id:
        print("⚠️ Sin TELEGRAM_BOT_TOKEN/TELEGRAM_CHAT_ID. No se envió por Telegram.")
        return False
    url = f"https://api.telegram.org/bot{token}/sendMessage"
    data = urllib.parse.urlencode({
        "chat_id": chat_id,
        "text": texto,
    }).encode()
    req = urllib.request.Request(url, data=data)
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            r.read()
        print("✅ Informe enviado por Telegram.")
        return True
    except Exception as e:
        print(f"⚠️ Error enviando Telegram: {e}")
        return False

# --------------------------------------------------------------------------
# Bucle principal
# --------------------------------------------------------------------------
def hacer_lectura():
    ts = datetime.now()
    vivos = leer_todos()
    for name in SENSORES:
        d = vivos.get(name, {})
        if "error" in d:
            escribir_csv({
                "timestamp": ts.isoformat(), "sensor": name, "error": d["error"]
            })
        else:
            escribir_csv({
                "timestamp": ts.isoformat(), "sensor": name,
                "temperature": d.get("temp"), "humidity": d.get("humidity"),
                "pressure": d.get("pressure"), "iaq": d.get("iaq"),
                "battery_voltage": d.get("battery_voltage"),
                "battery_status": d.get("battery_status"),
                "version": d.get("version"),
            })
    print(f"[{ts}] Lectura registrada en {CSV_FILE}")
    return vivos

def main():
    modo = "daemon"
    if len(sys.argv) > 1 and sys.argv[1] == "lectura":
        # Lectura única (usado por cron)
        hacer_lectura()
        return
    if len(sys.argv) > 1 and sys.argv[1] == "informe":
        generar_informe()
        return
    if len(sys.argv) > 1 and sys.argv[1] == "informe-tg":
        informe = generar_informe()
        enviar_telegram(informe)
        return
    if len(sys.argv) > 1 and sys.argv[1] == "foreground":
        modo = "foreground"

    print(f"📡 Monitoreo iniciado. Intervalo: {INTERVALO_MIN} min. "
          f"Registro en: {CSV_FILE}")
    os.makedirs(REG_DIR, exist_ok=True)
    # lectura inicial + informe de arranque
    hacer_lectura()
    generar_informe()

    if modo != "foreground":
        print("Modo daemon: usando nohup/setsid. Terminando aquí (cron gestiona las lecturas).")
        return

    while True:
        time.sleep(INTERVALO_MIN * 60)
        hacer_lectura()

if __name__ == "__main__":
    main()
