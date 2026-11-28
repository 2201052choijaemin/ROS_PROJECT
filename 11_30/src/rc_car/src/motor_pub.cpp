// motor_pub: 키보드 입력 -> /rc_car/cmd_vel 발행 (pub1_3 과 같은 방식)
//   f: 전진  b: 후진  l: 좌회전  r: 우회전  s: 정지  q: 종료
//   +: 속도 올리기  -: 속도 내리기  (1~10단계, 보내는 값 = 단계 숫자, ESP32에서 x25.5 = PWM)
#include "rclcpp/rclcpp.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include <memory>
#include <iostream>

int main(int argc, char * argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<rclcpp::Node>("motor_pub");
    auto qos_profile = rclcpp::QoS(rclcpp::KeepLast(10));
    auto mypub = node->create_publisher<geometry_msgs::msg::Twist>(
        "/rc_car/cmd_vel", qos_profile);
    char key;
    double speed = 4.0;   // 처음 속도 4단계 (ESP32에서 x25.5 -> PWM 102)
    std::cout << "f: 전진, b: 후진, l: 좌회전, r: 우회전, s: 정지, q: 종료" << std::endl;
    std::cout << "+: 속도 올리기, -: 속도 내리기 (현재 " << speed << "단계)" << std::endl;
    while (rclcpp::ok())
    {
        std::cin >> key;
        geometry_msgs::msg::Twist message;   // 기본값 0 = 정지
        if (key == 'q') {
            mypub->publish(message);          // 종료 전에 정지
            break;
        }
        if (key == '+' || key == '-') {
            speed += (key == '+') ? 1.0 : -1.0;
            if (speed > 10.0) speed = 10.0;
            if (speed < 1.0) speed = 1.0;
            std::cout << "속도 " << speed << "단계 (PWM " << int(speed * 25.5) << "/255)" << std::endl;
            continue;
        }
        if (key == 'f') message.linear.x = speed;
        else if (key == 'b') message.linear.x = -speed;
        else if (key == 'l') message.angular.z = speed;
        else if (key == 'r') message.angular.z = -speed;
        else if (key == 's') {}               // 모두 0 -> 정지
        else {
            std::cout << "f, b, l, r, s, +, -, q 중 하나를 입력하세요." << std::endl;
            continue;
        }
        mypub->publish(message);
        RCLCPP_INFO(node->get_logger(), "Publish: linear.x = %.2f, angular.z = %.2f",
            message.linear.x, message.angular.z);
    }
    rclcpp::shutdown();
    return 0;
}
