# Pingwin

**A taskbar latency monitor for Windows.**

Pingwin é um utilitário leve desenvolvido em C++ para monitoramento de rede em tempo real diretamente da bandeja do sistema (System Tray). Ideal para gamers, administradores de rede e usuários que precisam de diagnóstico constante de latência sem janelas intrusivas.

## Funcionalidades

- **Monitoramento no Tray**: Veja sua latência atual (ms) diretamente no ícone da barra de tarefas.
- **Cores Dinâmicas**: O ícone muda de cor (Verde, Amarelo, Laranja, Vermelho) baseado em limites de latência customizáveis.
- **Dashboard Completo**: Visualize estatísticas detalhadas, incluindo Jitter, Desvio Padrão, Taxa de Sucesso e Gráfico de Histórico.
- **Alertas Inteligentes**: Notificações Toast e alertas sonoros quando a latência ultrapassa o limite definido.
- **Ferramentas de Rede**:
  - **Port Scan**: Escaneie portas ativas localmente ou em alvos remotos.
  - **Traceroute Automático**: Disparado ao detectar alta latência persistente.
  - **Informações de Interface**: Acesso rápido a IP Local, Gateway, DNS e Endereço MAC.
- **Janelas Flutuantes**: Monitore múltiplos alvos simultaneamente com janelas compactas e translúcidas.

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
