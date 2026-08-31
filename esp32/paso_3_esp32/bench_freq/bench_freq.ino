/*
 * TP Sistemas de Computación - Punto 3
 * ESP32: efecto de la frecuencia de CPU sobre el tiempo de ejecución.
 *
 * Tres cargas de trabajo idénticas en estructura, distinto tipo de dato:
 *   - int32_t  : ALU entera
 *   - float    : FPU por hardware (el ESP32 clásico tiene FPU de simple precisión)
 *   - double   : NO hay FPU de doble precisión -> lo emula el compilador por software
 *
 * Se calibra la cantidad de iteraciones para que cada prueba dure ~10 s a la
 * frecuencia máxima, y después se repite exactamente el mismo trabajo a
 * frecuencias menores. La salida es CSV para graficar después.
 */

#include <Arduino.h>
#include "esp_task_wdt.h"

// Puntero a función de benchmark. Tiene que estar acá arriba: el preprocesador
// de Arduino inserta los prototipos automáticos justo después de los #include,
// y si el typedef estuviera más abajo esos prototipos no lo conocerían.
typedef uint64_t (*bench_fn)(uint64_t);

// ---------------------------------------------------------------------------
// Configuración
// ---------------------------------------------------------------------------

// Frecuencias a barrer, de mayor a menor (MHz).
// 240 / 160 / 80 vienen del PLL. 40 / 20 / 10 vienen del cristal (XTAL/n).
// Si a 40 MHz el monitor serie muestra basura, borrá el 40 de esta lista.
static const uint32_t FREQS[]  = {240, 160, 80, 40};
static const size_t   N_FREQS  = sizeof(FREQS) / sizeof(FREQS[0]);

// Duración objetivo de cada prueba a la frecuencia máxima, en segundos.
static const double TARGET_S = 10.0;

// ---------------------------------------------------------------------------
// Variables del benchmark
//
// El "paso" es volatile a propósito: obliga al compilador a releerlo de memoria
// en cada vuelta, así no puede reemplazar el bucle por una fórmula cerrada
// (suma de Gauss) ni borrarlo por ser código muerto. El costo extra es el mismo
// en las tres pruebas, por lo que la comparación entre tipos sigue siendo justa.
// ---------------------------------------------------------------------------
volatile int32_t step_i = 1;
volatile float   step_f = 1.0f;
volatile double  step_d = 1.0;

// Sumideros: el resultado se usa, entonces el bucle no se optimiza afuera.
volatile int32_t sink_i;
volatile float   sink_f;
volatile double  sink_d;

// Cada bench devuelve el tiempo en microsegundos.
// esp_timer_get_time() cuenta a 1 MHz con un timer independiente de la CPU,
// así que sigue midiendo bien aunque cambiemos la frecuencia del procesador.

uint64_t bench_int(uint64_t n) {
  int32_t acc = 0;
  int64_t t0 = esp_timer_get_time();
  for (uint64_t k = 0; k < n; k++) acc += step_i;
  int64_t t1 = esp_timer_get_time();
  sink_i = acc;
  return (uint64_t)(t1 - t0);
}

uint64_t bench_float(uint64_t n) {
  float acc = 0.0f;
  int64_t t0 = esp_timer_get_time();
  for (uint64_t k = 0; k < n; k++) acc += step_f;
  int64_t t1 = esp_timer_get_time();
  sink_f = acc;
  return (uint64_t)(t1 - t0);
}

uint64_t bench_double(uint64_t n) {
  double acc = 0.0;
  int64_t t0 = esp_timer_get_time();
  for (uint64_t k = 0; k < n; k++) acc += step_d;
  int64_t t1 = esp_timer_get_time();
  sink_d = acc;
  return (uint64_t)(t1 - t0);
}

// ---------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------

// Calibra cuántas iteraciones hacen falta para durar TARGET_S segundos.
uint64_t calibrar(bench_fn f, const char *nombre) {
  const uint64_t n0 = 1000000ULL;          // muestra corta
  uint64_t us = f(n0);
  if (us == 0) us = 1;
  uint64_t n = (uint64_t)((double)n0 * (TARGET_S * 1e6) / (double)us);
  Serial.printf("# calibracion %-6s : %llu iter en %llu us -> uso N = %llu\n",
                nombre, n0, us, n);
  return n;
}

// Cambia la frecuencia de CPU y reajusta el UART.
// Ojo: por debajo de 80 MHz el bus APB pasa a seguir a la CPU, y el UART
// cuelga del APB -> hay que recalcular el divisor del baudrate.
bool cambiar_frecuencia(uint32_t mhz) {
  Serial.flush();
  bool ok = setCpuFrequencyMhz(mhz);
  Serial.updateBaudRate(115200);
  delay(50);
  return ok;
}

void correr_una(const char *tipo, bench_fn f, uint64_t n, uint32_t mhz) {
  esp_task_wdt_reset();                     // por las dudas, si el WDT del loop está activo
  uint64_t us = f(n);
  esp_task_wdt_reset();
  double seg    = us / 1e6;
  double miters = (double)n / (double)us;   // millones de iteraciones por segundo
  // CSV: freq_mhz,tipo,iteraciones,tiempo_s,Miter_s
  Serial.printf("%lu,%s,%llu,%.3f,%.2f\n", (unsigned long)mhz, tipo, n, seg, miters);
  Serial.flush();
  delay(200);
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(2000);                              // tiempo para abrir el monitor serie

  Serial.println();
  Serial.println("# ==== ESP32: tiempo de ejecucion vs frecuencia de CPU ====");
  Serial.printf("# chip           : %s, %d core(s), rev %d\n",
                ESP.getChipModel(), ESP.getChipCores(), ESP.getChipRevision());
  Serial.printf("# frec. XTAL     : %lu MHz\n", (unsigned long)getXtalFrequencyMhz());
  Serial.printf("# frec. inicial  : %lu MHz\n", (unsigned long)getCpuFrequencyMhz());
  Serial.printf("# sizeof: int=%d float=%d double=%d bytes\n",
                (int)sizeof(int), (int)sizeof(float), (int)sizeof(double));

  // Calibramos siempre a la frecuencia mas alta de la lista.
  cambiar_frecuencia(FREQS[0]);
  Serial.printf("# calibrando a %lu MHz (objetivo %.0f s por prueba)...\n",
                (unsigned long)FREQS[0], TARGET_S);

  uint64_t N_int    = calibrar(bench_int,    "int");
  uint64_t N_float  = calibrar(bench_float,  "float");
  uint64_t N_double = calibrar(bench_double, "double");

  Serial.println("#");
  Serial.println("freq_mhz,tipo,iteraciones,tiempo_s,Miter_s");

  for (size_t i = 0; i < N_FREQS; i++) {
    uint32_t f = FREQS[i];
    if (!cambiar_frecuencia(f)) {
      Serial.printf("# NO se pudo fijar %lu MHz, la salteo\n", (unsigned long)f);
      continue;
    }
    uint32_t real = getCpuFrequencyMhz();
    if (real != f) {
      Serial.printf("# pedi %lu MHz y quedo en %lu MHz\n",
                    (unsigned long)f, (unsigned long)real);
    }
    correr_una("int",    bench_int,    N_int,    real);
    correr_una("float",  bench_float,  N_float,  real);
    correr_una("double", bench_double, N_double, real);
  }

  cambiar_frecuencia(FREQS[0]);
  Serial.println("# fin");
}

void loop() {
  delay(1000);
}
