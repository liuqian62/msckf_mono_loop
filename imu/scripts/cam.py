#!/usr/bin/env python
#coding:UTF-8
import rospy
from std_msgs.msg import Header
from sensor_msgs.msg import Image
from cv_bridge import CvBridge, CvBridgeError
import cv2
import numpy as np

IMAGE_WIDTH=640
IMAGE_HEIGHT=480

def PublishImg(cap):
    #stamp = rospy.get_rostime()
    ret, frame = cap.read()
    frame = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    img_msg = bridge.cv2_to_imgmsg(frame, encoding="mono8")
    stamp = rospy.get_rostime()
    img_msg.header.stamp = stamp
    imagePub.publish(img_msg)
    cv2.imshow('123',frame)
    key=cv2.waitKey(10)
    rate.sleep()

rospy.init_node("imu")
imagePub=rospy.Publisher('/cam0/image_raw', Image, queue_size=2)
rate=rospy.Rate(50)
cap = cv2.VideoCapture(2)
bridge = CvBridge()

try:
    while not rospy.is_shutdown():
        PublishImg(cap)

except rospy.ROSInterruptException:
   pass        
