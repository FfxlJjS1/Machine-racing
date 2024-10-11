#include "Stack.h"

#define BUTTON_FOR_START A0

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

// метод первичной настройки машинки
void setup() {
  // ...
}


// код для поворота на определенный градус от -180 до 180
void act_to_rotate_to_value(int8_t degree) {
  // ...
}

// метод для каллибровки позиции машинки в лабиринте относительно стен
void callibrate_machine_position() {
  // ...
}

// функция для движения до первой развилки, тупика или финиша
void act_to_go_forward() {
  // ...
}

// функция для определения, является ли квадрат финишом или нет
bool detect_is_finish() {
  // ...
}

// метод в котором определяется тип развилки через сенсоры
uint8_t determine_path_type() {
  uint8_t path_type = BACK_PATH_TYPE_CODE;

  // код определения типа развилки
  // ...

  return path_type;
}


// получение кода, описывающего развилку
uint8_t get_path_type(uint8_t step) {
  return step >> 4;
}

// получение кода, описывающего куда поехал
uint8_t get_step_path(uint8_t step) {
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
    act_to_rotate_to_value(-90);
    act_to_go_forward();
  }
  else if (step_type == GO_LEFT_PATH_CODE) {
    act_to_rotate_to_value(90);
    act_to_go_forward();
  }
  else if (step_type == GO_BACK_PATH_CODE) {
    is_forward = false;

    act_to_rotate_to_value(180);
    act_to_go_forward();
  }
}

// выполняет движение до развилки или тупике
void act_to_go(uint8_t step) {
  uint8_t step_path = get_step_path(step);
  uint8_t path_type = get_path_type(step);

  if (step_path == STOP_PATH_CODE) {
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
    if (is_forward) {
      steps.push(step); // вернуть информацию о том, куда было выполнено движение

      act_to_go_standart(step_path);
    }
    else {
      step_path = determine_path_to_prev_path(step);

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

// метод, который запускается повторениями
void loop() {
  // put your main code here, to run repeatedly:
  if (is_paused) {
    bool is_clicked = false;

    while(is_paused) {
      // получение кнопки и определение
      bool is_clicked = digitalRead(BUTTON_FOR_START);

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

  if (is_first_start || is_forward) {
    // определение пути на позиции
    uint8_t path_type = determine_path_type();

    uint8_t step_type = determine_step_path_for_path_type(path_type);

    steps.push(combine__path_type__with__step_path(path_type, step_type));
  }

  // запуск движения
  uint8_t step = steps.pop();

  act_to_go(step);
}
