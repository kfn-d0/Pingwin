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

O **Pingwin** é muito mais do que um simples "ping". Pela forma como foi construído (nativo, multithreaded e focado em diagnóstico), ele se encaixa perfeitamente nestes cenários:

### 1. Monitoramento de Jitter (Qualidade de Voz e Vídeo)
O Jitter (variação da latência) é o maior inimigo de chamadas VoIP (Teams, Zoom, Discord).
- **Cenário:** Durante uma videoconferência, você percebe falhas no áudio.
- **Uso:** O Pingwin calcula o Jitter em tempo real no dashboard. Isso permite identificar se o problema é o Wi-Fi instável ou congestionamento na rede, algo que um ping comum não mostra com facilidade.

### 2. Detecção de Instabilidade e Perda de Pacotes (Gaming)
Para gamers, um ping baixo que "pula" para 300ms (spike) é pior que um ping alto constante.
- **Cenário:** Você está jogando e sente "lag spikes".
- **Uso:** As cores dinâmicas no Tray avisam visualmente no exato momento do spike. O histórico gráfico permite ver se as quedas de performance são cíclicas ou aleatórias.

### 3. Observabilidade em Tempo Real (NOC e SOC)
Profissionais de monitoramento precisam manter o foco em dashboards principais.
- **Cenário:** Monitorar o gateway principal ou um servidor crítico sem ocupar uma das telas com um terminal preto.
- **Uso:** O ícone compacto na bandeja permite "observabilidade passiva". Você só olha para ele quando a cor muda de verde para laranja/vermelho, economizando carga cognitiva.

### 4. Diagnóstico de Mudanças de Rota (Backbone e ISP)
Mudanças súbitas de latência geralmente indicam que o tráfego mudou de rota (peering).
- **Cenário:** A latência para um servidor internacional subiu de 120ms para 180ms.
- **Uso:** O **Traceroute Automático** entra em ação. Se o Pingwin detecta alta latência persistente, ele dispara o traceroute para que você tenha o log exato de qual nó da rede (hop) causou o aumento antes que a rota volte ao normal.

### 5. Teste de Portas e Disponibilidade de Serviços
Às vezes o servidor responde ao ICMP (Ping), mas o serviço (Web, DB, SSH) está fora.
- **Cenário:** Validar se um firewall liberou o acesso a uma porta específica em um servidor remoto.
- **Uso:** As ferramentas de **Port Scan** integradas permitem testar rapidamente se portas TCP/UDP estão abertas e respondendo, sem precisar instalar o Nmap ou ferramentas pesadas no cliente.

### 6. Validação de Conectividade em Home Office
Diagnóstico rápido para usuários não técnicos ou suporte remoto.
- **Cenário:** Um funcionário reclama que "a internet está lenta".
- **Uso:** O Pingwin fornece informações de interface (IP, Gateway, DNS, MAC) e estatísticas de sucesso/falha de forma visual. É muito mais fácil pedir para o usuário ler o que está no "dashboard azul" do que rodar comandos no terminal.

### 7. Monitoramento de Múltiplos Alvos (Janelas Flutuantes)
Útil quando você está migrando um serviço ou acompanhando a estabilidade de vários pontos ao mesmo tempo.
- **Cenário:** Você está fazendo um deploy e quer monitorar o servidor novo, o antigo e o banco de dados.
- **Uso:** As janelas flutuantes translúcidas permitem que você "cole" o monitoramento de cada host em um canto da tela enquanto trabalha no código ou no terminal.

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
   g++ -O3 -std=c++20 -static -static-libgcc -static-libstdc++ -o Pingwin.exe main.cpp gui_manager.cpp network_service.cpp ui_windows.cpp resource.o -lws2_32 -lwinhttp -liphlpapi -lwlanapi -lcomctl32 -lgdi32 -luser32 -lshell32 -mwindows
   ```

## Configuracao

Acesse o menu de contexto clicando com o botão direito no ícone do tray para:

- Mudar o alvo do ping (Google, Cloudflare ou IP customizado).
- Alterar o tipo de ping (ICMP, TCP ou UDP).
- Ajustar os limites de cores e gatilhos de notificação.

---
