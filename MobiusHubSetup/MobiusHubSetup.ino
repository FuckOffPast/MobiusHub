// Bienvenido a Möbius, úsese este código en dispositivos ESP-32
// Programa y Testeado solo para ESP-32
// Licencia Apache v2.0
// Reportese y documentese cualquier error presente
// Z (Zona) / Motobombas
// R (Relé) / Espejo Z
// ! (Válvula) / Espejo Z
// Configurese PIN, Hora Local, WiFI SSID, Contraseña, Horarios
// WiFi Integrado (Deshabilitado Default)

// Es posible que usted tenga que actualizar su dispositivo a la versión más reciente para utilizar algunas características
// Después un apagón o desactivación de dispositivos, asegúrese de configurar nuevamente el Tiempo Local

#include <WiFi.h>
#include "time.h"

// TODO: Motor de Autoguardado de Estados

// Configuración WiFi
const char* ssid = "Möbius Hub";
const char* password = "Möbius@User/Hub";

// Setup PIN Digital y LED / Motobombas
const int Z1 = 12;     // Configurese
const int Z2 = 13;     // Configurese
const int Z3 = 14;     // Configurese
const int ledAzul = 2; // No Configurar o Modificar

// PIN Espejo (!1, !2, !3) / Válvulas
const int Z1_mirror = 15; // Configurese
const int Z2_mirror = 16; // Configurese
const int Z3_mirror = 17; // Configurese

// PIN Relés (R1, R2, R3) / Relés
const int R1 = 18; // Configurese
const int R2 = 19; // Configurese
const int R3 = 21; // Configurese

// Setup Tiempo
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 0;

// Setup Control de LED
unsigned long previousMillis = 0;
const long interval = 1000; // Intervalo 1s
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

// TODO: Integración de Motor de Autoguardado de Estados
// Días de Riego L(1), X(3), V(5), D(0)
const int diasRiego[] = {1, 3, 5, 0};
const int numDiasRiego = 4;

// Horarios de Riego
HorarioRiego horarios[] = {
  {6, 30, 6, 35},  // Configurese
  {13, 50, 13, 55}  // Configurese
};
const int numHorarios = 2;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Iniciando Configuración...");

  // Configuración de PIN
  pinMode(Z1, OUTPUT);
  pinMode(Z2, OUTPUT);
  pinMode(Z3, OUTPUT);
  pinMode(Z1_mirror, OUTPUT);
  pinMode(Z2_mirror, OUTPUT);
  pinMode(Z3_mirror, OUTPUT);
  pinMode(R1, OUTPUT);
  pinMode(R2, OUTPUT);
  pinMode(R3, OUTPUT);
  pinMode(ledAzul, OUTPUT);

  // Inicialización / Apagado en Inicialización
  desactivarBombas();
  digitalWrite(ledAzul, LOW);

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi ");
  Serial.println(ssid);

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 15000; // 15s Espera para Denegación

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n WiFi Conectado");
    Serial.print("IP Asignada ");
    Serial.println(WiFi.localIP());
  
// Servidor NTP / Setup Tiempo Online / Requiere una Conexión a WiFi

    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
    if (sincronizarHoraNTP()) {
      horaConfigurada = true;
      Serial.println("Configuración Horaria con NTP Éxitosa");
    } else {
      configurarHoraManual(); // Fallback
    }
  } else {
    Serial.println("\n Modo Offline.");
    configurarHoraManual();
  }

  mostrarHoraActual();
  delay(2000); // Evitación de Sobrecarga
}

void loop() {
  unsigned long currentMillis = millis();

  // Control LED
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    controlLED();
  }

  if (errorState) return;
  if (!horaConfigurada) {
    errorState = true;
    return;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error de Obtención Hora Local");
    errorState = true;
    delay(1000);
    return;
  }

  if (esDiaDeRiego(timeinfo.tm_wday)) {
    bool enHorarioRiego = false;

    for (int i = 0; i < numHorarios; i++) {
      if (estaEnHorario(timeinfo, horarios[i])) {
        enHorarioRiego = true;
        break;
      }
    }

    if (enHorarioRiego) {
      activarBombas();
      Serial.println("Bombas Activas / En Trabajo");
    } else {
      desactivarBombas();
      Serial.println("Bombas Inactivas / Fuera de Trabajo");
    }
  } else {
    desactivarBombas();
    Serial.println("Bombas Desactivadas / No Disponible");
  }

  delay(100);
}

void controlLED() {
  if (errorState) {
    digitalWrite(ledAzul, HIGH);
    return;
  }

  if (!horaConfigurada) {
    digitalWrite(ledAzul, HIGH);
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

  if (enHorarioRiego) patronRiego();
  else if (esDiaValido) patronEspera();
  else {
    digitalWrite(ledAzul, LOW);
    patternStep = 0;
  }
}

void patronRiego() {
  int ciclo = patternStep % 9;
  digitalWrite(ledAzul, (ciclo < 3 || ciclo >= 6) ? HIGH : LOW);
  if (++patternStep >= 100) patternStep = 0;
}

void patronEspera() {
  digitalWrite(ledAzul, (patternStep % 2 == 0) ? HIGH : LOW);
  if (++patternStep >= 600) patternStep = 0;
}

bool esDiaDeRiego(int diaSemana) {
  for (int i = 0; i < numDiasRiego; i++) if (diasRiego[i] == diaSemana) return true;
  return false;
}

bool estaEnHorario(struct tm timeinfo, HorarioRiego horario) {
  int actual = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int inicio = horario.horaInicio * 60 + horario.minutoInicio;
  int fin = horario.horaFin * 60 + horario.minutoFin;
  return (actual >= inicio && actual < fin);
}

// TODO: Integración de Motor de Autoguardado de Estados

void activarBombas() {
  // Zonas Principales
  digitalWrite(Z1, HIGH); digitalWrite(Z2, HIGH); digitalWrite(Z3, HIGH);
  // Espejos
  digitalWrite(Z1_mirror, HIGH); digitalWrite(Z2_mirror, HIGH); digitalWrite(Z3_mirror, HIGH);
  // Relés
  digitalWrite(R1, HIGH); digitalWrite(R2, HIGH); digitalWrite(R3, HIGH);
}

// TODO: Integración de Motor de Autoguardado de Estados

void desactivarBombas() {
  // Zonas Principales
  digitalWrite(Z1, LOW); digitalWrite(Z2, LOW); digitalWrite(Z3, LOW);
  // Espejos
  digitalWrite(Z1_mirror, LOW); digitalWrite(Z2_mirror, LOW); digitalWrite(Z3_mirror, LOW);
  // Relés
  digitalWrite(R1, LOW); digitalWrite(R2, LOW); digitalWrite(R3, LOW);
}

void mostrarHoraActual() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Error de Obtención de Hora");
    return;
  }

  // TODO: Integración de Motor de Autoguardado de Estados

  char buffer[50];
  strftime(buffer, sizeof(buffer), "Hora Actual: %A %Y-%m-%d %H:%M:%S", &timeinfo);
  Serial.println(buffer);
  Serial.print("Día de Semana ");
  Serial.println(timeinfo.tm_wday);
  Serial.print("Día de Riego ");
  Serial.println(esDiaDeRiego(timeinfo.tm_wday) ? "Sí" : "No");
}

bool sincronizarHoraNTP() {
  struct tm timeinfo;
  for (int i = 0; i < 10; i++) {
    if (getLocalTime(&timeinfo)) return true;
    delay(500);
  }
  return false;
}

void configurarHoraManual() {
  Serial.println("Configurando Hora Manual (Offline)");
  setenv("TZ", "COT-5", 1);
  tzset();

// Setup Tiempo
// Configurese Adecuadamente / Formato Reloj 24Hrs / 1900 + AAA
// TODO: Integración de Motor de Autoguardado de Estados

  struct tm timeinfo;      // Estructura de Tiempo / 10 de Noviembre del 2025 / 06:00 / Default / Configurese
  timeinfo.tm_year = 125;  // Año / 1900 + 125 = 2025
  timeinfo.tm_mon = 10;    // Mes / Noviembre / 0 o Enero / 11 o Diciembre
  timeinfo.tm_mday = 10;   // Día
  timeinfo.tm_hour = 6;   // Hora
  timeinfo.tm_min = 0;     // Minutos
  timeinfo.tm_sec = 0;     // Segundos

  time_t t = mktime(&timeinfo);
  struct timeval now = {.tv_sec = t};
  settimeofday(&now, NULL);

  horaConfigurada = true;
  Serial.println("Hora Local Offline Configurada");
}