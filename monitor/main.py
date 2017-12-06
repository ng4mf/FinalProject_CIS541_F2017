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
LRL = 1500.0 # milliseconds
URL = 600.0 # milliseconds

x = [1, 2, 3, 4, 5]

# Conversion to beats per minute
LRL_data = [1000.0/LRL for i in range(5)]
URL_data = [1000.0/URL for i in range(5)]

y = [0, 0, 0, 0, 0]


# Global variable for alerts
notifications = []


broker_address = "35.188.242.1"
port = 1883
timeout = 60
username = "mbed"
password = "homework"
uuid = "1234"
# topic = "cis541/hw-mqt/26013f37-08009003ae2a90e552b1fc8ef5001e87/echo"
topic = "group8/data"
qos = 0



# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, rc):
    # f.write("Connected with result code "+str(rc))
    print("Connected with result code " + str(rc))

    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe(topic)

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    y.pop(0)
    y.append(msg.payload)
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
def alerts_ep():
    for i in range(10):
        notifications.append({
            'id': i,
            'one': 'one',
            'two': 'two',
            'three': 'three',
            'four': 'four'
        })

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


    ax1.plot(x, y, marker='o', label="Heart Data")
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

    # ani = animation.FuncAnimation(fig, animate, interval=1000)

    time_string = str(int(time.time()))
    file_name = 'static/graphs/real/graph' + time_string + '.png'

    plt.savefig(file_name)

    return '<img src="' + file_name + '">'



if __name__ == "__main__":
    app.run(debug=True)