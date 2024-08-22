#include "config.h"
#include <MIDI.h>
#include "oled.h"
constexpr bool REC_BUTTON = false;
constexpr bool PLAY_BUTTON = true;

// Declarar el mutex
SemaphoreHandle_t xMutex;
MIDI_CREATE_INSTANCE(HardwareSerial, Serial, midi2);

// Definición de la enumeración para los modos del secuenciador
enum SequencerMode {
  REC_MODE,  // 0
  PLAY_MODE, // 1
  STOP_MODE  // 2
};
//GLOBALS
uint8_t  notes[MAX_NOTES] = {0};  //save the pitch separa con ceros los acordes
uint8_t count=0; //for display and others
uint8_t play_counter=0; //for display numbers in play mode
uint8_t notes_played = 0;
uint8_t notes_press_num = 1;
uint8_t midi_channel = 1;
uint8_t vel_value = 100;
unsigned long sustain = 1200000;
bool is_pressed = false;
volatile uint8_t bpm = 120;
volatile uint8_t figure = QUARTER_NOTE;
volatile bool pressButton = REC_BUTTON; //false = recButton, true = playButton
volatile bool shared_val = false;
volatile bool encoder_change = false;
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
int play_notes1(uint8_t notes[], uint8_t channel);
void play_notes(uint8_t *current_note);
void send_notes(uint8_t  **current_note, uint8_t velocity, uint8_t channel, bool prev_val);
void play_sequence();
void stop_sequence();
void rec_sequence();
void rec_reset(bool reset_all);
void add_silence();
void reset_MIDI();
void handleNoteOn(byte channel, byte pitch, byte velocity);
void handleNoteOff(byte channel, byte pitch, byte velocity);
unsigned long sustain_calcule(unsigned long blck);
uint8_t velocity_calcule();

void setup() {
  // Crear el mutex
  xMutex = xSemaphoreCreateMutex();
  // Crear la tarea para actualizar el display OLED en el segundo núcleo
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
      rec_reset(true);
      rec_sequence();
      break;

      case PLAY_MODE:
      currentMode = REC_MODE;
      rec_reset(false);
      rec_sequence();
      break;

      case STOP_MODE:
      currentMode = REC_MODE;
      rec_reset(false);
      rec_sequence();
      break;
    }      
  } else{//play pressed
      switch(currentMode){
        case REC_MODE:
        currentMode = PLAY_MODE;
        play_sequence();
        break;

        case PLAY_MODE:
        currentMode = STOP_MODE;
        stop_sequence();
        break;

        case STOP_MODE:
        currentMode = PLAY_MODE;
        play_sequence();
        break;

      }
      
    }

  delay(100); // Añadir un pequeño retardo para evitar rebotes
}

//SUB-MODES
void play_sequence(){
  midi2.setInputChannel(MIDI_CHANNEL_OFF);
  //show_play();
  play_notes(notes);      
  reset_MIDI(); 
}
void stop_sequence(){
  bool prev_val = shared_val;
  
  //show_stop();   
  while(prev_val == shared_val){
    while (Serial.available()>0)  Serial.read();
  } 
}
void rec_sequence(){
  bool prev_val = shared_val;
  show_number(0, true);
  while (prev_val == shared_val){
    midi2.read(MIDI_CHANNEL_OMNI);
    if(encoderButton == true){  //agregar silencio
      add_silence();
      encoderButton = false;    
    }  
  } 
}
void rec_reset(bool reset_all){
  if (reset_all){
    for (int i=0; i<MAX_NOTES; i++){
    notes[i] =  0;
    }
    show_number(0,true);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
  }
  
  notes_played = 0;
  count = 0;
  
}

//  SAVE NOTES
void handleNoteOn(byte channel, byte pitch, byte velocity){  
  if (notes_played < (MAX_NOTES-1)){
    notes[notes_played] = pitch;
    notes_played++;

    if (is_pressed == false){
      count++;  
      show_number(count, true);
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
int play_notes1(uint8_t notes[], uint8_t channel){
  
  uint8_t velocity;
  unsigned long prev_time = 0, sustain_start = 0,sustain_time, note_time;
  bool prev_val = shared_val;  
  uint8_t i = 0, j = 0;
  //Asignacion incial de valores
  if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
      note_time = note_duration;
      sustain_time = sustain;
      velocity = vel_value;
      xSemaphoreGive(xMutex);
    }
  
  while(prev_val == shared_val){
    //tiempo hasta la nota siguiente    
    if((micros() - prev_time) >= note_time){//si cumple un periodo
      prev_time = micros();
       //delay(300);   
      //Encendido de las notas
      while(notes[i] != 0){
        midi2.sendNoteOn(notes[i],vel_value,channel);
        delayMicroseconds(1);//tiempo entre nota y nota, ARPEGIAR (50000)
        i++;
      }
      //SUSTAIN
      sustain_start = micros(); // Usamos una nueva variable para el tiempo de sustain
      //actualizo valores de sustain
      if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
        sustain_time = sustain ;
        xSemaphoreGive(xMutex);
      }
      while ((micros() - sustain_start) <= sustain_time){       
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
      if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
        note_time = note_duration;
        velocity = vel_value;
        play_counter++;
        xSemaphoreGive(xMutex);
      }
      
  if (play_counter >= count){//si termina de tocar todas las notas
    i = 0;
    j = 0;
    play_counter = 0;
  }

      else if (i < MAX_NOTES) {//pasar sobre el cero, avanzar siguiente grupo
        i++;
        j++;     
      }
      
    }
  }
  return 0;
}
void play_notes(uint8_t *current_note){
  uint8_t velocity;
  unsigned long prev_time = 0, note_time;
  bool prev_val = shared_val;  
  
  //Asignacion incial de valores
  if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
      note_time = note_duration;
      velocity = vel_value;
      play_counter = 0;
      xSemaphoreGive(xMutex);
    }
  
  while(prev_val == shared_val){
    //tiempo hasta la nota siguiente    
    if((micros() - prev_time) >= note_time){//si cumple un periodo
      prev_time = micros();

      //Encendido de las notas  
      send_notes(&current_note, velocity, midi_channel, prev_val);

      //actualizo variables
      if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
      note_time = note_duration;
      velocity = vel_value;
      play_counter++;
      current_note++; //warning
      if (play_counter >= count){//si termina de tocar todas las notas   
        play_counter = 0;
        current_note = notes; //reinicia el puntero
      }
      xSemaphoreGive(xMutex);
    }
    }  
  }
}
void send_notes(uint8_t  **current_note, uint8_t velocity, uint8_t channel, bool prev_val){
  unsigned long sustain_start = 0, sustain_time;
  uint8_t *start_note = *current_note;
  while(**current_note != 0){
    midi2.sendNoteOn(**current_note,velocity,channel);
    delayMicroseconds(1);//tiempo entre nota y nota, ARPEGIAR (50000)
    (*current_note)++;
  } 
  //SUSTAIN
  sustain_start = micros(); // Usamos una nueva variable para el tiempo de sustain
  //actualizo valores de sustain
  if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
    sustain_time = sustain ;
    
    xSemaphoreGive(xMutex);
  }
  while ((micros() - sustain_start) <= sustain_time){       
    if (prev_val != shared_val){//si saliste del modo play
      while(*start_note != 0){//apaga las notas
        midi2.sendNoteOff(*start_note,0,channel);
          //delayMicroseconds(1);
          start_note++;
      } 
    }    
  }
  //apagado de las notas
  while(*start_note != 0){//apaga las notas
    midi2.sendNoteOff(*start_note,0,channel);
    //delayMicroseconds(1);
    start_note++;
  }     

}

void add_silence(){
  count++;    
  show_number(count, true);
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

void IRAM_ATTR encoder() {
  if (millis() - last_interrupt > 90) {
    if (xSemaphoreTakeFromISR(xMutex, NULL)) {
      if (digitalRead(DT) == HIGH && bpm < BPM_MAX) {
        bpm++;
      } else if (bpm > BPM_MIN) {
        bpm--;
      }
      encoder_change = true;
      xSemaphoreGiveFromISR(xMutex, NULL);
    }
    last_interrupt = millis();
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
    encoder_change = true;
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
  //Variables para actualizar el valor solo cuando cambia
  uint8_t currentBpm = bpm;
  uint8_t currentVel = vel_value, oldVel = currentVel;
  uint8_t oldCounter = 0;
  
  
  while (true) {
    // Esperar a que el semáforo sea liberado
    if (encoder_change) {
      update_clock(&currentBpm);
    }

    if (currentMode != REC_MODE){

      if(currentMode == PLAY_MODE){
        if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
          if (oldCounter != play_counter){
            show_number(play_counter+1, false);
            oldCounter = play_counter;
          }          
        xSemaphoreGive(xMutex);
        }
      } 
      else{
        show_stop();
      }
      //VELOCITY
      currentVel = velocity_calcule();
      if (currentVel != oldVel){
      if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
          vel_value = currentVel;       
          xSemaphoreGive(xMutex);
        }
        oldVel = currentVel;
      }
        
      if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
        sustain = sustain_calcule(note_duration);        
        xSemaphoreGive(xMutex);
      }     
    }  

    // Añadir un pequeño delay para evitar actualizaciones excesivas
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
///////////////

void play_sequence_2core(){

}

void update_clock(uint8_t *current_bpm){
  if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
    *current_bpm = bpm;
    note_duration = NOTE_DURATION(*current_bpm, figure);
    encoder_change = false;
    xSemaphoreGive(xMutex);
  }

  show_bpm(*current_bpm);
}
/*
void update_velocity_sustain(){
  //VELOCITY
  currentVel = velocity_calcule();
  if (currentVel != oldVel){
   if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
      vel_value = currentVel;       
      xSemaphoreGive(xMutex);
    }
    oldVel = currentVel;
  }
    
  if (xSemaphoreTake(xMutex, portMAX_DELAY)) {
    sustain = sustain_calcule(note_duration);        
    xSemaphoreGive(xMutex);
  }
}
*/
unsigned long sustain_calcule(unsigned long blck){
  unsigned long m;
  m=(0.8*blck)/4095;//0.8 -> 80%
  return (m*analogRead(GATE_PIN) + blck*GATE_MIN);
}

uint8_t velocity_calcule(){
  int vel = analogRead(VELOCITY_PIN);
  return map(vel, 0, 4096, 10, 127);
}