#include <Arduino.h>   // .cpp 에서는 직접 넣어야 함 (.ino 는 자동으로 넣어줌)
// motor_sub (ESP32): /rc_car/cmd_vel 을 ESP32가 직접 구독해서 모터 구동 (ESP32-CAM)
//   linear.x  > 0 : 전진    < 0 : 후진
//   angular.z > 0 : 좌회전  < 0 : 우회전
//   모두 0        : 정지
// 속도 = 받은 값(단계 1~10)의 크기 x 25.5  (1 -> 25, 4 -> 102, 10 -> 255 최대)
// 명령 1번에 MOVE_TIME_MS(2초) 동안만 움직이고 자동 정지 (신호가 끊겨도 안전)
//
// 보드: AI Thinker ESP32-CAM / ESP32 코어 2.0.17 / 파티션 Minimal SPIFFS (OTA용)
// 라이브러리: micro_ros_arduino (v2.0.8-jazzy, MTU 1400 으로 재빌드한 버전)
// OTA: 호스트명 esp32-cam-ros -> ~/microros_ws/ota/ota_upload.sh 로 무선 업로드

#include <micro_ros_arduino.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <rmw_microros/rmw_microros.h>
#include <geometry_msgs/msg/twist.h>
#include <ArduinoOTA.h>

// ===== 네트워크 설정 =====
char ssid[]     = "YOUR_WIFI_SSID";       // 공유기 이름 (2.4GHz)
char password[] = "YOUR_WIFI_PASSWORD";   // 공유기 비밀번호
char agent_ip[] = "192.168.0.188";   // WSL(=Windows PC) IP. 바뀌면 여기 수정
const uint16_t AGENT_PORT = 8888;

// ===== 모터 핀 (Keyestudio 원본과 동일) =====
#define MOTOR_R_PIN_1 14
#define MOTOR_R_PIN_2 15
#define MOTOR_L_PIN_1 13
#define MOTOR_L_PIN_2 12
#define LED_GPIO_NUM  4

const float SPEED_SCALE = 25.5;         // 받은 값 1 당 PWM 25.5 (10단계 = 255)
int motor_speed = 102;                   // 현재 속도 0~255 (명령 받을 때마다 바뀜)
const unsigned long MOVE_TIME_MS = 2000; // 한 번에 움직이는 시간

rcl_allocator_t allocator;
rclc_support_t support;
rcl_node_t node;
rcl_subscription_t cmd_sub;
rclc_executor_t executor;
geometry_msgs__msg__Twist cmd_msg;

unsigned long stop_at = 0;   // 이 시간이 되면 자동 정지 (0 = 정지 상태)

enum State { WAITING_AGENT, AGENT_AVAILABLE, AGENT_CONNECTED, AGENT_DISCONNECTED };
State state = WAITING_AGENT;

#define RCCHECK(fn) { if ((fn) != RCL_RET_OK) return false; }

// 모터 4개 핀에 PWM 값 쓰기 (R1, R2, L1, L2)
void motor(int r1, int r2, int l1, int l2) {
  analogWrite(MOTOR_R_PIN_1, r1);
  analogWrite(MOTOR_R_PIN_2, r2);
  analogWrite(MOTOR_L_PIN_1, l1);
  analogWrite(MOTOR_L_PIN_2, l2);
}
void car_forward()  { motor(0, motor_speed, motor_speed, 0); }
void car_backward() { motor(motor_speed, 0, 0, motor_speed); }
void car_left()     { motor(0, motor_speed, 0, motor_speed); }
void car_right()    { motor(motor_speed, 0, motor_speed, 0); }

// 받은 값 크기 -> PWM 속도 (0~255)
int to_speed(float v) {
  int sp = (int)(fabs(v) * SPEED_SCALE);
  if (sp > 255) sp = 255;
  return sp;
}
void car_stop()     { motor(0, 0, 0, 0); stop_at = 0; }

// /rc_car/cmd_vel 을 받으면 실행
void cmd_callback(const void *msgin) {
  const geometry_msgs__msg__Twist *m = (const geometry_msgs__msg__Twist *)msgin;
  const char *action;
  float v = (m->linear.x != 0) ? m->linear.x : m->angular.z;
  motor_speed = to_speed(v);
  if      (m->linear.x  > 0) { car_forward();  action = "forward"; }
  else if (m->linear.x  < 0) { car_backward(); action = "backward"; }
  else if (m->angular.z > 0) { car_left();     action = "left"; }
  else if (m->angular.z < 0) { car_right();    action = "right"; }
  else                       { car_stop();     action = "stop"; }

  if (strcmp(action, "stop") != 0) stop_at = millis() + MOVE_TIME_MS;
  Serial.printf("[RX] linear.x=%.2f angular.z=%.2f -> %s (speed %d)\n", m->linear.x, m->angular.z, action, motor_speed);
}

bool create_entities() {
  allocator = rcl_get_default_allocator();
  RCCHECK(rclc_support_init(&support, 0, NULL, &allocator));
  RCCHECK(rclc_node_init_default(&node, "motor_sub", "", &support));
  RCCHECK(rclc_subscription_init_default(&cmd_sub, &node,
          ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist), "rc_car/cmd_vel"));
  executor = rclc_executor_get_zero_initialized_executor();
  RCCHECK(rclc_executor_init(&executor, &support.context, 1, &allocator));
  RCCHECK(rclc_executor_add_subscription(&executor, &cmd_sub, &cmd_msg, &cmd_callback, ON_NEW_DATA));
  return true;
}

void destroy_entities() {
  rmw_context_t *rmw_context = rcl_context_get_rmw_context(&support.context);
  (void)rmw_uros_set_context_entity_destroy_session_timeout(rmw_context, 0);
  rcl_subscription_fini(&cmd_sub, &node);
  rclc_executor_fini(&executor);
  rcl_node_fini(&node);
  rclc_support_fini(&support);
}

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_R_PIN_1, OUTPUT);
  pinMode(MOTOR_R_PIN_2, OUTPUT);
  pinMode(MOTOR_L_PIN_1, OUTPUT);
  pinMode(MOTOR_L_PIN_2, OUTPUT);
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);
  car_stop();

  set_microros_wifi_transports(ssid, password, agent_ip, AGENT_PORT);
  Serial.print("WiFi OK, ESP32 IP: ");
  Serial.println(WiFi.localIP());

  ArduinoOTA.setHostname("esp32-cam-ros");
  ArduinoOTA.setPassword("YOUR_OTA_PASSWORD");
  ArduinoOTA.onStart([]() { car_stop(); Serial.println("OTA 업로드 시작"); });
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();

  // 2초 지나면 자동 정지
  if (stop_at != 0 && (long)(millis() - stop_at) >= 0) {
    car_stop();
    Serial.println("자동 정지");
  }

  switch (state) {
    case WAITING_AGENT:
      if (rmw_uros_ping_agent(100, 1) == RMW_RET_OK) state = AGENT_AVAILABLE;
      else delay(200);
      break;
    case AGENT_AVAILABLE:
      if (create_entities()) { state = AGENT_CONNECTED; Serial.println("Agent 연결됨!"); }
      else { destroy_entities(); state = WAITING_AGENT; }
      break;
    case AGENT_CONNECTED: {
      static unsigned long last_ping = 0;
      if (millis() - last_ping > 1000) {
        last_ping = millis();
        if (rmw_uros_ping_agent(100, 3) != RMW_RET_OK) { state = AGENT_DISCONNECTED; break; }
      }
      rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
      break;
    }
    case AGENT_DISCONNECTED:
      Serial.println("Agent 끊김 -> 정지 후 재연결 대기");
      car_stop();
      destroy_entities();
      state = WAITING_AGENT;
      break;
  }
}
