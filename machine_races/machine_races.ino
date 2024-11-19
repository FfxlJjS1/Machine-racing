#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <SparkFun_APDS9960.h>

#include "Stack.h"

#define BUTTON_FOR_START A0
#define TAKE_OFF_LEFT_LASER_RANGEFINDER_OUTPUT A2
#define TAKE_OFF_RIGHT_LASER_RANGEFINDER_OUTPUT A3

#define Trig 9   //GPIO9  (D9)
#define Echo 10  //GPIO10 (D10)

// Global Variables for APDS9960 - color detector
SparkFun_APDS9960 apds = SparkFun_APDS9960();
uint16_t ambient_light = 0;
uint16_t red_light = 0;
uint16_t green_light = 0;
uint16_t blue_light = 0;

//Line order test function of trolley wheel
#define IN_1  8            // L9110S B-1A motors Right Back       GPIO0 (D8)
#define IN_2  4            // L9110S B-2A motors Right Forw       GPIO4 (D4)
#define IN_3  12           // L9110S A-1A motors Left Back      GPIO7(D7)
#define IN_4  7           // L9110S A-1B motors Left Forw      GPIO12(D12)
int speedCar = 180;
int coeff_to_turn_90_degres = 260;

#define STOP_PATH_CODE 0b00000000 // обозначение, что программа пришла в черный квадрат
#define GO_FORWARD_PATH_CODE 0b00000001
#define GO_RIGHT_PATH_CODE 0b00000010
#define GO_LEFT_PATH_CODE 0b00000100
#define GO_BACK_PATH_CODE 0b00001000 // обозначение, что необходимо двигаться к предыдущей развилке, где доступно другое движение

#define RIGHT_PATH_TYPE_CODE 0b00000001
#define LEFT_PATH_TYPE_CODE 0b00000010
#define RIGHT_LEFT_PATH_TYPE_CODE 0b00000011
#define FORWARD_PATH_TYPE_CODE 0b00000100
#define RIGHT_FORWARD_PATH_TYPE_CODE 0b00000101
#define FORWARD_LEFT_PATH_TYPE_CODE 0b00000110
#define RIGHT_FORWARD_LEFT_PATH_TYPE_CODE 0b00000111
#define BACK_PATH_TYPE_CODE 0b00000000
#define FINISH_POSITION_CODE 0b00001000 // обозначение, что маршрут достиг конца - финиша

Adafruit_VL53L0X lox =  Adafruit_VL53L0X(); // Интерфейс для работы с лазерными дальномерами

Stack<uint8_t> steps_for_save; // запасной стек для возврата значений шага при повторных движениях к финишу
Stack<uint8_t> steps;
/*
Байт содержит две информационные единицы:
- 4 старших бита - код, описывающий куда поехал, относительно позиции, в которой встретил перекресток:
  - 0000 - стоп
  - 0001 - прямо
  - 0010 - поворот вправо
  - 0100 - поворот влево
  - 1000 - разворот на месте
- 4 младших бита - код, описывающий тип развилки:
  - 0100 - прямая развилка, используется для стартовой точки, если она такая
  - 0001 - развилка только вправо
  - 0010 - развилка только влево
  - 0011 - развилка и вправо и влево
  - 0100 - движение вперед
  - 0101 - развилка вправо и прямо
  - 0110 - развилка влево и прямо
  - 0111 - развилка вправо, прямо и влево
  - 0000 - движение только назад, до предыдущей развилки, где возможно другое движение
*/
bool is_forward = true; // показывает, что тупик не был встречен на данной развилке, иначе движение будет идти в обратном направлении
bool is_first_start = true; // показывает, что карта шагов еще не построена, поэтому работает метод поиска финиша, а не движения к нему
bool is_paused = true; // показывает состояние машинки, если ее включили

struct apds_color {
  unsigned short ambient_light = 0;
  unsigned short red_light = 0;
  unsigned short green_light = 0;
  unsigned short blue_light = 0;
};

// метод первичной настройки машинки
void setup() {
  Serial.begin(9600);

  pinMode(IN_1, OUTPUT);
  pinMode(IN_2, OUTPUT);
  pinMode(IN_3, OUTPUT);
  pinMode(IN_4, OUTPUT);

  act_to_stop_follow();

  pinMode(TAKE_OFF_LEFT_LASER_RANGEFINDER_OUTPUT, OUTPUT);
  pinMode(TAKE_OFF_RIGHT_LASER_RANGEFINDER_OUTPUT, OUTPUT);

  // Define ultrasonic sensor pins
  pinMode(Trig, OUTPUT);
  pinMode(Echo, INPUT);

  // Initialize APDS-9960 (configure I2C and initial values)
  if ( apds.init() ) {
    Serial.println(F("APDS-9960 initialization complete"));
  } else {
    Serial.println(F("Something went wrong during APDS-9960 init!"));
  }
  
  // Start running the APDS-9960 light sensor (no interrupts)
  if ( apds.enableLightSensor(false) ) {
    Serial.println(F("Light sensor is now running"));
  } else {
    Serial.println(F("Something went wrong during light sensor init!"));
  }

  // Initialize VL53O0X
  if (!lox.begin()) {
    Serial.println("Failed to boot");
  }
  
  // Wait for initialization and calibration to finish
  delay(500);

  is_paused = false;
}

apds_color read_apds_color() {
  apds_color apds_result;

  // Read the light levels (ambient, red, green, blue)
  if (  !apds.readAmbientLight(ambient_light) ||
        !apds.readRedLight(red_light) ||
        !apds.readGreenLight(green_light) ||
        !apds.readBlueLight(blue_light) ) {
    Serial.println("Error reading light values");
  } else {
    /*
    Serial.print("Ambient: ");
    Serial.print(ambient_light);
    Serial.print(" Red: ");
    Serial.print(red_light);
    Serial.print(" Green: ");
    Serial.print(green_light);
    Serial.print(" Blue: ");
    Serial.println(blue_light);
    */

    apds_result.ambient_light = ambient_light;
    apds_result.red_light = red_light;
    apds_result.green_light = green_light;
    apds_result.blue_light = blue_light;
  }

  // Wait 1 second before next reading
  delay(1000);

  return apds_result;
}


long read_distance_from_enable_laser_distancefinder() {
  // put your main code here, to run repeatedly:
  VL53L0X_RangingMeasurementData_t measure;
  long laser_distance = 0;
  int try_number = 1;
  
  do {
    delay(100);

    Serial.print("Reading a measurement...");
    lox.rangingTest(&measure, false);


    laser_distance = measure.RangeMilliMeter;

    try_number++;
  } while(try_number <= 6 && laser_distance == 20);

  return laser_distance;
}

float read_distance_from_ultrasonic_distancefinder() {
  digitalWrite(Trig, LOW);
  delayMicroseconds(5);
  
  digitalWrite(Trig, HIGH);
  delayMicroseconds(10);
  
  digitalWrite(Trig, LOW);
  
  float distance = pulseIn(Echo, HIGH) / 58.00;

  return distance;  //Return distance
}

// Чтение дистанции с левого лазерного дальномера с выключением правого
long read_distance_from_left_laser_distancefinder() {
  digitalWrite(TAKE_OFF_RIGHT_LASER_RANGEFINDER_OUTPUT, HIGH);
  digitalWrite(TAKE_OFF_LEFT_LASER_RANGEFINDER_OUTPUT, LOW);
  
  delay(50);

  long read_distance = read_distance_from_enable_laser_distancefinder();

  return read_distance;
}

// Чтение дистанции с левого лазерного дальномера с выключением правого
long read_distance_from_right_laser_distancefinder() {
  digitalWrite(TAKE_OFF_RIGHT_LASER_RANGEFINDER_OUTPUT, LOW);
  digitalWrite(TAKE_OFF_LEFT_LASER_RANGEFINDER_OUTPUT, HIGH);

  delay(50);

  long read_distance = read_distance_from_enable_laser_distancefinder();

  return read_distance;
}


// код для поворота на определенный градус от -180 до 180
void act_to_rotate_to_value(int coefficient) {
  if (coefficient > 0) {
    digitalWrite(IN_1, LOW);
    analogWrite(IN_2, speedCar);
    analogWrite(IN_3, speedCar);
    digitalWrite(IN_4, LOW);
  }
  else if (coefficient < 0) {
    digitalWrite(IN_1, speedCar);
    analogWrite(IN_2, LOW);
    analogWrite(IN_3, LOW);
    digitalWrite(IN_4, speedCar);
  }

  delay(abs(coefficient));

  act_to_stop_follow();
}

void act_to_go_for_coefficient(long coefficient) {
  Serial.print("Coefficient :");
  Serial.println(coefficient);

  if (coefficient > 0) {
    Serial.println("Go forward");

    digitalWrite(IN_1, LOW);
    analogWrite(IN_2, speedCar);
    analogWrite(IN_3, LOW);
    digitalWrite(IN_4, speedCar);
  }
  else if (coefficient < 0) {
    Serial.println("Go back");

    digitalWrite(IN_1, speedCar);
    analogWrite(IN_2, LOW);
    analogWrite(IN_3, speedCar);
    digitalWrite(IN_4, LOW);
  }

  delay(abs(coefficient));

  act_to_stop_follow();
}

// метод для каллибровки позиции машинки в лабиринте относительно стен
void callibrate_machine_position() {
  long left_distance = read_distance_from_left_laser_distancefinder();
  long right_distance = read_distance_from_right_laser_distancefinder();
  float forward_distance = read_distance_from_ultrasonic_distancefinder();

  Serial.println(left_distance);
  Serial.println(right_distance);

  int coeff_to_rotate = 90;
  int max_iteration_count = 3;
  int current_iteration = 1;

  // Корректировка позиции относительно передней стени
  if (forward_distance < 22) {
    while (forward_distance < 7) {
      act_to_go_for_coefficient(-130);

      forward_distance = read_distance_from_ultrasonic_distancefinder();
    }

    while (forward_distance > 9) {
      act_to_go_for_coefficient(130);

      forward_distance = read_distance_from_ultrasonic_distancefinder();
    }
  }

  if (left_distance < 95) {
    act_to_rotate_to_value(coeff_to_turn_90_degres);

    act_to_go_for_coefficient(-130);

    act_to_rotate_to_value(-coeff_to_turn_90_degres);
  }
  else if (right_distance < 95) {
    act_to_rotate_to_value(-coeff_to_turn_90_degres);

    act_to_go_for_coefficient(-130);

    act_to_rotate_to_value(coeff_to_turn_90_degres);
  }

  // Корректировка позиции относительно стонок
  if (forward_distance < 24) {
    // Каллибровка вдоль передней стенки
    long new_forward_distance = 0;

    do {
      act_to_rotate_to_value(coeff_to_rotate);

      new_forward_distance = read_distance_from_ultrasonic_distancefinder();

      if (forward_distance < new_forward_distance) {
        act_to_rotate_to_value(-coeff_to_rotate);
        
        coeff_to_rotate = -coeff_to_rotate;
      }
      else {
        forward_distance = new_forward_distance;
      }
      
      coeff_to_rotate = coeff_to_rotate / 2;

      current_iteration++;
    } while (current_iteration <= max_iteration_count + 1);
  }

  // Проверка по катетам
  if (left_distance + right_distance < 320) {
    int trying = 1;

    do {
      act_to_rotate_to_value(coeff_to_rotate);

      long new_left_distance = read_distance_from_left_laser_distancefinder();
      long new_right_distance = read_distance_from_right_laser_distancefinder();

      if (new_left_distance + new_right_distance > left_distance + right_distance) {
        act_to_rotate_to_value(- coeff_to_rotate);

        coeff_to_rotate = - coeff_to_rotate / 2;
      }
      else {
        left_distance = new_left_distance;
        right_distance = new_right_distance;

        coeff_to_rotate = coeff_to_rotate / 2;
      }

      trying++;
    } while(trying <= max_iteration_count + 1);
  }

  // Каллибровка вдоль одной из стенок (правой, левой или передней) до примерного перпендикуляра
  else if (left_distance > 80 && left_distance < 140) {
    // Каллибровка вдоль левой стенки
    long new_left_distance = 0;

    do {
      act_to_rotate_to_value(coeff_to_rotate);

      new_left_distance = read_distance_from_left_laser_distancefinder();

      if (left_distance < new_left_distance) {
        act_to_rotate_to_value(-coeff_to_rotate);
        
        coeff_to_rotate = -coeff_to_rotate;
      }
      else {
        left_distance = new_left_distance;
      }
      
      coeff_to_rotate = coeff_to_rotate / 2;

      current_iteration++;
    } while (current_iteration <= max_iteration_count && left_distance > 75);
  }
  else if (right_distance > 80 && right_distance < 140) {
    // Каллибровка вдоль правой стенки
    long new_right_distance = 0;

    do {
      act_to_rotate_to_value(coeff_to_rotate);

      new_right_distance = read_distance_from_right_laser_distancefinder();

      if (right_distance < new_right_distance) {
        act_to_rotate_to_value(-coeff_to_rotate);
        
        coeff_to_rotate = -coeff_to_rotate;
      }
      else {
        right_distance = new_right_distance;
      }
      
      coeff_to_rotate = coeff_to_rotate / 2;

      current_iteration++;
    } while (current_iteration <= max_iteration_count && right_distance > 75);
  }

  // Корректировка позиции относительно передней стени
  if (forward_distance < 24) {
    while (forward_distance < 7) {
      act_to_go_for_coefficient(-130);

      forward_distance = read_distance_from_ultrasonic_distancefinder();
    }

    while (forward_distance > 9) {
      act_to_go_for_coefficient(130);

      forward_distance = read_distance_from_ultrasonic_distancefinder();
    }
  }

}

// функция для движения до первой развилки, тупика или финиша
void act_to_go_forward() {
  uint8_t new_path_type = BACK_PATH_TYPE_CODE;

  do {
    act_to_go_for_coefficient(700);

    act_to_stop_follow();

    callibrate_machine_position();
    
    new_path_type = determine_path_type();
  } while(new_path_type == FORWARD_PATH_TYPE_CODE);
}

// функция для определения, является ли квадрат финишом или нет
bool detect_is_finish() {
  apds_color apds_possition_color =  read_apds_color();

  return apds_possition_color.ambient_light <= 1 && apds_possition_color.blue_light <= 1 && apds_possition_color.green_light <= 1 && apds_possition_color.red_light <= 1;
}

void act_to_stop_follow() {
  digitalWrite(IN_1, LOW);
  digitalWrite(IN_2, LOW);
  digitalWrite(IN_3, LOW);
  digitalWrite(IN_4, LOW);
  
  delay(300);
}

// метод в котором определяется тип развилки через сенсоры
uint8_t determine_path_type() {
  uint8_t path_type = BACK_PATH_TYPE_CODE;

  bool is_finish = detect_is_finish();

  Serial.print("Finish state: ");
  Serial.println(is_finish);

  if (is_finish) {
    path_type = FINISH_POSITION_CODE;
  }
  else {
    long left_distance = read_distance_from_left_laser_distancefinder();
    long right_distance = read_distance_from_right_laser_distancefinder();
    float forward_distance = read_distance_from_ultrasonic_distancefinder();

    if (left_distance > 270) {
      path_type |= LEFT_PATH_TYPE_CODE;
    }
    if (right_distance > 270) {
      path_type |= RIGHT_PATH_TYPE_CODE;
    }
    if (forward_distance > 23) {
      path_type |= FORWARD_PATH_TYPE_CODE;
    }
  }

  return path_type;
}


// получение кода, описывающего куда поехал
uint8_t get_step_path(uint8_t step) {
  return step >> 4;
}

// получение кода, описывающего развилку
uint8_t get_path_type(uint8_t step) {
  return (step << 4) >> 4;
}

uint8_t combine__path_type__with__step_path(uint8_t path_type, uint8_t step_type) {
  return (step_type << 4) | path_type;
}

// функция определения движения на неизведанных развилках при встрече в первой попытке
uint8_t determine_step_path_for_path_type(uint8_t path_type) {
  if (path_type == FINISH_POSITION_CODE) {
    return STOP_PATH_CODE;
  }
  else if (path_type == RIGHT_PATH_TYPE_CODE || path_type == RIGHT_LEFT_PATH_TYPE_CODE
          || path_type == RIGHT_FORWARD_PATH_TYPE_CODE || path_type == RIGHT_FORWARD_LEFT_PATH_TYPE_CODE) {
    return GO_RIGHT_PATH_CODE;
  }
  else if (path_type == FORWARD_PATH_TYPE_CODE || path_type == FORWARD_LEFT_PATH_TYPE_CODE) {
    return GO_FORWARD_PATH_CODE;
  }
  else if (path_type == LEFT_PATH_TYPE_CODE) {
    return GO_LEFT_PATH_CODE;
  }
  else if (path_type == BACK_PATH_TYPE_CODE) {
    return GO_BACK_PATH_CODE;
  }

  return 0;
}

void act_to_go_standart(uint8_t step_type) {
  if(step_type == GO_FORWARD_PATH_CODE) {
    act_to_go_forward();
  }
  else if (step_type == GO_RIGHT_PATH_CODE) {
    // Попробовать докручивать по сохранившимся дальностям стенок с попыткой повторить это же относительно симметричных лазерных дальномеров с докруткой в сторону поворота
    act_to_rotate_to_value(-coeff_to_turn_90_degres);

    act_to_go_forward();
  }
  else if (step_type == GO_LEFT_PATH_CODE) {
    // Попробовать докручивать по сохранившимся дальностям стенок с попыткой повторить это же относительно симметричных лазерных дальномеров с докруткой в сторону поворота
    act_to_rotate_to_value(coeff_to_turn_90_degres);

    act_to_go_forward();
  }
  else if (step_type == GO_BACK_PATH_CODE) {
    // Попробовать докручивать по сохранившимся дальностям стенок с попыткой повторить это же относительно симметричных лазерных дальномеров с докруткой в сторону поворота
    is_forward = false;

    long left_distance = read_distance_from_left_laser_distancefinder();

    if (left_distance < 110) {
      act_to_rotate_to_value(coeff_to_turn_90_degres);
      act_to_rotate_to_value(coeff_to_turn_90_degres);
    }
    else {
      act_to_rotate_to_value(- coeff_to_turn_90_degres);
      act_to_rotate_to_value(- coeff_to_turn_90_degres);
    }

    callibrate_machine_position();

    act_to_go_forward();
  }
  else if (step_type == STOP_PATH_CODE){
    act_to_go_for_coefficient(300);

    // callibrate_machine_position();

    act_to_stop_follow();
  }
}

// выполняет движение до развилки или тупике
void act_to_go(uint8_t step) {
  uint8_t step_path = get_step_path(step);
  uint8_t path_type = get_path_type(step);

  Serial.print("Path type: ");
  Serial.println(path_type);

  Serial.print("Step path: ");
  Serial.println(step_path);

  if (step_path == STOP_PATH_CODE) {
    Serial.println("Is paused");

    is_paused = true;

    if (is_first_start) {
      is_first_start = false;

      reverse_steps_stack();

      steps_for_save = steps;
    }
    else {
      steps = steps_for_save; // чтобы вернуть копию стека пути к финишу в стек, из которого с начала до конца были взяты все шаги
    }
  }
  else if (is_first_start) {
    if (step_path == BACK_PATH_TYPE_CODE) {
      act_to_go_standart(GO_BACK_PATH_CODE);

      is_paused = true;

      Serial.println("Back");

      // Будет неправильно, т.к. не учитывает, что проехал уже по маршруту и не заменяет неверный маршрут на новую попытку

      while(!is_forward) {
        step = steps.pop();

        step_path = get_step_path(step);
        path_type = get_path_type(step);

        Serial.print("Path type: ");
        Serial.println(path_type);

        Serial.print("Step path: ");
        Serial.println(step_path);

        step_path = determine_path_to_prev_path(step);

        step = combine__path_type__with__step_path(path_type, step_path);

        steps.push(step);

        act_to_go_standart(step_path);
      }
    }
    else {
      steps.push(step); // вернуть информацию о том, куда было выполнено движение

      act_to_go_standart(step_path);
    }
  }
  else {
    act_to_go_standart(step_path);
  }
}

// функция для определения движения для возврата к последней развилке с неизведанными путями движения к финишу
uint8_t determine_path_to_prev_path(uint8_t step) {
  uint8_t path_type = get_path_type(step);
  uint8_t step_path = get_step_path(step);

  // движение назад до предыдущей развилки, на которой остались не пройденные пути

  // поворот в другой путь на развилке против часовой стрелки
  if (path_type == RIGHT_LEFT_PATH_TYPE_CODE){
    // так как при обратном направлении мы должны повернуть в другой путь на развилке, то, при вправо-влево, мы возвращаемся из правой развилки и нам необходимо попасть в левую
    // чтобы не поворачиваться в начальную позицию, как мы впервые встретили развилку, мы оптимизируем путь движения
    is_forward = true;

    return GO_FORWARD_PATH_CODE;
  }
  else if (path_type == RIGHT_FORWARD_PATH_TYPE_CODE) {
    is_forward = true;
    
    return GO_RIGHT_PATH_CODE;
  }
  else if (path_type == FORWARD_LEFT_PATH_TYPE_CODE) {
    is_forward = true;
    
    return GO_RIGHT_PATH_CODE;
  }
  else if (path_type == RIGHT_FORWARD_LEFT_PATH_TYPE_CODE) {
    is_forward = true;
    
    return GO_RIGHT_PATH_CODE;
  }

  // движение до предыдущей доступной развилки
  else if (path_type == RIGHT_PATH_TYPE_CODE) {
    return GO_LEFT_PATH_CODE;
  }
  else if (path_type == LEFT_PATH_TYPE_CODE) {
    return GO_RIGHT_PATH_CODE;
  }
  else if (path_type == FORWARD_PATH_TYPE_CODE) {
    return GO_FORWARD_PATH_CODE;
  }

  return STOP_PATH_CODE;
}

void reverse_steps_stack() {
  Stack<uint8_t> reversed_stack;

  while (steps.length() > 0) {
    reversed_stack.push(steps.pop());
  }

  steps = reversed_stack;
}

void go() {
  // put your main code here, to run repeatedly:
  if (is_paused) {
    bool is_clicked = false;

    while(is_paused) {
      // получение кнопки и определение
      is_clicked = digitalRead(BUTTON_FOR_START);

      Serial.print("Is clicked: ");
      Serial.println(is_clicked);

      if (is_clicked)
        is_paused = false;

      delay(200);
    }

    while(is_clicked) {
      is_clicked = digitalRead(BUTTON_FOR_START);

      delay(100);
    }

    delay(100);
  }

  // каллибровка переменных машинки для движения
  callibrate_machine_position();

  if (is_first_start) {
    // определение пути на позиции
    uint8_t path_type = determine_path_type();

    Serial.print("path type: ");
    Serial.println(path_type);

    uint8_t step_type = determine_step_path_for_path_type(path_type);

    Serial.print("step type: ");
    Serial.println(step_type);

    steps.push(combine__path_type__with__step_path(path_type, step_type));
  }

  // запуск движения
  uint8_t step = steps.pop();

  act_to_go(step);
}

// метод, который запускается повторениями
void loop() {
  go();

  // callibrate_machine_position();
}
