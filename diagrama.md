# 🧠📡 MVP - Monitoramento de Sinais Vitais (Offline + Alertas)

```mermaid
flowchart TD

    A[Paciente] --> B[Sensores]

    B --> B1[Batimento + SpO2<br/>PPG]
    B --> B2[Temperatura]
    B --> B3[Acelerômetro<br/>(queda)]

    B1 --> C[Microcontrolador<br/>ESP32]
    B2 --> C
    B3 --> C

    C --> D[Processamento Local<br/>Edge Computing]

    D --> E{Análise de Dados}

    E -->|Normal| F[Armazenamento Local<br/>Buffer]
    E -->|Anomalia| G[Alerta Imediato]

    F --> H[Envio Periódico<br/>(ex: 1h)]
    G --> I[Envio Prioritário]

    H --> J[Modulo de Comunicação]
    I --> J

    J --> K{Tecnologia}

    K -->|LoRaWAN| L[Gateway LoRa]
    K -->|NB-IoT| M[Torre Celular]
    K -->|Satélite| N[Satélite]

    L --> O[Internet]
    M --> O
    N --> O

    O --> P[Backend / API]

    P --> Q[Banco de Dados]
    P --> R[Motor de Alertas]

    R --> S[Dashboard Médico]
    Q --> S

    S --> T[Médico / Hospital]
