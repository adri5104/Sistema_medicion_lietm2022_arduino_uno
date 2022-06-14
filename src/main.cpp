/*
Codigo para los sistemas de medicion de LIETM 2022
Author: Adrian Rieker
*/

#include <Adafruit_VL53L0X.h>
#include <Arduino.h>

// Tipo enum para la variable de estado
enum state {
  S_WAITING,
  S_RUNNING,
  S_RUNING_LD,
  S_RUNNING_FINISHED,
  S_CALIBRATING,
  S_CALIBRATING_FINISHED,
  S_DEBUG,
  S_DEBUG_1
};

// ##################### Variables globales ##############

// Variable de estado
volatile state State = S_WAITING;

// Objeto global para el sensor tof
Adafruit_VL53L0X d_sensor = Adafruit_VL53L0X();

// Diferencia que debe haber con la distancia trigger para detectar pendulo
const uint16_t DISTANCE_THRESHOLD = 20;

// Distancia actual en cada momento
uint16_t actualDistance = 0;

// Distancia de disparo
uint16_t trigger_distance = 200;

// Tiempo anterior
volatile unsigned long int last_time = millis();

volatile unsigned long int semiperiod = millis();

// Numero de pasos por cero
#define NUM_PASOS_CERO 20

// Iterador para el array de datos
uint16_t it = 0;

// Array de datos para la medicion
static uint16_t measurements[NUM_PASOS_CERO];

uint8_t det = 0;

// ##################### Definicion de funciones ##############

// Funcion para imprimir por serial el tutorial/ pantalla inicio
void print_tutorial()
{
  Serial.println("#########################################\n");
  Serial.print("Pulsa 'r' comenzar la medicion \n");
  Serial.print("Pulsa 'c' para empezar calibracion\n");
  Serial.print("Pulsa 'f' o vuelve a pulsar 'c' para terminar calibracion\n");
  Serial.print("Pulsa 'd' para empezar debug\n\n");

  Serial.print("trigger_distance: ");
  Serial.println(trigger_distance);

  Serial.println("\n#########################################\n");
}



// Funcion para el manejo de la comunicacion serial
void serial_things()
{
  if(Serial.available() > 0)
  {
    static char op = ' ';
    op = Serial.read();
    switch(op)
    {
      // Para hacer debug
      case 'd' :
      case 'D' :
        Serial.println("Debug mode");
        det = 0;
        if(State == S_DEBUG)
        {
          State = S_DEBUG_1;
          
        }
        else
        {
          State = S_DEBUG;
        }
        break;
      
      // Calibrando distancia trigger
      case 'c':
      case 'C':
        if(State == S_CALIBRATING)
        {
          State = S_CALIBRATING_FINISHED;
        }
        else
        {
          State = S_CALIBRATING;
        }
      break;

      case 'f':
        State = S_CALIBRATING_FINISHED;
        break;

      case 'r':
        State = S_RUNNING;
        for(it = 0; it < NUM_PASOS_CERO; it++)
        {
          measurements[it] = 0;
        }
        it = 0;
        last_time = millis();
        Serial.println("Midiendo...");
        Serial.end();
        break;

      case 'a':
        if(State == S_RUNNING)
        {
          State = S_RUNNING_FINISHED;
        }
        break;

      case 'x':
        Serial.println("Shutting down...");    
        print_tutorial();
        State = S_WAITING;
        it = 0;
        break;

      

      default:
        op = '0';
        break;
        
    }
  }

}

// Funcion para la deteccion del pendulo
bool pendulum_detected_trigger(uint16_t distance,uint16_t trigger)
{
  static bool flag = false;
  if(distance < trigger)
  {
    if(flag == false)
    {
      flag = true;
      return true;
    }
  }
  else
  {
    flag = false;
  }
  return false;
}





void setup() {

  // Inicializamos serial
  Serial.begin(9600);

  while (! Serial) {
    delay(1);
  }

  // Inicializamos el sensor
  Serial.print("Booting distance sensor...");
  if (!d_sensor.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while(1);
  }
  Serial.println(F("Done\n"));
  
  // Imprimimos el tutorial
  print_tutorial();

  // Activamos modo de medicion continua del sensor
  d_sensor.configSensor(Adafruit_VL53L0X::VL53L0X_SENSE_HIGH_SPEED);
  d_sensor.startRangeContinuous();
}

void loop() {

// cosas del serial
serial_things();

// Tomamos la distancia
if (d_sensor.isRangeComplete()) 
{
    actualDistance = d_sensor.readRange();
}


// Si el estado es calibrando, se va tomando la distancia 
// para actualizar el valor de la distancia trigger
// hasta que se salga del modo de calibracion
if(State == S_CALIBRATING)
{
  trigger_distance = actualDistance + DISTANCE_THRESHOLD;
  Serial.println(actualDistance);
}

// Cuando se termina la calibracion
if(State == S_CALIBRATING_FINISHED)
{
  State = S_WAITING;
  Serial.println("Calibracion finalizada");
  delay(1000);
  print_tutorial();
}

// Estado de toma de mediciones
if(State == S_RUNNING)
{
  // Comprobamos si hay pendulo
  bool pendulo = pendulum_detected_trigger(actualDistance,trigger_distance);

  // Si hay pendulo
  if(pendulo)
  {
    // Guardamos semiperiodo en millis(gundos y actualizamos el tiempo anterior
    semiperiod = millis() - last_time;
    last_time = millis();

    // Guardamos el semiperiodo en el array de datos e incrementamos el iterador
    measurements[it] = (uint16_t) semiperiod;
    
    it++;
  }

  // Array lleno, se ha terminado la medicion
  if(it == NUM_PASOS_CERO)
  {
    Serial.begin(9600);
    State = S_RUNNING_FINISHED;
  }
}

// Cuando se termina la medicion
if(State == S_RUNNING_FINISHED)
{
  // Median de los datos
  double mean = 0;

  // Desviacion de la media 
  double deviation = 0;

  // iterador
  uint8_t i = 0;
  
  // Calculamos la media (se pasa a ms)
  for(i = 0; i < it ; i++)
  {
    mean += (double ) measurements[i];
  }
  mean = mean / (it * 1000);
  
  // Calculamos la desviacion tipica
  for(i = 0; i < it ; i++)
  {
    deviation +=  (((double )measurements[i]/ 1000) - mean) * (((double )measurements[i]/1000) - mean);
  }
  double deviation_tipica = sqrt(deviation / (it ));

  Serial.println("#########################################\n");
  Serial.print("Mediciones tomadas:\n\n") ;
  for(i = 0; i < it ; i++)
  {
    Serial.print(i+1);
    Serial.print(": ");
    Serial.print((double) measurements[i]/ 1000); Serial.print(" ms\n");
  }
  Serial.print("\n\n  Media: "); Serial.print(mean,8) ; Serial.println(" s");
  Serial.print(" Desviacion tipica: "); Serial.print(deviation_tipica,8); Serial.println(" s"); 
  Serial.print(" Varianza: "); Serial.print(deviation,8); Serial.println(" s"); 
  Serial.println("\n#########################################\n");

  it = 0;
  State = S_WAITING;
  delay(1000);
  print_tutorial();

}

if(State == S_DEBUG)
{
  if(pendulum_detected_trigger(actualDistance,trigger_distance))
  {
    det++;
  }
  
  Serial.print(actualDistance);
  Serial.print("\t");
  Serial.print(millis() - last_time);
  Serial.print("\t");
  Serial.print(det);
  Serial.print("\n");

  last_time = millis(); 
  
}

if(State == S_DEBUG_1)
{
  bool pendulo = pendulum_detected_trigger(actualDistance, trigger_distance);
  if(pendulo)
  {
    Serial.println("Deteccion");
  }
}




}