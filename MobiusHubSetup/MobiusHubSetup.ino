// Bienvenido a Möbius, úsese este código en dispositivos ESP32. Reportese y documentese cualquier error presente.
// Z (Zona)
// Configurese PIN, Hora Local

#include <WiFi.h>
#include "time.h"

// Setup PIN Digital y LED
const int Z1 = 12;  // Configurese
const int Z2 = 13;  // Configurese
const int Z3 = 14;  // Configurese
const int ledAzul = 2;  // No Configurar o Modificar

// Setup Tiempo
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 0;

// Setup Control de LED
unsigned long previousMillis = 0;
const long interval = 1000; // Intervalo de 1 segundo para el parpadeo
int patternStep = 0;
bool errorState = false;
bool horaConfigurada = false;

// Estructura Horarios / Veasé Días y Horarios
struct HorarioRiego {
  int horaInicio;
  int minutoInicio;
  int horaFin;
  int minutoFin;
};

// Días de Riego L(1), X(3), V(5), D (0)
const int diasRiego[] = {1, 3, 5, 0};
const int numDiasRiego = 4;

// Horarios de Riego
HorarioRiego horarios[] = {
  {6, 30, 6, 45},   // Configurese
  {14, 40, 15, 0}  // Configurese
};
const int numHorarios = 2;

void setup() {
  Serial.begin(115200);
  
  // Output
  pinMode(Z1, OUTPUT);
  pinMode(Z2, OUTPUT);
  pinMode(Z3, OUTPUT);
  pinMode(ledAzul, OUTPUT);
  
  // Setup Bombas / Inicialmente OFF
  digitalWrite(Z1, LOW);
  digitalWrite(Z2, LOW);
  digitalWrite(Z3, LOW);
  digitalWrite(ledAzul, LOW);
  
  Serial.println("Iniciando configuración...");
  
  // Auto Setup Hora Local / No NTP Server / Configurese 
  configTime(gmtOffset_sec, daylightOffset_sec, NULL, NULL);
  
  // Forzar Setup de Horal Local / Incial
  setenv("TZ", "COT-5", 1);
  tzset();
  
  // Manual Setup Hora Local
  struct tm timeinfo;
  timeinfo.tm_year = 124;  // 1900 a 2026
  timeinfo.tm_mon = 0;     // MM
  timeinfo.tm_mday = 1;
  timeinfo.tm_hour = 12;
  timeinfo.tm_min = 0;
  timeinfo.tm_sec = 0;
  time_t t = mktime(&timeinfo);
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
  
  horaConfigurada = true;
  Serial.println("Setup de Hora Local Offline Éxitoso");
  
  // Muestreo de Hora Local
  mostrarHoraActual();
  
  delay(2000); // Evitación de Sobrecarga
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Control de LED / Activo y En Espera
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    controlLED();
  }
  
  // Control de LED / Error
  if (errorState) {
    return;
  }
  
  // Error de Configuración / Detección / Muestreo en Control de LED
  if (!horaConfigurada) {
    errorState = true;
    return;
  }
  
  struct tm timeinfo;
  
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error al obtener la hora local");
    errorState = true;
    delay(1000);
    return;
  }
  
  // Verificación de Horario
  if (esDiaDeRiego(timeinfo.tm_wday)) {
    // Verificación
    bool enHorarioRiego = false;
    
    for (int i = 0; i < numHorarios; i++) {
      if (estaEnHorario(timeinfo, horarios[i])) {
        enHorarioRiego = true;
        break;
      }
    }
    
    // Control de Bombas
    if (enHorarioRiego) {
      activarBombas();
      Serial.println("Bombas ACTIVADAS - En horario de riego");
    } else {
      desactivarBombas();
      Serial.println("Bombas DESACTIVADAS - Fuera de horario");
    }
  } else {
    desactivarBombas();
    Serial.println("Bombas DESACTIVADAS - No es día de riego");
  }
  
  delay(100); // Evitación de Sobrecarga
}

void controlLED() {
  // Control de Errores / Verificación
  if (errorState) {
    // Setup de Comportamiento de LED
    digitalWrite(ledAzul, HIGH);
    return;
  }
  
  // Verificación de Hora Local
  if (!horaConfigurada) {
    digitalWrite(ledAzul, HIGH); // Error
    return;
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    errorState = true;
    return;
  }
  
  bool enHorarioRiego = false;
  bool esDiaValido = esDiaDeRiego(timeinfo.tm_wday);
  
  if (esDiaValido) {
    for (int i = 0; i < numHorarios; i++) {
      if (estaEnHorario(timeinfo, horarios[i])) {
        enHorarioRiego = true;
        break;
      }
    }
  }
  
  // Estado LED
  if (enHorarioRiego) {
    // Patrón En Trabajo
    patronRiego();
  } else if (esDiaValido) {
    // Patrón Activo y En Espera
    patronEspera();
  } else {
    // Patrón Fuera de Trabajo
    digitalWrite(ledAzul, LOW);
    patternStep = 0; // Reset de Estado
  }
}

void patronRiego() {
  // Patrón 111000111 / Ciclo 9s
  int ciclo = patternStep % 9;
  
  if (ciclo < 3 || ciclo >= 6) {
    digitalWrite(ledAzul, HIGH); // Encendido
  } else {
    digitalWrite(ledAzul, LOW); // Apagado
  }
  
  patternStep++;
  if (patternStep >= 900) { // Evitar Overflow
    patternStep = 0;
  }
}

void patronEspera() {
  // Patrón 101010 (1s)
  if (patternStep % 2 == 0) {
    digitalWrite(ledAzul, HIGH);
  } else {
    digitalWrite(ledAzul, LOW);
  }
  
  patternStep++;
  if (patternStep >= 600) { // Evitar Overflow
    patternStep = 0;
  }
}

bool esDiaDeRiego(int diaSemana) {
  for (int i = 0; i < numDiasRiego; i++) {
    if (diasRiego[i] == diaSemana) {
      return true;
    }
  }
  return false;
}

bool estaEnHorario(struct tm timeinfo, HorarioRiego horario) {
  int horaActual = timeinfo.tm_hour;
  int minutoActual = timeinfo.tm_min;
  int segundoActual = timeinfo.tm_sec;
  
  // Conersión Horaria a M
  int inicioMinutos = horario.horaInicio * 60 + horario.minutoInicio;
  int finMinutos = horario.horaFin * 60 + horario.minutoFin;
  int actualMinutos = horaActual * 60 + minutoActual;
  
  // (Excluyendo el Minuto Final) / Solo considerar dentro del rango
  return (actualMinutos >= inicioMinutos && actualMinutos < finMinutos);
}

void activarBombas() {
  digitalWrite(Z1, HIGH);
  digitalWrite(Z2, HIGH);
  digitalWrite(Z3, HIGH);
}

void desactivarBombas() {
  digitalWrite(Z1, LOW);
  digitalWrite(Z2, LOW);
  digitalWrite(Z3, LOW);
}

void mostrarHoraActual() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error de Obtención de Hora");
    return;
  }
  
  char buffer[50];
  strftime(buffer, sizeof(buffer), "Hora Actual: %A %Y-%m-%d %H:%M:%S", &timeinfo);
  Serial.println(buffer);
  
  Serial.print("Día de Semana (número): ");
  Serial.println(timeinfo.tm_wday);
  
  // Depuración y Testeo
  Serial.print("Día de Riego ");
  Serial.println(esDiaDeRiego(timeinfo.tm_wday) ? "Si" : "No");
}
