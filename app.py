from flask import Flask, request, jsonify
import firebase_admin
from firebase_admin import credentials, db
from datetime import datetime

app = Flask(__name__)

# Inicializar Firebase con tu archivo de clave
cred = credentials.Certificate("serviceAccountKey.json")
firebase_admin.initialize_app(cred, {
    'databaseURL': 'https://agroguardian-d5d4d-default-rtdb.firebaseio.com/'
})

@app.route('/data', methods=['POST'])
def recibir_datos():
    data = request.get_json()

    if not data:
        return jsonify({'status': 'Error', 'message': 'No se recibió JSON'}), 400

    # Obtener timestamp en formato legible
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    # Guardar en Firebase
    ref = db.reference('/lecturas')
    ref.push({
        'timestamp': timestamp,
        'datos': data
    })

    print(f'Datos guardados en Firebase: {data}')
    return jsonify({'status': 'OK', 'message': 'Datos recibidos y guardados'})

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
