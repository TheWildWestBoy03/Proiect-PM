#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Wire.h>
#include <Rtc_Pcf8563.h>
#include <string.h>

// TFT setup
#define TFT_CS     10
#define TFT_RST    -1
#define TFT_DC     9
#define MAIN_MENU_STATE 0
#define GAME_STATE 1
#define WINNING_STATE 2
#define EXITING_STATE 3
#define LOSING_STATE 4

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
Rtc_Pcf8563 rtc;

// Joystick pins
#define JOY_VERT A0
#define JOY_HORZ A1
#define JOY_BUTTON 6
#define MOSI D11

struct Spaceship {
  uint16_t x_position;
  uint16_t y_position;
  uint16_t dimension;
  uint16_t damage;
  uint8_t health;
};

struct EnemySpaceship {
  bool is_alive;
  float x_position;
  float y_position;
  int16_t health;
  uint16_t dimension;
  uint16_t x_limit; // parametrul specific monstrului final
};

// reprezentare abstracta a coordonatelor entitatilor din joc
struct Coordinates {
    float x;
    float y;
};

// pinul conectat la buzzer
int buzzerPin = 8;

// notele muzicale de bun venit
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
int noteDuration = 500;

// Melody: "Do Re Mi"
int melody[] = {
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5,
};

int winNotes[] = {
  523, 587, 659, 698, 784, 880, 988, 1047,
  988, 1047, 1175, 1319, 1397, 1568, 1760, 1976,
  1760, 1568, 1397, 1319, 1175, 1047, 988, 1047
};

int loseNotes[] = {
  880, 830, 784, 740, 698, 659, 622, 587,
  554, 523, 494, 466, 440, 415, 392, 370,
  349, 330, 311, 294, 277, 262, 247, 233
};

// Meniul definit aici
const char* menuItems[] = {"Start Game", "Options", "Credits", "Exit"};
const int menuLength = sizeof(menuItems) / sizeof(menuItems[0]);
int selectedItem = 0;
int current_game_state = 0;
unsigned long lastMove = 0;
bool player_lost = false;
uint16_t sky_color;

// variabile limita, tinand cont de resursele limitate ale microcontrollerului
const uint8_t maximum_decoration_towers = 15;
const uint8_t maximum_bullets_on_screen = 10;
const uint8_t maximum_waves = 2;
const uint8_t maximum_enemies = 4;

// enemy information
uint8_t enemies_displayed = 0;
const uint8_t enemies_per_wave = 2;
EnemySpaceship enemies[maximum_enemies];
EnemySpaceship final_boss;
uint8_t current_wave = 0;
bool boss_ascending = true;

// informatiile despre starea turnurilor, inclusiv pozitii
bool drawn_decoration_towers = false;
uint16_t tower_x_position[maximum_decoration_towers];
uint16_t tower_y_position[maximum_decoration_towers] = {200, 170, 200, 210, 160, 190, 200, 190, 180, 140, 165, 185, 195, 200, 210};
uint8_t tower_length[maximum_decoration_towers];
uint8_t tower_height[maximum_decoration_towers] = {50, 80, 50, 40, 90, 60, 50, 60, 70, 110, 85, 65, 55, 50, 40};

// delay in ms folosit in implementarea debouncerului pentru butonul joystick-ului
int moveDelay = 500;
uint16_t bullet_x_positions[maximum_bullets_on_screen];
uint16_t bullet_y_positions[maximum_bullets_on_screen];
int winning_time = 0;

// logica gloantelor initializata aici
uint8_t bullet_state[maximum_bullets_on_screen];
uint16_t pending_bullet = 0;
const uint8_t bullet_radius = 2;
unsigned long last_shoot = 0;
const long shoot_delay = 500;
uint16_t player_score = 0;
const uint8_t shooting_limit_tolerance = 10;
const uint8_t passing_limit = 30;
bool finished_waves = false;

volatile unsigned long customMillis = 0;

void setupTimer1() {
  noInterrupts();

  TCCR1A = 0;
  TCCR1B = 0;

  TCNT1 = 0;

  OCR1A = 249;              // (16*10^6) / (64*1000) - 1 = 249 (for 1kHz)
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A);

  interrupts();
}

ISR(TIMER1_COMPA_vect) {
  customMillis++;
}

unsigned long my_millis() {
  return customMillis;
}

// functia de initializare a convertorului analog-digital
void adc_init(void) {
    ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    ADMUX = (1 << REFS0);

    ADCSRA |= (1 << ADEN);
}

// functia implementata de la laborator
uint16_t myAnalogRead(uint8_t channel) {
    channel &= 0x07;  // only 0-7 valid

    ADMUX = (ADMUX & 0xF0) | channel;
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));

    return ADC;
}

// deseneaza nava spatiala, folosind culoarea data ca parametru
void draw_spaceship (Spaceship spaceship, uint16_t color) {
    tft.setCursor(spaceship.x_position, spaceship.y_position);
    Coordinates left_up_dot;
    left_up_dot.x = spaceship.x_position;
    left_up_dot.y = spaceship.y_position - 10; 

    Coordinates left_down_dot;
    left_down_dot.x = spaceship.x_position;
    left_down_dot.y = spaceship.y_position + 10;

    Coordinates right_dot;
    right_dot.x = spaceship.x_position + 30;
    right_dot.y = spaceship.y_position;

    tft.fillTriangle(left_down_dot.x, left_down_dot.y, 
                     left_up_dot.x, left_up_dot.y,
                     right_dot.x, right_dot.y, color);
}

// functie care dese
void draw_enemy_spaceship(EnemySpaceship enemy, uint16_t color, int dimension) {
  tft.setCursor(enemy.x_position, enemy.y_position);

  Coordinates left_dot;
  left_dot.x = enemy.x_position;
  left_dot.y = enemy.y_position; 

  Coordinates right_down_dot;
  right_down_dot.x = enemy.x_position + dimension;
  right_down_dot.y = enemy.y_position + dimension;

  Coordinates right_up_dot;
  right_up_dot.x = enemy.x_position + dimension;
  right_up_dot.y = enemy.y_position - dimension;

  tft.fillTriangle(left_dot.x, left_dot.y, 
                    right_up_dot.x, right_up_dot.y,
                    right_down_dot.x, right_down_dot.y, color);
}

// initializeaza jucatorul si specificatiile lui
Spaceship init_player(float x, float y) {
  tft.fillScreen(ILI9341_BLACK);
  int hour = rtc.getHour();

  if (hour >= 17 || hour <= 6) {
    sky_color = tft.color565(150, 165, 0);
  } else {
    sky_color = ILI9341_BLUE;
  }

  tft.fillScreen(sky_color);

  Spaceship new_spaceship;
  new_spaceship.x_position = x;
  new_spaceship.y_position = y;
  new_spaceship.damage = 40;
  new_spaceship.health = 3;
  new_spaceship.dimension = 10;

  draw_spaceship(new_spaceship, ILI9341_BLACK);
  return new_spaceship;
}

// initializeaza dimensiunile turnurilor decorative
void setup_tower_information() {
  for (uint8_t i = 0; i < maximum_decoration_towers; i++) {
    tower_x_position[i] = i * 22;
    tower_length[i] = 21;
  }
}

// seteaza disponibilitatea gloantelor
void setup_bullet_statements() {
  for (uint8_t i = 0; i < maximum_bullets_on_screen; i++) {
    bullet_state[i] = 1;
  }
}

void setup() {
  Serial.begin(9600);
  Wire.begin();
  tft.begin();
  adc_init();
  setupTimer1();

  tft.setRotation(3);
  tft.fillScreen(ILI9341_MAROON);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_BLACK);
  
  // folosesc pull-up-ul butonului din joystick
  pinMode(JOY_BUTTON, INPUT_PULLUP);
  drawMenu();

  setup_tower_information();
  setup_bullet_statements();

  current_game_state = MAIN_MENU_STATE;

  // ruleaza melodia de bun venit  
  for (int i = 0; i < 8; i++) {
    tone(buzzerPin, melody[i], noteDuration);
    delay(noteDuration / 5);  // Slight delay between notes
  }

  rtc.initClock();

  // obtin informatiile legate de ora locala
  setRtcFromCompileTime();
}

void draw_decoration_towers() {
  for (uint8_t tower = 0; tower < maximum_decoration_towers; ++tower) {
    uint16_t color;
    if (tower % 3 == 0) {
      color = ILI9341_RED;
    } else if (tower % 3 == 1) {
      color = ILI9341_PURPLE;
    } else {
      color = ILI9341_YELLOW;
    }

    tft.fillRect(tower_x_position[tower], tower_y_position[tower], tower_length[tower], tower_height[tower], color);

    // aici desenez niste margini mai groase pentru turnuri
    for (uint8_t thickness = 0; thickness < 3; thickness++) {
      tft.drawRect(tower_x_position[tower] + thickness, tower_y_position[tower] + thickness, tower_length[tower] - 2 * thickness, 
                                          tower_height[tower] - 2 * thickness, ILI9341_BLACK);
    }
  }
}

void game_loop(Spaceship &spaceship) {

  // conditia necesara si suficienta de castig
  if (final_boss.is_alive == false && finished_waves == true) {
    current_game_state = WINNING_STATE;
    tft.fillScreen(ILI9341_MAROON);
    tft.setTextSize(2);
    tft.setTextColor(ILI9341_YELLOW);
    tft.setCursor(20, 20);
    tft.print("Congratulations!!! You won the game!!");
    for (int i = 0; i < 24; i++) {
      tone(buzzerPin, winNotes[i], noteDuration);
      delay(noteDuration / 2);  // Slight delay between notes
    }
    return;
  }

  // gestionez lupta finala din joc
  if (current_wave >= maximum_waves) {
    // inseamna ca nu avem boss initializat
    if (finished_waves == false) {
      final_boss.x_limit = 200;
      final_boss.x_position = tft.width();
      final_boss.y_position = 50;
      final_boss.is_alive = true;
      final_boss.health = 100;
    }
  
    finished_waves = true;

    // valabil doar cand bossul inca nu a fost nimicit  
    if (final_boss.is_alive == true) {
      draw_enemy_spaceship(final_boss, sky_color, 30);

      // definirea miscarii bossului
      if (final_boss.x_position > final_boss.x_limit) {
        final_boss.x_position -= 2;
      } else {
        if (boss_ascending == true) {
          final_boss.y_position -= 0.3;
        } else {
          final_boss.y_position += 0.3;
        }

        if (final_boss.y_position < 10) {
          boss_ascending = false;
        } else if (final_boss.y_position >= 100) {
          boss_ascending = true;
        }
      }

      draw_enemy_spaceship(final_boss, ILI9341_BLACK, 30);

      // daca glontul omite bossul, jocul s-a incheiat
      for (uint8_t i = 0; i < pending_bullet; i++) {
        if (bullet_state[i] == 1) {
          if (bullet_x_positions[i] > tft.width()) {
            current_game_state = LOSING_STATE;
            tft.fillScreen(ILI9341_MAROON);
            tft.setTextSize(2);
            tft.setTextColor(ILI9341_YELLOW);
            tft.setCursor(20, 20);
            tft.print("You lost this game!! Try again!");
            
            // muzica specifica pierderii meciului
            for (int i = 0; i < 24; i++) {
              tone(buzzerPin, loseNotes[i], noteDuration);
              delay(noteDuration / 2);  // Slight delay between notes
            }
            player_lost = true;
            break;
          } else {
            // glontul se intersecteaza cu inamicul final
            if (abs(final_boss.x_position - bullet_x_positions[i]) < 30 && abs(final_boss.y_position - bullet_y_positions[i]) < 30) {
              final_boss.health -= 40;
              Serial.println(final_boss.health);
              tft.fillCircle(bullet_x_positions[i], bullet_y_positions[i], bullet_radius, sky_color);
              bullet_state[i] = 0;
              if (final_boss.health < 0) {
                final_boss.is_alive = false;
              }
            }
          }
        }
      }
    }
  }

  if (!drawn_decoration_towers) {
    draw_decoration_towers();
    drawn_decoration_towers = true;
  }

  if (enemies_displayed == 0 && current_wave < maximum_waves) {
    for (int i = 0; i < enemies_per_wave; i++) {
      enemies[i].x_position = tft.width() + shooting_limit_tolerance;
      enemies[i].y_position = (i + 1) * 30;
      enemies[i].is_alive = true;
      enemies[i].dimension = 10;

      draw_enemy_spaceship(enemies[i], ILI9341_RED, 10);
    }

    enemies_displayed += enemies_per_wave;
  }

  // gestionarea inamicilor
  for (int i = 0; i < enemies_per_wave; i++) {
    if (enemies[i].is_alive == true) {
      if (abs(enemies[i].x_position - spaceship.x_position) < 20 && abs(enemies[i].y_position - spaceship.y_position) < 20) {
        current_game_state = LOSING_STATE;
        tft.fillScreen(ILI9341_MAROON);
        tft.setTextSize(2);
        tft.setTextColor(ILI9341_YELLOW);
        tft.setCursor(20, 20);
        tft.print("You lost this game!! Try again!");
        
        for (int i = 0; i < 24; i++) {
          tone(buzzerPin, loseNotes[i], noteDuration);
          delay(noteDuration / 2);  // Slight delay between notes
        }
        player_lost = true;
        break;
      }
    }
  
    if (enemies[i].x_position < 0 - passing_limit) {
      enemies[i].is_alive == false;
      enemies_displayed -= 1;
      
      if (!enemies_displayed) {
        current_wave += 1;
      }
    }

    if (enemies[i].is_alive == true) {
      draw_enemy_spaceship(enemies[i], sky_color, 10);
      enemies[i].x_position -= 0.2f;
      draw_enemy_spaceship(enemies[i], ILI9341_RED, 10);
    }
  }

  if (player_lost == true) {
    return;
  }

  // miscarea caracterului principal
  float current_vert = myAnalogRead(JOY_VERT - A0);
  float current_horz = myAnalogRead(JOY_HORZ - A0);
  bool move = false;

  // nici o miscare detectata
  if (current_vert > 600 || current_vert < 400 || current_horz < 400 || current_horz > 600) {
    move = true;
    draw_spaceship(spaceship, sky_color);
  }

  if (current_vert > 600 && spaceship.y_position > 30) {
      spaceship.y_position -= 2;
  } else if (current_vert < 400 && spaceship.y_position < 150) {
      spaceship.y_position += 2;
  }

  if (current_horz > 600) {
      spaceship.x_position += 2;
  } else if (current_horz < 400 && spaceship.x_position > 2) {
      spaceship.x_position -= 2;
  }

  // gestionarea gloantelor
  for (uint8_t i = 0; i < pending_bullet; i++) {
    if (bullet_x_positions[i] < tft.width() + shooting_limit_tolerance && bullet_state[i] == 1) {
      for (int j = 0; j < enemies_per_wave; j++) {

        if (enemies[j].x_position < 0) {
          enemies[j].is_alive == false;
          enemies_displayed -= 1;
        }

        if (enemies[j].is_alive == true) {
          uint16_t x_bullet = bullet_x_positions[i];
          uint16_t y_bullet = bullet_y_positions[i];

          if (enemies[j].x_position - x_bullet < 20 && abs(enemies[j].y_position - y_bullet) < 20 && bullet_state[i] == 1) {
            enemies[j].is_alive = false;
            enemies_displayed -= 1;

            if (!enemies_displayed) {
              current_wave += 1;
            }

            bullet_state[i] = 0;

            // drawing the enemy and bullet so they are invisible
            draw_enemy_spaceship(enemies[j], sky_color, 10);
            player_score += 100;
            tft.setCursor(1, 5);
            tft.setTextSize(1);
            tft.print("Player score: " + String(player_score));
            tft.fillCircle(bullet_x_positions[i], bullet_y_positions[i], bullet_radius, sky_color);
            bullet_x_positions[i] = 10000;
            break;
          }
        }
      }

      tft.fillCircle(bullet_x_positions[i], bullet_y_positions[i], bullet_radius, sky_color);
      bullet_x_positions[i] += 15;
      tft.fillCircle(bullet_x_positions[i], bullet_y_positions[i], bullet_radius, ILI9341_BLACK);
    } else {
      bullet_state[i] = 0;
    }
  }

  // logica apelata la apasarea butonului
  if (digitalRead(JOY_BUTTON) == LOW && (my_millis() - last_shoot > shoot_delay)) {
    tone(buzzerPin, NOTE_C4, noteDuration / 3);
    uint8_t bullet_x = spaceship.x_position + 40;
    bullet_x_positions[pending_bullet] = bullet_x;
    bullet_y_positions[pending_bullet] = spaceship.y_position;
    pending_bullet += 1;

    last_shoot = my_millis();
  }

  // desenarea navei, numai dupa ce s-a observat o miscare a acesteia
  if (move) {
    draw_spaceship(spaceship, ILI9341_BLACK);
  }
}

void loop() {
  float vert = myAnalogRead(JOY_VERT - A0);
  float horz = myAnalogRead(JOY_HORZ - A0);

  if (current_game_state == MAIN_MENU_STATE) {
    // Gestionarea navigarii prin meniu
    if (my_millis() - lastMove > moveDelay) {
      if (vert < 400) {
        selectedItem++;
        if (selectedItem < 0) selectedItem = menuLength - 1;
        lastMove = my_millis();
        drawMenu();
      } else if (vert > 600) {
        selectedItem--;
        if (selectedItem >= menuLength) selectedItem = 0;
        lastMove = my_millis();
        drawMenu();
      }
    }
  }

  if (digitalRead(JOY_BUTTON) == LOW && my_millis() - winning_time > 500) {
    if (current_game_state == MAIN_MENU_STATE) {
      current_game_state = GAME_STATE;
      Spaceship player_spaceship = init_player(0, 50);
      tft.setCursor(1, 5);
      tft.setTextSize(1);
      tft.setTextColor(ILI9341_WHITE, sky_color);
      tft.print("Player score: " + String(player_score));

      memset(&final_boss, 0, sizeof(EnemySpaceship));
      while(1) {
        game_loop(player_spaceship);

        if (current_game_state == WINNING_STATE || current_game_state == LOSING_STATE) {
          break;
        }
      }
    } else if (current_game_state == WINNING_STATE || current_game_state == LOSING_STATE) {
      current_game_state = MAIN_MENU_STATE;
      winning_time = my_millis();
      tft.fillScreen(ILI9341_MAROON);
      tft.setTextSize(2);
      tft.setTextColor(ILI9341_BLACK);
      drawMenu();

      // resetarea statisticilor jucatorului
      finished_waves = false;
      memset(&final_boss, 0, sizeof(EnemySpaceship));
      current_wave = 0;
      pending_bullet = 0;
      enemies_displayed = 0;
      drawn_decoration_towers = false;
      player_lost = false;
      player_score = 0;
      boss_ascending = true;
    }
  }
}

void drawMenu() {
  for (int i = 0; i < menuLength; i++) {
    if (i == selectedItem) {
      tft.setTextColor(ILI9341_YELLOW);
    } else {
      tft.setTextColor(ILI9341_WHITE);
    }
    tft.setCursor(20, 30 + i * 30);
    tft.print(menuItems[i]);
  }
}

// functie de retinere a orei din cadrul timpului local
void setRtcFromCompileTime() {
  // informatia despre timpul local
  const char* time = __TIME__;

  char *p = strtok(time, ":");
  int hour = atoi(p);
  p = strtok(NULL, ":");
  int minute = atoi(p);
  p = strtok(NULL, ":");
  int seconds = atoi(p);

  rtc.setTime(hour, minute, seconds);
}