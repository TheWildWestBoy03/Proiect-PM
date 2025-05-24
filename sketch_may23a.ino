#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// TFT setup
#define TFT_CS     10
#define TFT_RST    -1
#define TFT_DC     9
#define MAIN_MENU_STATE 0
#define GAME_STATE 1
#define WINNING_STATE 2
#define EXITING_STATE 3

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

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
  uint16_t y_position;
  uint16_t dimension;
  uint16_t x_limit; // this parameter is available only for the boss
};

struct Coordinates {
    float x;
    float y;
};

// Pin connected to the buzzer
int buzzerPin = 8;

// Notes (frequencies in Hz)
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
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5,
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5,
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5,
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5,
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5,
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5,
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5,
  NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4, NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5
};

// Menu variables
const char* menuItems[] = {"Start Game", "Options", "Credits", "Exit"};
const int menuLength = sizeof(menuItems) / sizeof(menuItems[0]);
int selectedItem = 0;
int current_game_state = 0;
unsigned long lastMove = 0;
bool in_playing_mode = true;

// const variables
const uint8_t maximum_decoration_towers = 15;
const uint8_t maximum_bullets_on_screen = 10;
const uint8_t maximum_waves = 2;
const uint8_t maximum_enemies = 4;

// enemy information
uint8_t enemies_displayed = 0;
const uint8_t enemies_per_wave = 2;
EnemySpaceship enemies[maximum_enemies];
uint8_t current_wave = 0;

// towers generic information
bool drawn_decoration_towers = false;
uint16_t tower_x_position[maximum_decoration_towers];
uint16_t tower_y_position[maximum_decoration_towers] = {200, 170, 200, 210, 160, 190, 200, 190, 180, 140, 165, 185, 195, 200, 210};
uint8_t tower_length[maximum_decoration_towers];
uint8_t tower_height[maximum_decoration_towers] = {50, 80, 50, 40, 90, 60, 50, 60, 70, 110, 85, 65, 55, 50, 40};

// delay in ms used in debouncing implementation for joystick btn
int moveDelay = 500;
uint16_t bullet_positions[maximum_bullets_on_screen];
uint16_t bullet_y_positions[maximum_bullets_on_screen];

uint8_t bullet_state[maximum_bullets_on_screen];
uint16_t pending_bullet = 0;
const uint8_t bullet_radius = 2;
unsigned long last_shoot = 0;
const long shoot_delay = 2000;
const uint8_t shooting_limit_tolerance = 10;

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

void draw_enemy_spaceship(EnemySpaceship enemy, uint16_t color) {
  tft.setCursor(enemy.x_position, enemy.y_position);

  Coordinates left_dot;
  left_dot.x = enemy.x_position;
  left_dot.y = enemy.y_position; 

  Coordinates right_down_dot;
  right_down_dot.x = enemy.x_position + 10;
  right_down_dot.y = enemy.y_position + 10;

  Coordinates right_up_dot;
  right_up_dot.x = enemy.x_position + 10;
  right_up_dot.y = enemy.y_position - 10;

  tft.fillTriangle(left_dot.x, left_dot.y, 
                    right_up_dot.x, right_up_dot.y,
                    right_down_dot.x, right_down_dot.y, color);
}

Spaceship init_player(float x, float y) {
    tft.fillScreen(ILI9341_MAROON);
    Spaceship new_spaceship;
    new_spaceship.x_position = x;
    new_spaceship.y_position = y;
    new_spaceship.damage = 10;
    new_spaceship.health = 3;
    new_spaceship.dimension = 10;

    draw_spaceship(new_spaceship, ILI9341_BLACK);
    return new_spaceship;
}

void setup_tower_information() {
  for (uint8_t i = 0; i < maximum_decoration_towers; i++) {
    tower_x_position[i] = i * 22;
    tower_length[i] = 21;
  }
}

void setup_bullet_statements() {
  for (uint8_t i = 0; i < maximum_bullets_on_screen; i++) {
    bullet_state[i] = 1;
  }
}
void setup() {
  Serial.begin(9600);

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9341_MAROON);
  tft.setTextSize(2);
  tft.setTextColor(ILI9341_BLACK);
  
  pinMode(JOY_BUTTON, INPUT_PULLUP); // Joystick button
  drawMenu();

  setup_tower_information();
  setup_bullet_statements();

  current_game_state = MAIN_MENU_STATE;
  
  // logic for welcoming song
  // for (int i = 0; i < 8; i++) {
  //   tone(buzzerPin, melody[i], noteDuration);
  //   delay(noteDuration / 5);  // Slight delay between notes
  // }
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

    for (uint8_t thickness = 0; thickness < 3; thickness++) {
      tft.drawRect(tower_x_position[tower] + thickness, tower_y_position[tower] + thickness, tower_length[tower] - 2 * thickness, 
                                          tower_height[tower] - 2 * thickness, ILI9341_BLACK);
    }
  }
}

void game_loop(Spaceship &spaceship) {
  if (current_wave >= maximum_waves) {
    current_game_state = WINNING_STATE;
    tft.fillScreen(ILI9341_MAROON);
    tft.setTextSize(4);
    tft.setTextColor(ILI9341_YELLOW);

    tft.print("Congratulations!!! You won the game!!");

    return;
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

      draw_enemy_spaceship(enemies[i], ILI9341_RED);
    }

    current_wave += 1;
    enemies_displayed += enemies_per_wave;
  }

  //handling enemies if any
  for (int i = 0; i < enemies_per_wave; i++) {
    if (enemies[i].x_position < 0) {
      enemies[i].is_alive == false;
      enemies_displayed -= 1;
    }

    if (enemies[i].is_alive == true) {
      draw_enemy_spaceship(enemies[i], ILI9341_MAROON);
      enemies[i].x_position -= 0.2;
      draw_enemy_spaceship(enemies[i], ILI9341_RED);
    }
  }

  float current_vert = analogRead(JOY_VERT);
  float current_horz = analogRead(JOY_HORZ);
  bool move = false;

  if (current_vert > 600 || current_vert < 400 || current_horz < 400 || current_horz > 600) {
    move = true;
    draw_spaceship(spaceship, ILI9341_MAROON);
  }

  if (current_vert > 600 && spaceship.y_position > 2) {
      spaceship.y_position -= 2;
  } else if (current_vert < 400 && spaceship.y_position < 100) {
      spaceship.y_position += 2;
  }

  if (current_horz > 600) {
      spaceship.x_position += 2;
  } else if (current_horz < 400 && spaceship.x_position > 2) {
      spaceship.x_position -= 2;
  }

  // displaying bullets if any
  for (uint8_t i = 0; i < pending_bullet; i++) {
    if (bullet_positions[i] < tft.width() + shooting_limit_tolerance && bullet_state[i] == 1) {
      for (int j = 0; j < enemies_per_wave; j++) {

        if (enemies[j].x_position < 0) {
          enemies[j].is_alive == false;
          enemies_displayed -= 1;
        }

        if (enemies[j].is_alive == true) {
          uint16_t x_bullet = bullet_positions[i];
          uint16_t y_bullet = bullet_y_positions[i];

          if (enemies[j].x_position - x_bullet < 7 && enemies[j].y_position - y_bullet < 7 && bullet_state[i] == 1) {
            Serial.println("enemy killed");
            enemies[j].is_alive = 0;
            enemies_displayed -= 1;
            bullet_state[i] = 0;

            // drawing the enemy and bullet so they are invisible
            draw_enemy_spaceship(enemies[j], ILI9341_MAROON);
            tft.fillCircle(bullet_positions[i], bullet_y_positions[i], bullet_radius, ILI9341_MAROON);
            bullet_positions[i] = 10000;

            break;
          }
        }
      }

      tft.fillCircle(bullet_positions[i], bullet_y_positions[i], bullet_radius, ILI9341_MAROON);
      bullet_positions[i] += 1;
      tft.fillCircle(bullet_positions[i], bullet_y_positions[i], bullet_radius, ILI9341_BLACK);
    }
  }

  if (digitalRead(JOY_BUTTON) == LOW && (millis() - last_shoot > shoot_delay)) {
    uint8_t bullet_x = spaceship.x_position + 40;
    bullet_positions[pending_bullet] = bullet_x;
    bullet_y_positions[pending_bullet] = spaceship.y_position;
    pending_bullet += 1;

    last_shoot = millis();
  }

  // draw the spaceship with the new coordinates
  if (move) {
    draw_spaceship(spaceship, ILI9341_BLACK);
  }
}

void loop() {
  float vert = analogRead(JOY_VERT);
  float horz = analogRead(JOY_HORZ);

  if (in_playing_mode = true) {
    in_playing_mode = false;
  }

  if (current_game_state == MAIN_MENU_STATE) {
    // Handle up/down navigation with basic debouncing
    if (millis() - lastMove > moveDelay) {
      if (vert < 400) { // Down
        selectedItem++;
        if (selectedItem < 0) selectedItem = menuLength - 1;
        lastMove = millis();
        drawMenu();
      } else if (vert > 600) { // Up
        selectedItem--;
        if (selectedItem >= menuLength) selectedItem = 0;
        lastMove = millis();
        drawMenu();
      }
    }
  }

//   Optional: handle button press to "select" item
  if (digitalRead(JOY_BUTTON) == LOW) {
    if (current_game_state == WINNING_STATE) {
      current_game_state = MAIN_MENU_STATE;
    } else if (current_game_state == MAIN_MENU_STATE && in_playing_mode == false) {
      in_playing_mode = true;
      current_game_state = GAME_STATE;
      Spaceship player_spaceship = init_player(0, 50);
      while(1) {
        game_loop(player_spaceship);

        if (current_game_state == WINNING_STATE) {
          break;
        }
      }
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
