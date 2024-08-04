#include "arduino_secrets.h"

#include "config.h"
#include <MIDI.h>
#include "oled.h"
constexpr bool REC_BUTTON = false;
constexpr bool PLAY_BUTTON = true;

// Declarar el mutex
SemaphoreHandle_t xMutex;
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, midi2);

// DefiniciÃ³n de la enumeraciÃ³n para los modos del secuenciador
enum SequencerMode {
  REC_MODE,  // 0
  PLAY_MODE, // 1
  STOP_MODE  // 2
};
//GLOBALS
uint8_t  notes[MAX_NOTES] = {0};  //save the pitch separa con ceros los acordes
uint8_t count=0; //for display and others
uint8_t notes_played = 0;
uint8_t notes_press_num = 1;
uint8_t midi_channel = 1;
uint8_t vel_value = 100;
uint8_t sustain = 1200000;
bool is_pressed = false;
volatile uint8_t bpm = 120;
volatile uint8_t figure = QUARTER_NOTE;
volatile bool pressButton = REC_BUTTON; //false = recButton, true = playButton
volatile bool shared_val = false;
volatile unsigned long last_interrupt = 0; 
volatile bool encoderButton = false;
unsigned long note_duration = NOTE_DURATION(bpm,figure);
// Variable para almacenar el modo actual del secuenciador
SequencerMode currentMode = REC_MODE;
//INTERRUPT
void IRAM_ATTR play_press();
void IRAM_ATTR rec_press();
void IRAM_ATTR encoder();
void IRAM_ATTR encoder_push();



// Declarar la tarea segundo nucleo
void read_and_display(void *parameter);

//FUNCIONES
int play_notes(uint8_t notes[], uint8_t channel);
void play_sequence();
void stop_sequence();
void rec_sequence();
void rec_reset();
void add_silence();
void reset_MIDI();
void handleNoteOn(byte channel, byte pitch, byte velocity);
void handleNoteOff(byte channel, byte pitch, byte velocity);

void setup() {
  // Crear el mutex
  xMutex = xSemaphoreCreateMutex();
  // Crear la tarea para actualizar el display OLED en el segundo nÃºcleo
  xTaskCreatePinnedToCore(read_and_display, "Update Display and read values", 4096, NULL, 1, NULL, 0);

  pinMode (LED_BUILTIN, OUTPUT);
  pinMode(PLAY_SWT, INPUT);//pulsador play
  pinMode(REC_SWT, INPUT);//pulsador rec  
  pinMode(ENCODER_SWT, INPUT);
  pinMode(DT, INPUT);
  pinMode(CLK, INPUT);
  pinMode(GATE_PIN,INPUT);
  
  midi2.setHandleNoteOn(handleNoteOn);  // Put only the name of the function       
  midi2.setHandleNoteOff(handleNoteOff);// Do the same for NoteOffs  
  midi2.begin(MIDI_CHANNEL_OMNI);// Initiate MIDI communications, listen to all channels 
  midi2.turnThruOff();   
  attachInterrupt(digitalPinToInterrupt(PLAY_SWT),play_press,FALLING);
  attachInterrupt(digitalPinToInterrupt(REC_SWT),rec_press,FALLING);  
  attachInterrupt(digitalPinToInterrupt(ENCODER_SWT),encoder_push,FALLING); 
  attachInterrupt(digitalPinToInterrupt(CLK),encoder,FALLING);

  display_ini();
  //show hello
  show_intro();  
  rec_sequence();
}



void loop() {
  
  if (pressButton == REC_BUTTON){
    switch(currentMode){
      case REC_MODE:
      rec_reset();
      rec_sequence();
      break;

      case PLAY_MODE:
      rec_sequence();
      currentMode = REC_MODE;
      break;

      case STOP_MODE:
      rec_sequence();
      currentMode = REC_MODE;
      break;
    }      
  } else{//play pressed
      switch(currentMode){
        case REC_MODE:
        play_sequence();
        currentMode = PLAY_MODE;
        break;

        case PLAY_MODE:
        stop_sequence();
        currentMode = STOP_MODE;
        break;

        case STOP_MODE:
        play_sequence();
        currentMode = PLAY_MODE;
        break;

      }
      
    }

  delay(100); // AÃ±adir un pequeÃ±o retardo para evitar rebotes
}



//SUB-MODES
void play_sequence(){
  midi2.setInputChannel(MIDI_CHANNEL_OFF);
  show_play();
  play_notes(notes, midi_channel);      
  reset_MIDI(); 
}
void stop_sequence(){
  bool prev_val = shared_val;
  
  show_stop();   
  while(prev_val == shared_val){
    while (Serial.available()>0)  Serial.read();
  } 
}
void rec_sequence(){
  bool prev_val = shared_val;
  show_number(0);
  while (prev_val == shared_val){
    midi2.read(MIDI_CHANNEL_OMNI);
    if(encoderButton == true){  //agregar silencio
      add_silence();
      encoderButton = false;    
    }  
  } 
}
void rec_reset(){
  for (int i=0; i<MAX_NOTES; i++){
    notes[i] =  0;
  }
  notes_played = 0;
  count = 0;
  show_number(0);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(200);
  digitalWrite(LED_BUILTIN, LOW);
}

//  SAVE NOTES
void handleNoteOn(byte channel, byte pitch, byte velocity){  
  if (notes_played < (MAX_NOTES-1)){
    notes[notes_played] = pitch;
    notes_played++;

    if (is_pressed == false){
      count++;  
      show_number(count);
    }
  }
  else{
    void show_max_notes();
  }  
  is_pressed = true;
}

void handleNoteOff(byte channel, byte pitch, byte velocity){
  if (is_pressed == true && notes_played < MAX_NOTES){ 
    notes[notes_played] = 0;
    notes_played++;
  }
  if (count == 1) midi_channel = channel; //guarda el canal de la primera nota
  is_pressed = false;
}

//  PLAY NOTES  (reproduce en bucle la secuencia hasta que playButton == false)
int play_notes(uint8_t notes[], uint8_t channel){
  uint8_t repeat=0;
  unsigned long prev_time = 0, sustain_time = 0;
  bool prev_val = shared_val;  
  uint8_t i = 0, j = 0;
  while(prev_val == shared_val){
    
    //tiempo hasta la nota siguiente    
    if((micros() - prev_time) >= (600000)){//si cumple un periodo
      prev_time = micros();
       //delay(300);   
      //Encendido de las notas
      while(notes[i] != 0){
        midi2.sendNoteOn(notes[i],vel_value,channel);
        delayMicroseconds(1);//tiempo entre nota y nota, ARPEGIAR (50000)
        i++;
      }
      //delay(200);
      //SUSTAIN
      sustain_time = micros(); // Usamos una nueva variable para el tiempo de sustain
      while ((micros() - sustain_time) <= 300000){
        if (prev_val != shared_val){//si saliste del modo play
          while(notes[j] != 0){//apaga las notas
            midi2.sendNoteOff(notes[j],0,channel);
            delayMicroseconds(1);
            j++;
          }
          return 0;  
        }    
      }
      //apagado de las notas
      while(notes[j] != 0){
        midi2.sendNoteOff(notes[j],0,channel);
        delayMicroseconds(1);
        j++;
      }
      repeat++;
      if (repeat >= count){//si termina de tocar todas las notas
        i = 0;
        j = 0;
        repeat = 0;
      }

      else if (i < MAX_NOTES) {//pasar sobre el cero, avanzar siguiente grupo
        i++;
        j++;     
      }
    }
  }
  return 0;
}

void add_silence(){
  count++;    
  show_number(count);
  if (notes_played < (MAX_NOTES-1)){
    notes[notes_played]=0;
    notes_played++;
  }
  else{
    show_max_notes();
  }
}

void reset_MIDI(){
  while (Serial.available()>0)  Serial.read();
  midi2.begin(MIDI_CHANNEL_OMNI);//reinicia el midi para borrar buffer
  midi2.turnThruOff();  //hay que volverle a decir     
}

void gate_calcule(unsigned long blck){
  unsigned long m;
  m=(0.8*blck)/4095;
  gateDelay=m*analogRead(GATE_PIN) + blck*GATE_MIN;
}

//  INTERRUPCIONES:
void IRAM_ATTR play_press(){
  if(millis()-last_interrupt >300)
  {
    pressButton = PLAY_BUTTON;
    shared_val = !shared_val;
    
  }
  last_interrupt=millis();  
}
void IRAM_ATTR rec_press(){
  if(millis()-last_interrupt >300)
  {
    pressButton = REC_BUTTON;
    shared_val = !shared_val;
    
  }
  last_interrupt=millis();  
}

void IRAM_ATTR encoder(){
  
  if(millis()-last_interrupt >90)
  {
    if(digitalRead(DT)==HIGH && bpm<BPM_MAX){
      if (xSemaphoreTakeFromISR(xMutex, NULL)) {
        bpm++;
        xSemaphoreGiveFromISR(xMutex, NULL);
      }     
    }
    else if(bpm > BPM_MIN){
      if (xSemaphoreTakeFromISR(xMutex, NULL)) {
        bpm--;
        xSemaphoreGiveFromISR(xMutex, NULL);
      }  
    }  
  last_interrupt=millis();
  }
}
void IRAM_ATTR encoder_push(){
  if(pressButton==REC_BUTTON){  //modo rec
    if(millis()-last_interrupt >200)
    {
      encoderButton=true;  
      last_interrupt=millis();
    }
    
  }
  
  else { //modo play o stop
    if(millis()-last_interrupt >200)
    {
      if(figure < SIXTEENTH_NOTE){
        figure = figure*2;  
      }
      else{
        figure = WHOLE_NOTE;
      }
      last_interrupt=millis();
    }
  }
}

//SEGUNDO NUCLEO TASK
void read_and_display(void *parameter) {
  uint8_t currentBpm = bpm;
  uint8_t oldBpm = currentBpm;
  
  while (true) {
    // Esperar a que el semÃ¡foro sea liberado
    if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
      // Acceder a la variable global `bpm` de manera segura
      currentBpm = bpm;
      xSemaphoreGive(xMutex);
    }
    if (oldBpm != currentBpm){
      // Actualizar el display OLED con el valor de `bpm`
      show_bpm(currentBpm);
      oldBpm = currentBpm;
      note_duration = NOTE_DURATION(bpm,figure);
    }

    if (currentMode != REC_MODE){
      sustain = 
    }  

    // AÃ±adir un pequeÃ±o delay para evitar actualizaciones excesivas
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void sustain_calcule(unsigned long blck){
  unsigned long m;
  m=(0.8*blck)/4095;
  gateDelay=m*analogRead(GATE_PIN) + blck*GATE_MIN;
}

