# Projeto de Rastreamento de Pátio Mottu (IoT, Java & Oracle)

Este projeto demonstra uma solução de ponta-a-ponta para o rastreamento de motocicletas em um pátio, utilizando uma arquitetura distribuída que integra IoT (Wokwi), um middleware serverless (Node.js) e um backend robusto (Java/Spring Boot) conectado a um banco de dados Oracle com lógica de negócios embarcada (PL/SQL).

## 🚀 Visão Geral da Arquitetura (Fluxo de Dados)

O fluxo de dados é projetado para ser assíncrono e resiliente, garantindo que o sensor de IoT não precise conhecer a lógica complexa do backend.

1.  **Captura (Wokwi):** Um simulador de ESP32 (Wokwi) age como um leitor de RFID. Ele lê um `ID` de moto e a `vaga` (sensor) onde ela foi vista.
2.  **Ingestão (Firebase):** O Wokwi envia esses dados brutos (ex: `{id: "RFID001", vaga: "vaga_1", status: "entrada"}`) para o Firebase Realtime Database (RTDB), que atua como uma "fila" de eventos.
3.  **Ponte/Middleware (Node.js):** Um script local (`bridge.js`) usando o Firebase Admin SDK "ouve" o Firebase em tempo real.
4.  **Tradução:** Assim que um novo dado chega, a "ponte" o "limpa" (ex: remove espaços do RFID) e o traduz para o formato que a API Java espera.
5.  **API de Negócio (Java):** A ponte faz uma chamada `POST` autenticada (usando `X-API-KEY`) para o backend Spring Boot no endpoint `/api/movimentacoes`.
6.  **Banco de Dados (Oracle):** O Java **não** escreve diretamente no banco. Ele chama a procedure `PKG_MOVIMENTACAO.sp_registrar_movimentacao`, que valida (Moto existe? Sensor existe? Moto está 'Ativa'?) e insere o dado de forma transacional na tabela `MOVIMENTACAO`.

---

## 🛠️ Componentes da Solução

### 1. IoT (Wokwi - Simulador ESP32)

- **Link do Projeto:** [**Simulador Wokwi ESP32**](https://wokwi.com/projects/431699886903605249)
- **Arquivo Principal:** `wokwi_esp32.ino`
- **Função:** Simular o hardware de um pátio. Ele valida se um RFID digitado (ex: `RFID001`) está em sua "lista de permissão" (`MOTO_IDS[]`).
- **Ação:** Se o RFID é válido, ele determina a próxima vaga livre (ex: `vaga_1`) e envia os dados brutos (`id`, `vaga`, `status`) para o Firebase RTDB. Ele **não** conhece o Java nem o Oracle.

### 2. Middleware (Ponte Node.js - `local-bridge-script/`)

- **Arquivo Principal:** `bridge.js`
- **Função:** Atuar como um "tradutor" e "mensageiro" desacoplado. Ele é o único que sabe falar tanto com o Firebase quanto com o Java.
- **Ação:**
  1.  Usa `firebase-admin` para se conectar ao Firebase RTDB (usando a chave `serviceAccountKey.json`).
  2.  Ouve por novos eventos em `/movimento` usando `ref.on('child_added', ...)`.
  3.  Recebe o JSON do Wokwi, limpa o `id` (ex: `"12 34 56 78"` -> `"12345678"`).
  4.  Usa `axios` para fazer um `POST` para `http://localhost:8080/api/movimentacoes`.
  5.  Passa a `X-API-KEY` (do `application.properties`) no header para autenticar no backend Java.

### 3. Backend (Java/Spring Boot - `motolocation-java/`)

- **Arquivos Principais:** `MovimentacaoApiController.java`, `MovimentacaoService.java`, `SecurityConfig.java`
- **Função:** O cérebro da lógica de negócio e o proprietário dos dados.
- **Ação:**
  1.  `SecurityConfig.java`: Protege o endpoint `/api/movimentacoes` e exige um header `X-API-KEY` válido (validado pelo `ApiKeyAuthFilter`).
  2.  `MovimentacaoApiController.java`: Recebe o `POST` da "ponte" Node.js.
  3.  `MovimentacaoService.java`: **Não usa JpaRepository para escrever**. Em vez disso, usa o `EntityManager` para chamar a procedure do Oracle:
      ```java
      StoredProcedureQuery query = em.createStoredProcedureQuery("PKG_MOVIMENTACAO.sp_registrar_movimentacao");
      query.setParameter("p_rfid_tag", rfid);
      query.setParameter("p_sensor_codigo", sensorCodigo);
      query.execute();
      ```
  4.  **Banco (Oracle):** A procedure `sp_registrar_movimentacao` (escrita em PL/SQL) faz as validações de negócio (`Moto não encontrada`, `Sensor não encontrado`, `Moto não está Ativa`) e, se tudo estiver OK, insere na tabela `MOVIMENTACAO`.

---

## 🚀 Como Executar o Fluxo Completo (Ponta-a-Ponta)

Para demonstrar a integração, você precisará de 3 terminais e o Wokwi rodando simultaneamente.

### Pré-requisitos

- Java (IntelliJ) e a aplicação Spring Boot (`motolocation-java`) prontos.
- Banco de Dados Oracle no ar, com o script `.sql` (tabelas, pacotes, procedures) já executado.
- A pasta `local-bridge-script` com o `bridge.js` e o arquivo `.json` da Service Account.
- `npm install` rodado dentro de `local-bridge-script`.
- O [Simulador Wokwi](https://wokwi.com/projects/431699886903605249) aberto no navegador.

### Passo 1: Iniciar o Backend (Terminal 1)

1.  Abra o projeto `motolocation-java` no IntelliJ.
2.  Confirme que o `application.properties` está com a URL, usuário, senha, `driver-class-name` e `maximum-pool-size` corretos.
3.  Clique em "Run" (Play).
4.  **Aguarde** o log: `... Started MotolocationApplication in X.XXX seconds`

### Passo 2: Iniciar a "Ponte" (Terminal 2)

1.  Abra um novo terminal (PowerShell/CMD).
2.  Navegue até a pasta `local-bridge-script`.
3.  Execute o script:
    ```bash
    node bridge.js
    ```
4.  **Aguarde** o log: `Aguardando dados do Wokwi...`

### Passo 3: Iniciar o IoT (Wokwi)

1.  Abra o [Simulador Wokwi](https://wokwi.com/projects/431699886903605249) no navegador.
2.  **Confirme** que o Wokwi (`.ino`) tem os `MOTO_IDS` corretos (ex: `RFID001`, `RFID-HNIPORNE`) que existem na sua tabela `MOTO` no Oracle.
3.  **Confirme** que os sensores (ex: `vaga_1`, `vaga_2`) existem na sua tabela `SENSOR` no Oracle.
4.  Inicie a simulação (botão "Play").

### Passo 4: O Teste

1.  No terminal do Wokwi, digite um ID válido (ex: `RFID001`) e aperte Enter.
2.  **Observe o Terminal 2 (Ponte):** Você verá os logs:
    > `--- NOVO EVENTO RECEBIDO ---` > `Enviando para a API Java: { rfid: 'RFID001', sensorCodigo: 'vaga_1' }` > `SUCESSO! API Java respondeu com status: 200`
3.  **Observe o Terminal 1 (Java):** Você verá o log do Hibernate:
    > `Hibernate: {call PKG_MOVIMENTACAO.sp_registrar_movimentacao(?, ?, ?)}`
4.  **Observe o Oracle:** Se você rodar `SELECT * FROM MOVIMENTACAO`, o novo registro estará lá.
