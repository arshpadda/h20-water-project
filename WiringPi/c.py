# encoding=utf8
import sys
import zmq
import json
from google.cloud import pubsub

CONTROLLER_ID = 'C001'

# Insert pub/sub data

#

context = zmq.Context()
socket = context.socket(zmq.SUB)
print("Collecting Information")
socket.connect("tcp://localhost:5556")

filter = ""
filter = filter.decode('ascii')

socket.setsockopt_string(zmq.SUBSCRIBE, filter)
while True:
	string1 = socket.recv_string()
	string2 = socket.recv_string()
	date1, time1, data_ph1, data_ph2, data_ph3 = string1.split(",")
	date2, time2, data_c1, data_c2, data_c3 = string2.split(",")
	#print("Date1 : %s , Time1 : %s , data1 : %s , data2 : %s , data3 : %s" %(date1, time1, data_ph1, data_ph2, data_ph3))
	#print("Date2 : %s , Time2 : %s , data1 : %s , data2 : %s , data3 : %s" %(date2, time2, data_c1, data_c2, data_c3))
	json_data1 = json.dumps({'date':date1,'time':time1,'data1':data_ph1,'data2':data_ph2,'data3':data_ph3})
	json_data2 = json.dumps({'date':date2,'time':time2,'data1':data_c1,'data2':data_c2,'data3':data_c3})
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
	topic.publish(json_data1)
	topic.publish(json_data2)
	#topic.publish(json_data)
