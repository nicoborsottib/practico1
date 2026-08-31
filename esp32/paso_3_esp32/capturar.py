#!/usr/bin/env python3
"""Resetea el ESP32, captura la corrida completa y separa log crudo de CSV limpio."""
import serial, time, sys, re

PUERTO, BAUD, LIMITE_S = '/dev/ttyUSB0', 115200, 500

s = serial.Serial(PUERTO, BAUD, timeout=1)
s.setDTR(False); s.setRTS(True); time.sleep(.15); s.setRTS(False)
s.reset_input_buffer()

crudo, filas = [], []
# el encabezado CSV y las filas de datos: freq,tipo,iters,tiempo,Miter_s
fila_re = re.compile(r'^\d+,(int|float|double),\d+,[\d.]+,[\d.]+$')

t0 = time.time()
while time.time() - t0 < LIMITE_S:
    linea = s.readline().decode('ascii', 'replace').rstrip()
    if not linea:
        continue
    crudo.append(linea)
    if fila_re.match(linea):
        filas.append(linea)
        print(f"  {linea}", flush=True)
    elif linea.startswith('#'):
        print(linea, flush=True)
    if linea.startswith('# fin'):
        print(f"\n>>> terminó solo a los {time.time()-t0:.0f}s", flush=True)
        break
else:
    print(f"\n>>> corte por limite de {LIMITE_S}s", flush=True)
s.close()

with open('captura_cruda.log', 'w') as f:
    f.write('\n'.join(crudo) + '\n')
with open('resultados.csv', 'w') as f:
    f.write('freq_mhz,tipo,iteraciones,tiempo_s,Miter_s\n')
    f.write('\n'.join(filas) + '\n')
print(f"\n{len(filas)} filas -> resultados.csv | {len(crudo)} lineas -> captura_cruda.log")
