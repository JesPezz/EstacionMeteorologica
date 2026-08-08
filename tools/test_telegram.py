#!/usr/bin/env python3
import os
import sys
import argparse
import urllib.request
import urllib.parse
import json

def send_telegram_test(token, chat_id, message="✅ Test de notificación desde Estación Meteorológica IoT"):
    if not token or not chat_id:
        print("❌ Error: TELEGRAM_BOT_TOKEN y TELEGRAM_CHAT_ID son obligatorios.")
        print("Uso: python3 tools/test_telegram.py --token <BOT_TOKEN> --chat_id <CHAT_ID>")
        print("O exporta las variables de entorno TELEGRAM_BOT_TOKEN y TELEGRAM_CHAT_ID.")
        return False

    url = f"https://api.telegram.org/bot{token}/sendMessage"
    payload = {
        "chat_id": chat_id,
        "text": message,
        "parse_mode": "HTML"
    }
    
    data = urllib.parse.urlencode(payload).encode("utf-8")
    req = urllib.request.Request(url, data=data, method="POST")
    
    try:
        with urllib.request.urlopen(req) as response:
            res_body = response.read().decode("utf-8")
            res_json = json.loads(res_body)
            if res_json.get("ok"):
                print("✅ ¡Notificación de prueba enviada con éxito por Telegram!")
                return True
            else:
                print(f"❌ Error de Telegram API: {res_json}")
                return False
    except Exception as e:
        print(f"❌ Error de conexión al enviar Telegram: {e}")
        return False

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Test de notificación por Telegram para Estación Meteorológica")
    parser.add_argument("--token", default=os.getenv("TELEGRAM_BOT_TOKEN"), help="Telegram Bot Token")
    parser.add_argument("--chat_id", default=os.getenv("TELEGRAM_CHAT_ID"), help="Telegram Chat ID")
    parser.add_argument("--message", default="✅ Test de notificación desde Estación Meteorológica IoT", help="Mensaje a enviar")
    
    args = parser.parse_args()
    success = send_telegram_test(args.token, args.chat_id, args.message)
    sys.exit(0 if success else 1)
