from flask import Flask, render_template, request, redirect, url_for, session, jsonify
import asyncio
import websockets
import json
import threading
import os
import math
import time

app = Flask(__name__)
app.secret_key = 'supersecretkey'  # Cambia esto por una clave secreta segura

# Datos para almacenar los valores de BPM y otros parámetros
data = {
    "timestamps": [],
    "bpm": [],
    "p": [],
    "qrs": [],
    "t": [],
    "pr": [],
    "qt": [],
    "qtc": [],
    "temp": []
}

# Archivo JSON para almacenar los datos del paciente
PATIENT_DATA_FILE = 'patient_data.json'

# Leer los datos del paciente desde el archivo JSON
def load_patient_data():
    if os.path.exists(PATIENT_DATA_FILE):
        with open(PATIENT_DATA_FILE, 'r') as file:
            return json.load(file)
    return {}

# Guardar los datos del paciente en el archivo JSON
def save_patient_data(data):
    with open(PATIENT_DATA_FILE, 'w') as file:
        json.dump(data, file)

# Cargar los datos del paciente al iniciar la aplicación
patient_data = load_patient_data()

# Conectar al servidor WebSocket
async def connect_to_server():
    #uri= "ws://10.42.1.138:8080"
    uri= "ws://192.168.11.151:8080"
    print(f"Intentando conectar a {uri}")
    try:
        async with websockets.connect(uri) as websocket:
            print("Conectado al servidor WebSocket")
            while True:
                try:
                    message = await websocket.recv()
                    print(f"Mensaje recibido: {message}")
                    process_message(message)
                except websockets.exceptions.ConnectionClosed:
                    print("Conexión cerrada, intentando reconectar...")
                    break
    except Exception as e:
        print(f"Error de conexión: {e}")
        await asyncio.sleep(1)
        await connect_to_server()

# Procesar los mensajes JSON recibidos
def process_message(message):
    global data
    try:
        parsed = json.loads(message)
        timestamp = time.time()
        data["timestamps"].append(timestamp)
        for key in data.keys():
            if key in parsed and parsed[key] is not None and not math.isnan(parsed[key]):
                data[key].append(parsed[key])
        
        # Limitar la cantidad de datos almacenados
        max_data_points = 60
        for key in data:
            data[key] = data[key][-max_data_points:]
    except json.JSONDecodeError as e:
        print(f"Error decodificando JSON: {e}")

def get_normal_bpm_range(age, gender):
    if gender == "male":
        if 20 <= age <= 29:
            return (70, 84)
        elif 30 <= age <= 39:
            return (72, 84)
        elif 40 <= age <= 49:
            return (74, 88)
        elif age >= 50:
            return (76, 88)
    else:  # Female
        if 20 <= age <= 29:
            return (78, 94)
        elif 30 <= age <= 39:
            return (80, 96)
        elif 40 <= age <= 49:
            return (80, 98)
        elif age >= 50:
            return (84, 102)
    return (0, 200)  # Un rango muy amplio si no se encuentra en ninguna categoría

@app.route('/')
def index():
    if 'username' in session:
        age = patient_data.get('age')
        gender = patient_data.get('gender')
        
        # Obtener el rango normal de BPM
        min_bpm, max_bpm = get_normal_bpm_range(age, gender)
        
        bpm_range = {
            'min': min_bpm,
            'max': max_bpm
        }
        
        return render_template('index.html', patient=patient_data, data=data, 
                               bpm_range=json.dumps(bpm_range))
    else:
        return redirect(url_for('login'))

@app.route('/register', methods=['GET', 'POST'])
def register():
    global patient_data
    if request.method == 'POST':
        name = request.form['name']
        age = int(request.form['age'])
        gender = request.form['gender']
        
        patient_data["name"] = name
        patient_data["age"] = age
        patient_data["gender"] = gender
        
        # Establecer rango de BPM
        min_bpm, max_bpm = get_normal_bpm_range(age, gender)
        patient_data["bpm_range"] = f"{min_bpm}-{max_bpm}"

        # Guardar los datos del paciente en el archivo JSON
        save_patient_data(patient_data)

        session['username'] = name  # Iniciar sesión automáticamente después del registro
        return redirect(url_for('index'))
    return render_template('register.html')

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        try:
            username = request.form['username']
            # Aquí puedes añadir lógica para verificar el usuario (en este caso, simplemente almacenamos el nombre)
            session['username'] = username
            return redirect(url_for('index'))
        except KeyError:
            return "Bad Request: Missing username", 400
    return render_template('login.html')

@app.route('/logout')
def logout():
    session.pop('username', None)
    return redirect(url_for('login'))

@app.route('/history')
def history():
    # Limitar la cantidad de datos almacenados a los últimos 10 registros
    max_records = 10
    recent_data = {
        key: values[-max_records:]
        for key, values in data.items()
    }

    return jsonify(recent_data)


    return jsonify(recent_data)

def start_flask():
    app.run(debug=True, use_reloader=False)

def start_websocket():
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(connect_to_server())

if __name__ == "__main__":
    threading.Thread(target=start_websocket).start()
    start_flask()