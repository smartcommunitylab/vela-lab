import paho.mqtt.client as mqtt

# def on_publish_cb(client, data, result):
#     net.addPrint("[MQTT] on_publish_cb. data: "+str(data)+", result: "+str(result))

# def on_disconnect_cb(client, userdata, result):
#     global mqtt_connected
#     net.addPrint("[MQTT] on_disconnect. result: "+str(result))

#     mqtt_connected=False

# def on_connect_cb(client, userdata, flags, rc):
#     global mqtt_connected
#     net.addPrint("[MQTT] on_connect. result code: "+str(rc))

#     if rc==0:
#         mqtt_connected=True
#     else:
#         mqtt_connected=False

#     #client.subscribe() #in case data needs to be received, subscribe here

# def on_message_cb(client, net, userdata, msg):
#     net.addPrint("[MQTT] on_message_cb.")

# def connect_mqtt():
#     net.addPrint("[MQTT] Connecting to mqtt broker...")

#     client=mqtt.Client()
#     client.on_publish=on_publish_cb
#     client.on_connect=on_connect_cb
#     client.on_disconnect=on_disconnect_cb
#     client.on_message=on_message_cb

#     client.username_pw_set(DEVICE_TOKEN, password=None)
#     client.connect(MQTT_BROKER,MQTT_PORT)
    
#     return client