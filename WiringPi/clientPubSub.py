# encoding=utf8
import sys
import zmq
import json
from google.cloud import pubsub
import google.auth
from google.oauth2 import service_account

# Controller ID to identify the origin of data.
CONTROLLER_ID = 'C001'
 
# Insert pub/sub data
pubsub_client = pubsub.Client('wioceanbridge')
topic = pubsub_client.topic('asu-sensory-readings')
#


# Start the Client to local host 5556 to get the data stream.
context = zmq.Context()
socket = context.socket(zmq.SUB)
print("Collecting Information")
socket.connect("tcp://localhost:5556")

filter = ""
filter = filter.decode('ascii')

# Connect to local host 5556 socket.
socket.setsockopt_string(zmq.SUBSCRIBE, filter)

while True:
	# Recieve the string in the socket.
	string1 = socket.recv_string()
	string2 = socket.recv_string()

	# Extract the data from the string. 
	date1, time1, data_ph1, data_ph2, data_ph3 = string1.split(",")
	date2, time2, data_c1, data_c2, data_c3 = string2.split(",")

	# Display the data recieved. (Optional)
	#print("Date1 : %s , Time1 : %s , data1 : %s , data2 : %s , data3 : %s" %(date1, time1, data_ph1, data_ph2, data_ph3))
	#print("Date2 : %s , Time2 : %s , data1 : %s , data2 : %s , data3 : %s" %(date2, time2, data_c1, data_c2, data_c3))

	# Create the json file to be send to google pub/sub.
	json_data = json.dumps({
				'controller_id':CONTROLLER_ID,
				'date':date1,
				'time':time1,
				'ph_data1':data_ph1,
				'ph_data2':data_ph2,
				'ph_data3':data_ph3,
				'c_data1':data_c1,
				'c_data2':data_c2,
				'c_data3':data_c3
				})
	print json_data
	# Publish the data to google pub/sub.
	topic.publish(json_data)
