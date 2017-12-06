from flask import Flask, render_template, make_response
from flask_bootstrap import Bootstrap
from flask_nav import Nav
from flask_nav.elements import *
import paho.mqtt.client as mqtt
import time
import os
from flask.json import jsonify

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib import style

# Patient Data
patient_data = {
    "nikhil_shenoy": {
        "name": "Nikhil Shenoy",
        "description": "Pacemaker Patient",
        "age": 24,
        "height": "5\' 8\"",
        "occupation": "student"
    },
    "neeraj_gandhi": {
        "name": "Neeraj Gandhi",
        "description": "Pacemaker Patient",
        "age": 23,
        "height": "5\' 8\"",
        "occupation": "student"
    },
    "abhijeet_singh": {
        "name": "Abhijeet Singh",
        "description": "Pacemaker Patient",
        "age": 23,
        "height": "5\' 8\"",
        "occupation": "student"
    },
    "ramneet_kaur": {
        "name": "Ramneet Kaur",
        "description": "Pacemaker Patient",
        "age": 23,
        "height": "5\' 8\"",
        "occupation": "student"
    },
    "default": {
        "name": "John Doe",
        "description": "Pacemaker Patient",
        "age": 50,
        "height": "6\' 0\"",
        "occupation": "doctor"
    }

}


# Global variables for graphing
LRL = 1500.0  # milliseconds
URL = 600.0  # milliseconds

x = [float(-1 * i) for i in range(5,0,-1)]

# Conversion to beats per minute
LRL_data = [1000.0/LRL for i in range(5)]
URL_data = [1000.0/URL for i in range(5)]
fig = plt.figure()

heart_data = [0.0 for i in range(5)]


# Global variable for alerts
slow_alarms = ["" for i in range(5)]
fast_alarms = ["" for i in range(5)]

broker_address = "35.188.242.1"
port = 1883
timeout = 60
username = "mbed"
password = "homework"
uuid = "1234"
# topic = "cis541/hw-mqt/26013f37-08009003ae2a90e552b1fc8ef5001e87/echo"
data_topic = "group8/data"
fast_alert_topic = "group8/slowHeartBeatAlarm"
slow_alert_topic = "group8/fastHeartBeatAlarm"
qos = 0

def add_data(data, payload):
    data.pop(0)
    data.append(payload)

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    # f.write("Connected with result code "+str(rc))
    print("Connected with result code " + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(data_topic)
    client.subscribe(fast_alert_topic)
    client.subscribe(slow_alert_topic)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    if "slow" in msg.payload:
        add_data(slow_alarms, msg.payload)
    elif "fast" in msg.payload:
        add_data(fast_alarms, msg.payload)
    else:
        pieces = msg.payload.split(':')
        timestamp = float(pieces[0])
        heart_val = float(pieces[1])
        add_data(x, timestamp)
        add_data(heart_data, heart_val)

    print(msg.topic+" "+str(msg.payload) + '\n')

def on_disconnect(client, userdata, rc):
    print("Closing data file...")

client = mqtt.Client(client_id=uuid)
client.on_connect = on_connect
client.on_message = on_message
client.on_disconnect = on_disconnect
client.username_pw_set(username, password)

client.connect(broker_address, port, 60)
client.loop_start()

nav = Nav()

def delete_old_graphs():
    files = [f for f in os.listdir('./static/graphs')]
    for f in files:
        os.remove(f)

def get_patients():
    patients = []
    for key in patient_data:
        if key != "default":
            patients.append({
                'id': key,
                'name': patient_data[key]["name"]})

    return patients


# @todo Apply theme to Nav Bar
@nav.navigation()
def mynavbar():
    return Navbar(
        'Pacemaker Project',
        View('Dashboard', 'index'),
        View('About', 'about')
    )

app = Flask(__name__)
Bootstrap(app)
nav.init_app(app)

@app.route('/')
def index():
    return render_template("index.html", patients=get_patients(), ns_data=patient_data["nikhil_shenoy"])

@app.route('/patient/<string:patient_name>')
def patient(patient_name):
    '''
        Use a variable parameter here to decide which data to display in the main view
    '''


    if patient_name != "" and patient_name not in patient_data:
        patient_name = "default"


    return render_template('patient.html',
                           patients=get_patients(),
                           patient_data=patient_data[patient_name])

@app.route('/about')
def about():
    return render_template('about.html', message='About page under construction')

@app.route('/alerts')
def alarms_ep():
    # Combine alarms and take out empty strings
    all_alarms = filter(lambda x: x != "", slow_alarms + fast_alarms)

    notifications = []

    alarm_type = ""
    for i in range(len(all_alarms)):
        if "slow" in all_alarms[i]:
            alarm_type = "slow"
        else:
            alarm_type = "fast"

        timestamp = float(all_alarms[i].split(':')[0])

        notifications.append({
            'timestamp': timestamp,
            'type': alarm_type
        })

    def compare(a, b):
        if a["timestamp"] > b["timestamp"]:
            return 1
        elif a["timestamp"] == b["timestamp"]:
            return 0
        else:
            return -1

    notifications.sort(compare)

    notifications.reverse()


    return jsonify(notifications)

@app.route('/graph')
def graph_ep():

    fig = plt.figure()

    # Matplotlib setup
    style.use('fivethirtyeight')  # Make the graphs look better

    ax1 = fig.add_subplot(1, 1, 1)

    ax1.clear()
    # plt.ylim(0,100)
    # y_axis = [float(i)/10.0 for i in range(0,16,1)]


    ax1.plot(x, heart_data, marker='o', label="Heart Data")
    ax1.plot(x, LRL_data, marker='o', label="Lower Rate Limit")
    ax1.plot(x, URL_data, marker='o', label="Upper Rate Limit")
    ax1.set(xlabel="Time",
            ylabel="Beats per Minute",
            title="Moving Average of Heart Rate")

    ax1.legend(loc="upper center")

    ax1.grid(True)

    # Change y axis
    ax1.set_ylim(0,2.5)
    x1, x2, y1, y2 = plt.axis()
    plt.axis((x1,x2,0.0, 2.5))
    y_axis = [float(i)/10.0 for i in range(0,25,2)]
    plt.yticks(y_axis)


    time_string = str(int(time.time()))
    file_name = 'static/graphs/real/graph' + time_string + '.png'

    plt.savefig(file_name)

    return '<img src="' + file_name + '">'



if __name__ == "__main__":
    app.run(debug=True)