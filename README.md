# Pingwin

<img width="614" height="652" alt="image" src="https://github.com/user-attachments/assets/fe455a31-e842-48e8-adee-4df062d27655" /> 

[![C++20](https://img.shields.io/badge/Language-C%2B%2B20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)](https://www.microsoft.com/windows)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Build Status](https://img.shields.io/badge/Build-Stable-brightgreen.svg)]()

Pingwin é um utilitário leve desenvolvido em C++ para monitoramento de rede em tempo real diretamente da bandeja do sistema (System Tray). Ideal para gamers, SOC, NOC, administradores de rede e usuários que precisam de diagnóstico constante de latência sem janelas intrusivas.

## Funcionalidades
<img width="216" height="40" alt="image" src="https://github.com/user-attachments/assets/1be98995-2df9-48b9-8986-b23ed3d3e067" />

- **Monitoramento no Tray**: Veja sua latência atual (ms) diretamente no ícone da barra de tarefas.
- **Cores Dinâmicas**: O ícone muda de cor (Verde, Amarelo, Laranja, Vermelho) baseado em limites de latência customizáveis.
- **Dashboard Completo**: Visualize estatísticas detalhadas, incluindo Jitter, Desvio Padrão, Taxa de Sucesso e Gráfico de Histórico.
- **Alertas Inteligentes**: Notificações Toast e alertas sonoros quando a latência ultrapassa o limite definido.
- **Ferramentas de Rede**:
  - **Port Scan**: Escaneie portas ativas localmente ou em alvos remotos.
  - **Traceroute Automático**: Disparado ao detectar alta latência persistente.
  - **Informações de Interface**: Acesso rápido a IP Local, Gateway, DNS e Endereço MAC.
- **Janelas Flutuantes**: Monitore múltiplos alvos simultaneamente com janelas compactas e translúcidas.

## Cenários de Uso

- **Jogos Online**: Validação de estabilidade de rota para servidores de jogos antes e durante sessões.
- **Teletrabalho (Home Office)**: Diagnóstico de qualidade em chamadas VoIP e videoconferências via análise de Jitter.
- **Administração de Sistemas**: Validação rápida de conectividade de serviços e disponibilidade de portas em servidores.
- **NOC**: Provedores para analise interna de rotas em seu backbone.

## Tecnologias

- **Linguagem**: C++20
- **Interface**: Win32 API (GDI para renderização de alto desempenho)
- **Rede**: WinSock2, IP Helper API, ICMP API
- **Arquitetura**: Multithreaded (Assíncrona) com sincronização por mutexes e atômicos.

## Como Compilar

Certifique-se de ter o g++ (MinGW-w64) instalado e configurado no seu PATH.

1. Clone o repositório:

   ```bash
   git clone https://github.com/kfn-d0/Pingwin.git
   ```

2. Compile os recursos e o executável:

   ```bash
   windres resource.rc -o resource.o
   g++ -O3 -std=c++20 -o Pingwin.exe main.cpp gui_manager.cpp network_service.cpp ui_windows.cpp resource.o -lws2_32 -lwinhttp -liphlpapi -lwlanapi -lcomctl32 -lgdi32 -luser32 -lshell32 -mwindows
   ```

## Configuracao

Acesse o menu de contexto clicando com o botão direito no ícone do tray para:

- Mudar o alvo do ping (Google, Cloudflare ou IP customizado).
- Alterar o tipo de ping (ICMP, TCP ou UDP).
- Ajustar os limites de cores e gatilhos de notificação.

---
