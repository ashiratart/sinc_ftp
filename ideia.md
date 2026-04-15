
---

# 🧠📡 1. VISÃO DO MVP

### 🎯 Objetivo

Dispositivo portátil de monitoramento de sinais vitais que:

* Funciona **offline (sem Wi-Fi)**
* Processa dados localmente (edge)
* Envia:

  * 📊 **Resumo periódico (ex: 1h)**
  * 🚨 **Alertas imediatos (quando necessário)**

---

# ⚙️ 2. ARQUITETURA DO SISTEMA

```
[ Sensores ] 
      ↓
[ Microcontrolador (Edge AI) ]
      ↓
[ Comunicação (LoRa / NB-IoT) ]
      ↓
[ Gateway / Torre / Satélite ]
      ↓
[ Backend (API + Banco) ]
      ↓
[ Dashboard Médico ]
```

---

# 🔬 3. BASE TECNOLÓGICA (REFERÊNCIAS)

### 📚 Conceitos usados:

* Photoplethysmography → batimento/oxigênio
* Edge Computing → processamento local
* LoRaWAN → comunicação longa distância
* NB-IoT → IoT via celular
* HL7 FHIR → integração hospitalar

---

### 🏥 Instituições (pra embasar discussão)

* World Health Organization → telemedicina remota
* IEEE → IoT médico
* National Institutes of Health → monitoramento contínuo

---

# 🧩 4. COMPONENTES DO MVP (HARDWARE)

## 🧠 1. Microcontrolador

### 🔹 ESP32

* Wi-Fi (opcional) + BLE
* baixo custo
* suficiente para MVP

💰 **R$ 40 – R$ 80**

---

## ❤️ 2. Sensor cardíaco + oxigênio

### 🔹 MAX30102

* mede:

  * batimento
  * SpO2

💰 **R$ 20 – R$ 50**

---

## 🌡️ 3. Sensor de temperatura

### 🔹 DS18B20

💰 **R$ 10 – R$ 25**

---

## 🧍 4. Acelerômetro (queda)

### 🔹 MPU6050

💰 **R$ 15 – R$ 40**

---

## 📡 5. Comunicação (escolha 1)

### 🔸 Opção A — LoRa

#### SX1276 LoRa Module

💰 **R$ 60 – R$ 120**

✔️ ideal pra:

* área rural
* baixo consumo

---

### 🔸 Opção B — NB-IoT

#### SIM7000

💰 **R$ 150 – R$ 300**

✔️ melhor pra:

* integração com operadora
* confiabilidade

---

## 🔋 6. Bateria

### 🔹 Li-Po 2000–5000mAh

💰 **R$ 30 – R$ 80**

---

## 💵 💡 CUSTO TOTAL (MVP)

| Configuração | Custo aproximado |
| ------------ | ---------------- |
| LoRa         | R$ 200 – R$ 350  |
| NB-IoT       | R$ 300 – R$ 550  |

---

# 🧠 5. LÓGICA DO SOFTWARE (EDGE)

## 📊 Coleta contínua

* leitura a cada X segundos

---

## 🧠 Processamento local

### Regras simples:

```pseudo
if SpO2 < 90 → ALERTA
if FC > 140 → ALERTA
if queda detectada → ALERTA
```

---

## 📦 Envio periódico

* média por hora
* min/max
* eventos

---

## 🚨 Envio imediato

* pacote pequeno
* prioridade alta

---

# ☁️ 6. BACKEND (SIMPLES)

## 🔹 Stack sugerida

* API: Node.js / Python
* Banco: PostgreSQL
* Dashboard: web simples

---

## 🔹 Integração futura

* padrão HL7 FHIR

---

# 🚧 7. PRINCIPAIS PONTOS NEGATIVOS

## ❌ 1. Precisão médica

* sensores baratos ≠ hospital
* risco de falso positivo/negativo

---

## ❌ 2. Certificação

* precisa passar por:

  * ANVISA

👉 sem isso:

* só pode ser protótipo / estudo

---

## ❌ 3. Conectividade

* LoRa depende de gateway
* NB-IoT depende de cobertura

---

## ❌ 4. Energia

* transmissão consome muito
* precisa otimização forte

---

## ❌ 5. Latência

* especialmente com:

  * Satellite Communication

---

## ❌ 6. Segurança

* dados médicos = sensíveis
* precisa criptografia (TLS, etc.)

---

# 🚀 8. DIFERENCIAL DO PROJETO

👉 O valor não está só no hardware, mas em:

* detecção inteligente local
* funcionamento offline
* baixo custo
* aplicável em áreas remotas

---

# 🧠 CONCLUSÃO (pra discussão)

✔️ Projeto **tecnicamente viável hoje**
✔️ Custo relativamente baixo para MVP
✔️ Baseado em tecnologias consolidadas

❗ Desafios reais:

* precisão clínica
* certificação
* confiabilidade em campo

---