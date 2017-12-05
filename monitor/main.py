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
x = [1, 2, 3, 4, 5]
y = [0, 0, 0, 0, 0]
fig = plt.figure()

# Global variable for alerts
notifications = []


broker_address = "35.188.242.1"
port = 1883
timeout = 60
username = "mbed"
password = "homework"
uuid = "1234"
topic = "cis541/hw-mqt/26013f37-08009003ae2a90e552b1fc8ef5001e87/echo"
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

# client.connect(broker_address, port, 60)
# client.loop_start()

nav = Nav()

def delete_old_graphs():
    files = [f for f in os.listdir('./static/graphs')]
    for f in files:
        os.remove(f)

def get_patients():
    patients = []
    for key in patient_data:
        if key != "default":
            patients.append(patient_data[key]["name"])

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
    '''
        Use a variable parameter here to decide which data to display in the main view
    '''

    return render_template('index.html', patients=get_patients())

@app.route('/about')
def about():
    return render_template('about.html',
                           message='About page under construction',
                           patients=get_patients())

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

    # Matplotlib setup
    style.use('fivethirtyeight')  # Make the graphs look better

    ax1 = fig.add_subplot(1, 1, 1)

    ax1.clear()
    ax1.plot(x, y)

    # ani = animation.FuncAnimation(fig, animate, interval=1000)

    time_string = str(int(time.time()))
    file_name = 'static/graphs/graph' + time_string + '.png'

    plt.savefig(file_name)

    return '<img src="' + file_name + '">'



if __name__ == "__main__":
    tick = 0
    app.run(debug=True)