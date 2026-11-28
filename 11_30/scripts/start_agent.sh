#!/bin/bash
# micro-ROS Agent 실행 (ESP32와 ROS 2를 이어주는 중계기). 이 창은 켜 둔 채로 사용.
source /opt/ros/jazzy/setup.bash
source ~/microros_ws/install/local_setup.bash
echo "Agent 실행 중... ESP32가 연결되면 'session established' 가 뜹니다."
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888
